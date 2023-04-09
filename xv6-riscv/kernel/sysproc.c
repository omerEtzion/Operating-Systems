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
  printf("after2\n");

  
  acquire(&p->lock);
  printf("%d, %d, %d, %d, %d\n", myproc()->pid, cfs_priority, rtime, stime, retime);
  *((int*)cfs_priority) = p->cfs_priority;
  printf("2\n");
  *((int*)rtime) = p->rtime;
  printf("3\n");
  *((int*)stime) = p->stime;
  printf("4\n");
  *((int*)retime) = p->retime;
  printf("5\n");
  release(&p->lock);
  
  return 0;
}