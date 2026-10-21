// Two-chip test of the chiplet router over a D2D CTC link (FireSim
// D2DCTCFireSimConfig + D2DLinkBridge).
//
// Each FireSim node runs the same plusargs, so a chip can't learn its ID from
// them: build one binary per chip (d2d-ctc-test-chip1 / -chip2, CHIP_ID set at
// compile time) and give each node its own job in the workload.
//
// Each chip:
//   1. programs its own chip ID into the router (incoming requests only have
//      their chip-ID tag stripped when it matches this register),
//   2. routes the peer's chip ID to D2D port 0,
//   3. waits for the peer to do the same,
//   4. writes then reads back a buffer in the peer's memory (its own slot of
//      the peer's mailbox),
//   5. sets its done flag and waits for the peer's (read remotely).

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <riscv-pk/encoding.h>
#include "marchid.h"
#include "mmio.h"
#include "router.h"

#ifndef CHIP_ID
#error "Build with -DCHIP_ID=1 or -DCHIP_ID=2"
#endif

#define PEER_ID (3 - CHIP_ID)

// Must match WithOffchipAddressRange in D2DCTCFireSimConfig
#define OFFCHIP_OFFSET 0x800000000L
#define PEER_OFFSET    (OFFCHIP_OFFSET * PEER_ID)

// Long enough for the peer to finish steps 1-2 after it comes out of reset.
#define SETTLE_CYCLES  200000

// Fixed mailbox in each chip's DRAM, well past the program image. The two
// binaries are built separately, so their symbol addresses may differ; only
// fixed addresses are safe to use in the peer's memory.
#define MAILBOX_BASE   0x90000000UL
#define SLOT(writer)   ((uint32_t *)(MAILBOX_BASE + 0x100 * (writer)))  // written by chip `writer`
#define DONE_FLAG      ((volatile uint32_t *)(MAILBOX_BASE + 0x1000))    // in the owner's memory
#define DONE_MAGIC(id) (0xD2D00000u | (id))

#define N_WORDS 10

uint32_t src[N_WORDS];
uint32_t test[N_WORDS];

// CTC only supports 32-bit transactions, so move one word at a time with
// explicit lw/sw.
static inline void copy_words(uint32_t *to, uint32_t *from, int n) {
  for (int i = 0; i < n; i++) {
    asm volatile(
        "lw  t0, 0(%1)\n"
        "sw  t0, 0(%0)\n"
        :
        : "r"(to + i), "r"(from + i)
        : "t0", "memory");
  }
}

static inline uint32_t remote_read32(uintptr_t local_addr) {
  uint32_t v;
  asm volatile("lw %0, 0(%1)\n"
               : "=r"(v)
               : "r"(local_addr + PEER_OFFSET)
               : "memory");
  return v;
}

int main(void) {
  // DRAM isn't cleared between FireSim runs: clear our flag before the peer
  // can look at it (it waits SETTLE_CYCLES first).
  *DONE_FLAG = 0;
  reg_write64(CHIP_ID_ADDR, CHIP_ID);
  program_router(0, PEER_ID, 0);
  printf("Chip %d: routing chip %d -> port 0\n", CHIP_ID, PEER_ID);

  uint64_t settle = rdcycle() + SETTLE_CYCLES;
  while (rdcycle() < settle)
    ;

  for (int i = 0; i < N_WORDS; i++)
    src[i] = (CHIP_ID << 16) | i;

  uint32_t *remote_dest = (uint32_t *)((uintptr_t)SLOT(CHIP_ID) + PEER_OFFSET);

  uint64_t t0 = rdcycle();
  copy_words(remote_dest, src, N_WORDS);
  uint64_t t1 = rdcycle();
  copy_words(test, remote_dest, N_WORDS);
  uint64_t t2 = rdcycle();

  for (int i = 0; i < N_WORDS; i++) {
    if (test[i] != src[i]) {
      printf("Chip %d: FAIL at word %d: wrote 0x%x, read back 0x%x\n",
             CHIP_ID, i, src[i], test[i]);
      exit(1);
    }
  }
  printf("Chip %d: wrote %d words in %lu cycles, read back in %lu cycles\n",
         CHIP_ID, N_WORDS, t1 - t0, t2 - t1);

  *DONE_FLAG = DONE_MAGIC(CHIP_ID);
  while (remote_read32((uintptr_t)DONE_FLAG) != DONE_MAGIC(PEER_ID))
    ;

  printf("Chip %d: PASS\n", CHIP_ID);
  return 0;
}
