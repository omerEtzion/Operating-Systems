#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl) {
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Set paging metadata to 0
  memset(&p->pg_m, 0, sizeof(p->pg_m));
  for(int i = 0; i < MAX_PSYC_PAGES; i++) {
    p->pg_m.memory_pgs[i].lapa_counter = 0xFFFFFFFF;
    p->pg_m.memory_pgs[i].vaddr = -1;
    p->pg_m.swapFile_pgs[i].lapa_counter = 0xFFFFFFFF;
    p->pg_m.swapFile_pgs[i].vaddr = -1;
  }

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  if(p->pid >= 2) {
    // Create the process' swapFile
    if(createSwapFile(p) == -1){
      freeproc(p);
      release(&p->lock);
      return 0;
    }
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  // Clear paging metadata
  memset(&p->pg_m, 0, sizeof(p->pg_m));
  // Remove the process' swapFile
  if(p->swapFile)
    removeSwapFile(p);
  p->swapFile = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }
  p->pg_m.num_of_pgs_in_memory += 1; // Don't add the page to the linked list because it's not swappable

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
  p->pg_m.num_of_pgs_in_memory += 1; // Don't add the page to the linked list because it's not swappable

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  printf("1\n");
  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc_wrapper(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc_wrapper(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }

  // copy the paging metadata 
  np->pg_m = p->pg_m; 

  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if(first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

// extern int SWAP_ALGO;
// extern int NONE;
// extern int NFUA;
// extern int LAPA;
// extern int SCFIFO;

void
choose_and_swap(uint64 v_addr_to_swap_in)
{
  
  struct proc* p = myproc();

  if(p->pid < 2 || SWAP_ALGO == NONE) {
    return;
  }
  
  uint64 v_addr_to_swap_out = choose_pg_to_swap();
  // pte_t* pte = walk(p->pagetable, to_swap_out, 0);

  swap_out(v_addr_to_swap_out, 1);

  swap_in(v_addr_to_swap_in, 1);
  
  
  
  // uint64 sz;
  // if((sz = uvmalloc(p->pagetable, p->sz, p->sz + PGSIZE)) == 0)
  //   panic("choose_and_swap: uvmalloc failed");
  // uint64 new_pg_v_addr = sz - PGSIZE;
  // uint64 new_pg_p_addr = walkaddr(p->pagetable, new_pg_v_addr);
  
  // int permissions = PTE_FLAGS(*walk(p->pagetable, v_addr_to_swap_in, 0));
  // permissions = (permissions & !PTE_PG) | PTE_V; // Turn off swapped flag and on valid flag


  // uvmunmap(p->pagetable, new_pg_v_addr, 1, 0); // unmmap new p_addr from new v_addr
  // mappages(p->pagetable, v_addr_to_swap_in, PGSIZE, new_pg_p_addr, permissions); // map new p_addr to old v_addr

  // int index_in_sf;
  // struct page* pgs_in_sf = p->pg_m.swapFile_pgs;
  // int found  = 0;
  // for(int i = 0; i < MAX_PSYC_PAGES; i++) {
  //   if(pgs_in_sf[i].vaddr == v_addr_to_swap_in) {
  //     index_in_sf = i;
  //     found = 1;
  //     break;
  //   }
  // }

  // if(!found) {
  //   panic("choose_and_swap: v_addr_to_swap_in not in swapFile");
  // }

  // readFromSwapFile(p, (char*)new_pg_p_addr, index_in_sf*PGSIZE, PGSIZE);

}

void
swap_out(uint64 v_addr, int to_swapFile)
{
  struct proc* p = myproc();

  if(p->pid < 2 || SWAP_ALGO == NONE) {
    return;
  }

  pte_t* pte = walk(p->pagetable, v_addr, 0);
  
  if(to_swapFile) {
    int pg_indx;
    struct page* sf_pgs = p->pg_m.swapFile_pgs;
    int found = 0;
    for(int i = 0; i < MAX_PSYC_PAGES; i++) {
      if(sf_pgs[i].vaddr == -1) {
        found = 1;
        sf_pgs[i].vaddr = v_addr;
        pg_indx = i;
        printf("removed page at v_addr %x from memory_pgs of process %d\n", v_addr, p->pid);
        break;
      }
    }

    if(!found) {
      panic("swap_out: no space in swapFile");
    }
    
    char* pa = (char*)PTE2PA(*pte);
    if(writeToSwapFile(p, pa, pg_indx*PGSIZE, PGSIZE) < 0)
      panic("swap_out: writeToSwapFile failed");
    uvmunmap(p->pagetable, v_addr, 1, 1); // Unmap the page and free the memory
    *pte = *pte | PTE_PG; // Turn on swapped flag
  }
  
  struct page* mem_pgs = p->pg_m.memory_pgs;
  int found = 0;
  for(int i = 0; i < MAX_PSYC_PAGES; i++) {
    if(mem_pgs[i].vaddr == v_addr) {
      found = 1;
      mem_pgs[i].vaddr = -1;
      mem_pgs[i].nfua_counter = 0;
      mem_pgs[i].lapa_counter = 0xFFFFFFFF;

      // remove pg from the circular list
      mem_pgs[i].next->prev = mem_pgs[i].prev;
      mem_pgs[i].prev->next = mem_pgs[i].next;

      // if the pg was the first_added_pg, update it to point to the next added pg
      if(&mem_pgs[i] == p->pg_m.first_added_pg) {
        if(mem_pgs[i].next) {
          p->pg_m.first_added_pg = mem_pgs[i].next;
        } else {
          p->pg_m.first_added_pg = 0;
        } 
      }
      
      mem_pgs[i].next = 0;
      mem_pgs[i].prev = 0;

      printf("removed page at v_addr %x from memory_pgs of process %d\n", v_addr, p->pid);
      break;
    }
  }

  if(!found) {
    panic("swap_out: v_addr was not in memory");
  } 
  
  p->pg_m.num_of_pgs_in_memory -= 1;

  *pte = *pte & !PTE_V; // Turn off valid flag
  
  

  // Move a page_node* from memory to swapFile,
  // copy the page to swapFile,
  // free the memory,
  // inc num_of_pages_in_swapFile
  // dec num_of_pages_in_memory
  // set PTE_PG and turn off PTE_V

  // if to_swapFile == 1, select a page based oin policy, else remove v_addr
}

void
swap_in(uint64 v_addr, int from_swapFile)
{  
  struct proc* p = myproc();

  if(p->pid < 2 || SWAP_ALGO == NONE) {
    return;
  }

  pte_t* pte = walk(p->pagetable, v_addr, 0);

  if(from_swapFile) {
    uint64 sz;
    if((sz = uvmalloc(p->pagetable, p->sz, p->sz + PGSIZE)) == 0)
      panic("choose_and_swap: uvmalloc failed");
    uint64 new_pg_v_addr = sz - PGSIZE;
    uint64 new_pg_p_addr = walkaddr(p->pagetable, new_pg_v_addr);
    
    int permissions = PTE_FLAGS(*walk(p->pagetable, v_addr, 0));
    permissions = (permissions & !PTE_PG) | PTE_V; // Turn off swapped flag and on valid flag


    uvmunmap(p->pagetable, new_pg_v_addr, 1, 0); // unmmap new p_addr from new v_addr
    mappages(p->pagetable, v_addr, PGSIZE, new_pg_p_addr, permissions); // map new p_addr to old v_addr

    int index_in_sf;
    struct page* pgs_in_sf = p->pg_m.swapFile_pgs;
    int found  = 0;
    for(int i = 0; i < MAX_PSYC_PAGES; i++) {
      if(pgs_in_sf[i].vaddr == v_addr) {
        index_in_sf = i;
        pgs_in_sf[i].vaddr = -1;
        found = 1;
        break;
      }
    }

    if(!found) {
      panic("swap_in: v_addr_to_swap_in not in swapFile");
    }

    readFromSwapFile(p, (char*)new_pg_p_addr, index_in_sf*PGSIZE, PGSIZE);
  }

  *pte = *pte | PTE_V;    // Turn on valid flag
  *pte = *pte & !PTE_PG;  // Turn off swapped flag

  struct page* mem_pgs = p->pg_m.memory_pgs;
  int found = 0;
  // int index_in_mem_pgs;
  for(int i = 0; i < MAX_PSYC_PAGES; i++) {
    if(mem_pgs[i].vaddr == -1) {
      found = 1;
      mem_pgs[i].vaddr = v_addr;
      mem_pgs[i].nfua_counter = 0;
      mem_pgs[i].lapa_counter = 0xFFFFFFFF;

      // add the page behind the first_added_pg in the circular list
      struct page* first_pg = p->pg_m.first_added_pg;
      if(first_pg == 0) {
        // if it's the first page added in this process
        p->pg_m.first_added_pg = &mem_pgs[i];
      } else {
        first_pg->prev->next = &mem_pgs[i];
        mem_pgs[i].prev = first_pg->prev;
        first_pg->prev = &mem_pgs[i];
        mem_pgs[i].next = first_pg;
      }

      // index_in_mem_pgs = i;
      printf("added page at v_addr %x to memory_pgs of process %d\n", v_addr, p->pid);
      break;
    }
  }

  if(!found) {
    panic("swap_in: no free slots");
  }

  p->pg_m.num_of_pgs_in_memory += 1;

  // return index_in_mem_pgs;
}

uint64
uvmalloc_wrapper(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  // char *mem;
  uint64 a;
  struct proc* p = myproc();

  if(p->pid < 2 || SWAP_ALGO == NONE) {
    return uvmalloc(pagetable, oldsz, newsz);
  }

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  uint64 output = newsz;
  for(a = oldsz; a < newsz; a += PGSIZE){
    // If a process allocates more than 32 pages, terminate it
    if(p->pg_m.num_of_pgs_in_memory + p->pg_m.num_of_pgs_in_swapFile >= MAX_TOTAL_PAGES) {
      printf("process %s exceeded %d pages\n", p->pid, MAX_TOTAL_PAGES);
      exit(1);
    }

    // If there are too many pages in memory, swap one out
    if(p->pg_m.num_of_pgs_in_memory == MAX_PSYC_PAGES) {
      uint64 v_addr = choose_pg_to_swap();
      swap_out(v_addr, 1); // select a page based on policy to move to swapFile
    }

    // Allocate page in memory
    output = uvmalloc(pagetable, a, a + PGSIZE);
    
    if (output != 0) {
      // Insert the new page to the first free slot in pg_m.memory_pgs
      swap_in(a, 0);
    }
  }

  return output;
}

uint64
uvmdealloc_wrapper(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{  
  struct proc* p = myproc();
  if(p->pid < 2 || SWAP_ALGO == NONE) {
    return uvmdealloc(pagetable, oldsz, newsz);
  }
  
  uint64 output = oldsz;

  for(uint64 a = PGROUNDUP(newsz); a < PGROUNDUP(oldsz); a += PGSIZE){
    output = uvmdealloc(pagetable, a + PGSIZE, a);
    // Update p->pg_metadata
    swap_out(a, 0); 
  }

  return output;
}

uint64
nfua(){
  struct page* memory_pgs = myproc()->pg_m.memory_pgs;
  uint64 to_swap_out = 0;
  uint64 curr_min_counter = -1;

  for(int i = 0; i < MAX_PSYC_PAGES; i++){
    // get curr page
    struct page pg = memory_pgs[i];

    if(pg.vaddr != -1){
      if(curr_min_counter == -1 || curr_min_counter > pg.nfua_counter){
        curr_min_counter = pg.nfua_counter;
        to_swap_out = pg.vaddr;
      }
    }
  }

  return to_swap_out;
}

// Brian Kernighan's algorithm for counting the number of ones in a binary number
int
num_of_ones(int num)
{
  int counter = 0;
  while(num != 0) {
    num  = num & (num-1);
    counter += 1;
  }

  return counter;
}

uint64
lapa()
{
  struct page* mem_pgs = myproc()->pg_m.memory_pgs;
  uint64 min_lapa_counter = 0xFFFFFFFF; // max num of ones in an int
  uint64 chosen_vaddr = mem_pgs[3].vaddr; // just a place holder

  for (int i = 0; i < MAX_PSYC_PAGES; i++) {
    struct page pg = mem_pgs[i];
    if(pg.vaddr != -1) {
      int curr_num_of_ones = num_of_ones(pg.lapa_counter);
      int min_num_of_ones = num_of_ones(min_lapa_counter);
      if( curr_num_of_ones < min_num_of_ones || (curr_num_of_ones == min_num_of_ones && pg.lapa_counter <= min_lapa_counter)) {
        min_lapa_counter = pg.lapa_counter;
        chosen_vaddr = pg.vaddr;
      }
    }
  }

  return chosen_vaddr;
}

uint64
sc_fifo()
{
  struct proc* p = myproc();
  struct page* iter = p->pg_m.first_added_pg;
  for(;;) {
    pte_t* pte = walk(p->pagetable, iter->vaddr, 0);
    int reference_bit = *pte & PTE_A;
    if(reference_bit == 0) {
      // // remove page from circular list
      // iter->prev->next = iter->next; 
      // iter->next->prev = iter->prev;
      // // if it was the first_added_pg, update it to point to the next added page
      // if(iter == p->pg_m.first_added_pg) {
      //   p->pg_m.first_added_pg = iter->next;
      // }
      return iter->vaddr;

    } else {
      *pte = *pte & !PTE_A; // zero the reference bit
      iter = iter->next;
    }
  }
}


uint64 
choose_pg_to_swap()
{
  if(SWAP_ALGO == NFUA)
    return nfua();
  else if (SWAP_ALGO == LAPA)
    return lapa();
  else if(SWAP_ALGO == SCFIFO)
    return sc_fifo();
  else
    panic("choose_pg_to_swap: trying to swap a page with paging disabled");
  return -1;
  // return p->pg_m.memory_pgs[4].vaddr;
}
