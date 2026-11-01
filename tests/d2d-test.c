// Two-chip RTL test of the chiplet router over a D2D link, for any link type
// (e.g. DualUcieNoPhyConfig).
//
// Both chips run this binary, so each learns its chip ID (1 or 2) from the
// router, preloaded by plusarg. From sims/verilator:
//   make run-binary CONFIG=DualUcieNoPhyConfig BINARY=../../tests/build/d2d-test.riscv
//     EXTRA_SIM_FLAGS="+chip_id0=0x00004080:0x00000001 +chip_id1=0x00004080:0x00000002"
//
// Each chip routes the peer's chip ID to D2D port 0, writes then reads back a
// buffer in the peer's memory (its own slot there), then sets its done flag
// and waits for the peer's. The binaries are the same, so a global has the
// same address on both chips.

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

// Slot `chip_id - 1` is written by that chip.
volatile uint64_t slot[2][N_WORDS];
volatile uint32_t done_flag;

static uint64_t pattern(int chip_id, int i) {
  return ((uint64_t)chip_id << 56) | (0xD2DULL << 32) | i;
}

int main(void) {
  int chip_id = reg_read64(CHIP_ID_ADDR);
  int peer_id = 3 - chip_id;
  uintptr_t peer = OFFCHIP_OFFSET * peer_id;
  program_router(0, peer_id, 0);
  printf("Chip %d: routing chip %d -> port 0\n", chip_id, peer_id);

  uint64_t settle = rdcycle() + SETTLE_CYCLES;
  while (rdcycle() < settle)
    ;

  volatile uint64_t *remote = (volatile uint64_t *)((uintptr_t)slot[chip_id - 1] + peer);

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

  done_flag = DONE_MAGIC(chip_id);
  volatile uint32_t *peer_done = (volatile uint32_t *)((uintptr_t)&done_flag + peer);
  while (*peer_done != DONE_MAGIC(peer_id))
    ;

  printf("Chip %d: PASS\n", chip_id);
  return 0;
}
