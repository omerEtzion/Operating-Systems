#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

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

  int ktid;
  
  acquire(&p->ktid_lock);
  ktid = p->nextktid;
  p->nextktid += 1;
  release(&p->ktid_lock);

  return ktid;
}

// Look in the kthread table for an UNUSED kthread.
// If found, initialize state required to run in the kernel,
// and return with kt->lock held.
// If there are no free kthreads, return 0.
struct kthread*
allockthread(struct proc* p)
{
  // printf("called allockthread\n");
  
  struct kthread *kt;

  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    acquire(&kt->lock);
    if(kt->state == KT_UNUSED) {
      goto found;
    } else {
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
  int i, ktid;
  struct kthread *nkt;
  struct proc *p = myproc();
  struct kthread *kt = mykthread();

  // Allocate process.
  if((nkt = allockthread(p)) == 0){
    return -1;
  }

  // // Copy user memory from parent to child.
  // if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
  //   freekthread(nkt);
  //   release(&nkt->lock);
  //   return -1;
  // }
  // np->sz = p->sz;

  // copy saved user registers.
  *(nkt->trapframe) = *(kt->trapframe);

  // Cause kthread_create to return 0 in the child.
  nkt->trapframe->a0 = 0;

  // set nkt->kstack to the allocated kstack and the 'sp' register to the top of the kstack
  nkt->kstack = (uint64)stack;
  nkt->context.sp = stack + stack_size - 1;

  // set the start_func field of the thread's struct and set the 'ra' register to start_func_wrapper
  nkt->start_func = start_func;
  nkt->context.ra = (uint64)start_func_wrapper;

  ktid = nkt->ktid;

  release(&nkt->lock);

  acquire(&nkt->lock);
  nkt->state = KT_RUNNABLE;
  release(&nkt->lock);

  return ktid;
}

int kthread_kill(int ktid) {
  struct proc *p = myproc();
  struct kthread *kt;

  acquire(&p->lock);
  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->lock);
    if(kt->ktid == ktid && kt->state != KT_UNUSED){
      kt->killed = 1;
      struct kthread* kt;
      if (kt->state == KT_SLEEPING) {
        kt->state = KT_RUNNABLE;
      }
      release(&kt->lock);
      return 0;
    }
    release(&kt->lock);
  }
  release(&p->lock);
  return -1;
}

int
kthread_killed(struct kthread *kt)
{
  int k;
  
  acquire(&kt->lock);
  k = kt->killed;
  release(&kt->lock);
  return k;
}

void kthread_exit(int status) {
  // TODO: just copied from exit() in proc, still havent changed much


  struct proc *p = myproc();
  struct kthread* kt = mykthread();

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  struct kthread* kt;
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    acquire(&kt->lock);
    if (kt->state != KT_UNUSED)
      kt->state = KT_ZOMBIE;
    else {
      release(&kt->lock);
    }
  }

  p->xstate = status;
  p->state = P_ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  // printf("calling sched from exit; noff = %d\n", mycpu()->noff);
  sched();
  panic("zombie exit");
}

void start_func_wrapper() {
  // a wrapper function for start_func that calls kthread_exit after start_func finishes
  mykthread()->start_func();
  kthread_exit(0);
}

