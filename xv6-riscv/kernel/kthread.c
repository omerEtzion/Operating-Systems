#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

int debug_mode = 0;

void kthreadinit(struct proc *p)
{
  // printf("called kthreadinit\n");

  initlock(&p->ktid_lock, "nextktid");
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    initlock(&kt->lock, "kthread");
    kt->state = KT_UNUSED;
    kt->proc = p;

    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

struct kthread *mykthread()
{
  // printf("called mykthread\n");
  
  push_off();
  struct cpu *c = mycpu();
  // printf("push_off mykthread; noff = %d\n", c->noff);
  struct kthread *kt = c->kthread;
  pop_off();
  // printf("pop_off mykthread; noff = %d\n", c->noff);
  return kt;
}

int
allocktid(struct proc *p)
{
  // printf("called allocktid\n");
  
  // printf("acquire allocktid\n");
  if (get_debug_mode())
    printf("called acquire on lock %d 3\n", &p->ktid_lock);
  acquire(&p->ktid_lock);
  int ktid = p->nextktid;
  p->nextktid += 1;
  if (get_debug_mode()) 
    printf("called release on lock %d 3\n", &p->ktid_lock);
  release(&p->ktid_lock);

  return ktid;
}

// Look in the kthread table for an UNUSED kthread.
// If found, initialize state required to run in the kernel,
// and return with kt->lock held.
// If there are no free kthreads, return 0.
// p-.lock need to be locked before invoking this function
struct kthread*
allockthread(struct proc* p)
{
  // printf("called allockthread\n");
    
  struct kthread *kt;
  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    // printf("acquire allockthread\n");
    if (get_debug_mode())
      printf("called acquire on lock %d 4\n", &kt->lock);
    acquire(&kt->lock);
    if(kt->state == KT_UNUSED) {
      goto found;
    } else {
      if (get_debug_mode()) 
        printf("called release on lock %d 4\n", &kt->lock);
      release(&kt->lock);
    }
  }
  printf("no available kthread found\n");
  return 0;

found:
  kt->ktid = allocktid(p);
  kt->state = KT_USED;

  // Assign a trapframe page.
  kt->trapframe = get_kthread_trapframe(p, kt);

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&kt->context, 0, sizeof(kt->context));
  kt->context.ra = (uint64)forkret;
  kt->context.sp = kt->kstack + PGSIZE;

  return kt;
}

// free a kthread structure and the data hanging from it,
// including user pages.
// kt->lock must be held.
void
freekthread(struct kthread *kt)
{
  // printf("called freekthread\n");
  
  kt->state = KT_UNUSED;
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->ktid = 0;   
  kt->trapframe = 0;  
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  // printf("called get_kthread_trapframe\n");
  
  return p->base_trapframes + ((int)(kt - p->kthread));
}

int kthread_create(void *(*start_func)(), void *stack, uint stack_size) {
  printf("debugging: start of kthread create\n");
  int ktid;
  struct kthread *nkt;
  struct proc *p = myproc();
  struct kthread *kt = mykthread();

  printf("p->lock: %d\n", &p->lock);
  printf("kt->lock: %d\n", &kt->lock);

  printf("debugging: before allockthread()\n");
  if (get_debug_mode())
    printf("called acquire on lock %d 5\n", &p->lock);
  acquire(&p->lock); 
  // Allocate kthread.
  if((nkt = allockthread(p)) == 0){
    return -1;
  }
  printf("nkt->lock: %d\n", &nkt->lock);
  printf("nkt.proc->lock: %d\n", &nkt->proc->lock);  
  printf("debugging: after allockthread()\n");

  // copy saved user registers.
  *(nkt->trapframe) = *(kt->trapframe);

  // Cause kthread_create to return 0 in the child.
  nkt->trapframe->a0 = 0;

  // set nkt->kstack to the allocated kstack and the 'sp' register to the top of the kstack
  nkt->kstack = (uint64)stack;
  nkt->context.sp = (uint64)(stack + stack_size - 1);

  // set the start_func field of the thread's struct and set the 'ra' register to start_func_wrapper
  nkt->start_func = start_func;
  nkt->trapframe->epc = (uint64)start_func_wrapper;
  nkt->state = KT_RUNNABLE;

  ktid = nkt->ktid;

  if (get_debug_mode()) 
    printf("called release on lock %d 5\n", &nkt->lock);
  release(&nkt->lock);
  if (get_debug_mode()) 
    printf("called release on lock %d 6\n", &p->lock);
  release(&p->lock);

  printf("debugging: after releasing, ktid = %d\n", ktid);


  return ktid;
}

int kthread_kill(int ktid) {
  struct proc *p = myproc();
  struct kthread *kt;

  // printf("acquire kthread_kill 1\n");
  if (get_debug_mode())
    printf("called acquire on lock %d 6\n", &p->lock);
  acquire(&p->lock);
  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    // printf("acquire kthread_kill 2\n");
    if (get_debug_mode())
      printf("called acquire on lock %d 7\n", &kt->lock);
    acquire(&kt->lock);
    if(kt->ktid == ktid && kt->state != KT_UNUSED){
      // found the kthread to kill
      kt->killed = 1;
      if (kt->state == KT_SLEEPING) {
        kt->state = KT_RUNNABLE;
      }
      if (get_debug_mode()) 
        printf("called release on lock %d 7\n", &kt->lock);
      release(&kt->lock);
      return 0;
    }
    if (get_debug_mode()) 
      printf("called release on lock %d 8\n", &kt->lock);
    release(&kt->lock);
  }
  if (get_debug_mode()) 
    printf("called release on lock %d 9\n", &p->lock);
  release(&p->lock);
  return -1;
}

int
kthread_killed(struct kthread *kt)
{
  int k;
  
  // printf("acquire kthread_killed\n");
  if (get_debug_mode())
    printf("called acquire on lock %d 8\n", &kt->lock);
  acquire(&kt->lock);
  k = kt->killed;
  if (get_debug_mode()) 
    printf("called release on lock %d 10\n", &kt->lock);
  release(&kt->lock);
  return k;
}

void kthread_exit(int status) {
  struct proc *p = myproc();
  struct kthread* mykt = mykthread();

  // set the current kthread's state to KT_ZOMBIE and it's xstate
  // printf("acquire kthread_exit 1\n");
  if (get_debug_mode())
    printf("called acquire on lock %d 9\n", &mykt->lock);
  acquire(&mykt->lock);
  mykt->state = KT_ZOMBIE;
  mykt->xstate = status;
  if (get_debug_mode()) 
    printf("called release on lock %d 11\n", &mykt->lock);
  release(&mykt->lock);

  // wake up any kthread that were waiting to join mykt
  wakeup(mykt);

  // printf("acquire kthread_exit 2\n");
  if (get_debug_mode())
    printf("called acquire on lock %d 10\n", &p->lock);
  acquire(&p->lock);
  
  int should_terminate_proc = 1;
  struct kthread* kt;
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    // printf("acquire kthread_exit 3\n");
    if (get_debug_mode())
      printf("called acquire on lock %d 11\n", &kt->lock);
    acquire(&kt->lock);
    if (kt->state != KT_UNUSED && kt->state != KT_ZOMBIE) {
      // found a "living" kthread in the current proc, 
      // so it shouldn't be terminated
      should_terminate_proc = 0;
      break;
    }
    if (get_debug_mode()) 
      printf("called release on lock %d 12\n", &kt->lock);
    release(&kt->lock);
  }
  // if (get_debug_mode()) 
  //   printf("called release on lock %d 12.1\n", &p->lock);
  // release(&p->lock);

  if (should_terminate_proc) {
    // proc has no "living" kthreads and should b terminated
    if (get_debug_mode()) 
      printf("called release on lock %d 13\n", &p->lock);
    release(&p->lock);
    exit(status);
  }

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie kthread_exit");
}

int kthread_join(int ktid, int* status) {
  struct proc* p = myproc();
  struct kthread* kt_to_join = 0;

  // printf("acquire kthread_join 1\n");
  if (get_debug_mode())
    printf("called acquire on lock %d 12\n", &p->lock);
  acquire(&p->lock);
  struct kthread* kt;
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    // printf("acquire kthread_join 2\n");
    if (get_debug_mode())
    printf("called acquire on lock %d 14\n", &kt->lock);
    acquire(&kt->lock);
    if (kt->ktid == ktid && kt->state != KT_UNUSED) {
      kt_to_join = kt;
      break;
    }
    if (get_debug_mode()) 
      printf("called release on lock %d 14\n", &kt->lock);
    release(&kt->lock);
  }
  if (get_debug_mode()) 
    printf("called release on lock %d 15\n", &p->lock);
  release(&p->lock);

  if (kt_to_join == 0) {
    return -1;
  }

  if (kt_to_join->state == KT_ZOMBIE) {
    if (status != 0 && copyout(p->pagetable, (uint64)status, (char *)&kt_to_join->xstate,
                                  sizeof(kt_to_join->xstate)) < 0) {
      release(&kt_to_join->lock);
      return -1;
    }
  } else {
    // TODO: which lock should we sleep on?
    sleep(kt_to_join, &kt_to_join->lock); 
  }

  release(&kt_to_join->lock);
  return 0;
}

void start_func_wrapper() {
  // a wrapper function for start_func that calls kthread_exit after start_func finishes
  printf("started wrapper func\n");
  mykthread()->start_func();
  kthread_exit(0);
}

// // Returns the next KT_RUNNABLE kthread, or 0 if none were found
// struct kthread* find_next_kthread(){
//   struct kthread* curr_kt = mykthread();
//   struct kthread* next_kt = curr_kt + sizeof(struct kthread);
//   struct proc* p = myproc();

//   acquire(&p->lock);
//   while (next_kt != curr_kt) {
//     if (next_kt == &proc->kthread[NKT]) {
//       next_kt = proc->kthread;
//     }

//     acquire(&next_kt->lock);
//     if (next_kt->state == KT_RUNNABLE) {
//       release(&next_kt->lock);
//       break;
//     }
//     release(&next_kt->lock);
    
//     next_kt += sizeof(struct kthread);
//   }
//   release(&p->lock);

//   if (next_kt != curr_kt)
//     return next_kt;
//   else
//     return 0;
// }

void set_debug_mode(int i) {
  debug_mode = i;
}

int get_debug_mode() {
  return debug_mode;
}