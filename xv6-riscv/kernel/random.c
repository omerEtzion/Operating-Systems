#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

static uint8 lfsr = 0x2A; // Shared seed value

struct
{
	struct spinlock lock;

	// input
#define INPUT_BUF 128
	char buf[INPUT_BUF];
	uint r; // Read index
	uint w; // Write index
	uint e; // Edit index
} rand;

uint8 lfsr_char()
{
	uint8 bit;
	bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
	lfsr = (lfsr >> 1) | (bit << 7);
	return lfsr;
}

int randomwrite(int user_src, uint64 src, int n)
{

	if (n != 1)
		return -1;

	uint8 c;
	if (either_copyin(&c, user_src, src, 1) == -1)
		return -1;

	acquire(&rand.lock);
	lfsr = c;
	release(&rand.lock);

	return 1;
}

int randomread(int user_dst, uint64 dst, int n)
{
	int target = n;
	while(n > 0) {
		acquire(&rand.lock);
		if(either_copyout(user_dst, dst, &lfsr, 1) == -1)
      break;
		// printf("\n%x\n", lfsr);
		dst++;
    --n;
		lfsr_char();
		release(&rand.lock);
	}

	return target - n;
}

void randominit(void)
{
	initlock(&rand.lock, "rand");
	lfsr = 0x2A;
	// connect read and write system calls
  // to randomread and randomwrite.
  devsw[RANDOM].read = randomread;
  devsw[RANDOM].write = randomwrite;
}
