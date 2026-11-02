// Two-chip test of the chiplet router over a D2D link, for any link type
// (e.g. DualUcieNoPhyConfig in RTL sim, UCIeFireSimConfig in FireSim).
//
// Each chip needs its chip ID (1 or 2):
// - RTL sim: one binary (d2d-test) for both chips, which reads the ID from the
//   router, preloaded by plusarg. From sims/vcs:
//     make run-binary CONFIG=DualUcieNoPhyConfig BINARY=../../tests/build/d2d-test.riscv
//       EXTRA_SIM_FLAGS="+chip_id0=0x00004080:0x00000001 +chip_id1=0x00004080:0x00000002"
// - FireSim: every node gets the same plusargs, so build one binary per chip
//   (d2d-test-chip1 / -chip2, CHIP_ID set at compile time), which programs
//   its own ID into the router.
//
// Each chip routes the peer's chip ID to D2D port 0, writes then reads back a
// buffer in the peer's memory (its own slot there), then sets its done flag
// and waits for the peer's. The per-chip binaries are built separately, so
// shared data lives at fixed addresses rather than in globals.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <riscv-pk/encoding.h>
#include "marchid.h"
#include "mmio.h"
#include "router.h"

// Must match WithOffchipAddressRange in the config
#define OFFCHIP_OFFSET 0x800000000L

// Long enough for the peer to program its router after reset.
#define SETTLE_CYCLES 20000

#define N_WORDS 16
#define DONE_MAGIC(id) (0xD2D00000u | (id))

// Fixed mailbox in each chip's DRAM, past the program image and within the
// smallest DRAM used (256 MiB in RTL sim).
#define MAILBOX_BASE 0x88000000UL
// Slot `chip_id - 1` is written by that chip.
#define SLOT(chip_id) ((volatile uint64_t *)(MAILBOX_BASE + 0x100 * ((chip_id) - 1)))
#define DONE_FLAG ((volatile uint32_t *)(MAILBOX_BASE + 0x1000))

static uint64_t pattern(int chip_id, int i) {
  return ((uint64_t)chip_id << 56) | (0xD2DULL << 32) | i;
}

int main(void) {
  // DRAM isn't cleared between FireSim runs: clear our flag before the peer
  // can look at it (it waits SETTLE_CYCLES first).
  *DONE_FLAG = 0;
#ifdef CHIP_ID
  int chip_id = CHIP_ID;
  reg_write64(CHIP_ID_ADDR, chip_id);
#else
  int chip_id = reg_read64(CHIP_ID_ADDR);
#endif
  int peer_id = 3 - chip_id;
  uintptr_t peer = OFFCHIP_OFFSET * peer_id;
  program_router(0, peer_id, 0);
  printf("Chip %d: routing chip %d -> port 0\n", chip_id, peer_id);

  uint64_t settle = rdcycle() + SETTLE_CYCLES;
  while (rdcycle() < settle)
    ;

  volatile uint64_t *remote = (volatile uint64_t *)((uintptr_t)SLOT(chip_id) + peer);

  uint64_t t0 = rdcycle();
  for (int i = 0; i < N_WORDS; i++)
    remote[i] = pattern(chip_id, i);
  uint64_t t1 = rdcycle();
  for (int i = 0; i < N_WORDS; i++) {
    uint64_t read = remote[i];
    if (read != pattern(chip_id, i)) {
      printf("Chip %d: FAIL at word %d: wrote 0x%lx, read back 0x%lx\n",
             chip_id, i, pattern(chip_id, i), read);
      exit(1);
    }
  }
  uint64_t t2 = rdcycle();
  printf("Chip %d: wrote %d words in %lu cycles, read back in %lu\n",
         chip_id, N_WORDS, t1 - t0, t2 - t1);

  *DONE_FLAG = DONE_MAGIC(chip_id);
  volatile uint32_t *peer_done = (volatile uint32_t *)((uintptr_t)DONE_FLAG + peer);
  while (*peer_done != DONE_MAGIC(peer_id))
    ;

  printf("Chip %d: PASS\n", chip_id);
  return 0;
}
