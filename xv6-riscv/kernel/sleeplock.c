// Sleeping locks

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  if (get_debug_mode())
    printf("called acquire on lock %d 35\n", &lk->lk);
  acquire(&lk->lk);
  // // printf("acquire acquiresleep\n");
  while (lk->locked) {
    // printf("sleep called from acquiresleep\n");
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  if (get_debug_mode())
    printf("called release on lock %d 39\n", &lk->lk);
  release(&lk->lk);
  // printf("release acquiresleep\n");
}

void
releasesleep(struct sleeplock *lk)
{
  if (get_debug_mode())
    printf("called acquire on lock %d 36\n", &lk->lk);
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  if (get_debug_mode())
    printf("called release on lock %d 40\n", &lk->lk);
  release(&lk->lk);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  if (get_debug_mode())
    printf("called acquire on lock %d 37\n", &lk->lk);
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  if (get_debug_mode())
    printf("called release on lock %d 41\n", &lk->lk);
  release(&lk->lk);
  return r;
}



