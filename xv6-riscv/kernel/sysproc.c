#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  char msg[32];

  if(argint(0, &n) < 0)
    return -1;
  
  if(argstr(1, msg, 32) < 0)  // TODO change 32 to global var, 1 intead of 0
    return -1;

  exit(n, msg);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  uint64 msg_p;

  if(argaddr(0, &p) < 0)
    return -1;

  if(argaddr(1, &msg_p) < 0)
    return -1;

  return wait(p, msg_p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_memsize(void)
{
   return myproc()->sz;
}

uint64
sys_set_ps_priority(void)
{
  int n;

  if(argint(0, &n) < 0 || n < 1 || n > 10)
    return -1;

  struct proc* p = myproc();
  
  acquire(&p->lock);
  p->ps_priority = n;
  release(&p->lock);

  return 0;
}

uint64
sys_set_cfs_priority(void)
{
  int n;
  if(argint(0, &n) < 0 || n < 0 || n > 2)
    return -1;

  struct  proc* p = myproc();

  acquire(&p->lock);
  p->cfs_priority = n;
  release(&p->lock);
  
  return 0;
}

uint64
sys_get_cfs_stats(void)
{
  int pid;
  uint64 cfs_priority;
  uint64 rtime;
  uint64 stime;
  uint64 retime; 

  if (argint(0, &pid) < 0 || argaddr(1, &cfs_priority) < 0 || argaddr(2, &rtime) < 0 || argaddr(3, &stime) < 0 || argaddr(4, &retime) < 0)
    return -1;

  struct proc* p = get_proc_by_pid(pid);
  
  acquire(&p->lock);
  if ((cfs_priority != 0 && copyout(myproc()->pagetable, cfs_priority, (char *)&p->cfs_priority, sizeof(int)) < 0) ||
      (rtime != 0 && copyout(myproc()->pagetable, rtime, (char *)&p->rtime, sizeof(int)) < 0) ||
      (stime != 0 && copyout(myproc()->pagetable, stime, (char *)&p->stime, sizeof(int)) < 0) ||
      (retime != 0 && copyout(myproc()->pagetable, retime, (char *)&p->retime, sizeof(int)) < 0))
  {
    release(&p->lock);
    return -1;
  }
  release(&p->lock);
  
  return 0;
}