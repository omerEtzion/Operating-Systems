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

static uint8_t random_seed = 0x2A; // Shared seed value

struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF 128
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} rand;


uint8_t lfsr_char(uint8_t lfsr) {
    uint8_t bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
    lfsr = (lfsr >> 1) | (bit << 7);
    return lfsr;
}

int randomwrite(int user_src, uint64 src, int n) {

    if (n != 1)
        return -1;

    char c;
    if (either_copyin(&c, user_src, src, 1) == -1)
        return -1;

    acquire(&rand.lock);
    random_seed = c;
    release(&rand.lock);

    return 1;

}

int randomread(int user_dst, uint64 dst, int n) {
    

}

void randominit(void) {

}
