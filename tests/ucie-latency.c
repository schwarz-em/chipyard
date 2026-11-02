// Two-chip UCIe latency measurement, for calibrating the FireSim link model
// (see UCIE_LINK_MODEL_PLAN.md).
//
// Chip 1 times isolated remote loads and stores to chip 2's memory; chip 2
// only serves them. Run the same measurement over the full link and over the
// PHY-less one; half the difference in round trip is the link's one-way
// latency, since everything else on the path is the same:
// - ucie-latency (DualUcieConfig): brings the link up through the UCIe
//   digital controller first.
// - ucie-latency-nophy (DualUcieNoPhyConfig): no registers, no bring-up.
// From sims/vcs:
//   make run-binary CONFIG=<config> BINARY=../../tests/build/<binary>.riscv
//     EXTRA_SIM_FLAGS="+chip_id0=0x00004080:0x00000001 +chip_id1=0x00004080:0x00000002"
//
// All times are in core cycles (rdcycle).

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <riscv-pk/encoding.h>
#include "marchid.h"
#include "mmio.h"
#include "router.h"
#ifdef UCIE_PHY
#include "ucie.h"
#endif

// Must match the configs (UcieTLParams.address, WithOffchipAddressRange)
#define UCIE_BASE 0x200000UL
#define OFFCHIP_OFFSET 0x800000000L

// Long enough for the peer to program its router after reset.
#define SETTLE_CYCLES 20000

#define N_SAMPLES 16
#define DONE_MAGIC 0xD2D0D04Eu

// Fixed mailbox in each chip's DRAM, past the program image.
#define MAILBOX_BASE 0x88000000UL
#define TARGET ((volatile uint64_t *)MAILBOX_BASE)
#define DONE_FLAG ((volatile uint32_t *)(MAILBOX_BASE + 0x1000))

static void idle(uint64_t cycles) {
  uint64_t end = rdcycle() + cycles;
  while (rdcycle() < end)
    ;
}

// One uncached load, timed to when its data can be used: the add waits on
// the load, and the second rdcycle waits on the add.
static uint64_t timed_load(volatile uint64_t *p) {
  uint64_t t0, t1, v;
  asm volatile(
      "rdcycle %0\n"
      "ld %1, 0(%3)\n"
      "add %1, %1, zero\n"
      "rdcycle %2\n"
      : "=&r"(t0), "=&r"(v), "=&r"(t1)
      : "r"(p)
      : "memory");
  return t1 - t0;
}

// One uncached store, timed to its acknowledgement (the fence waits on it).
static uint64_t timed_store(volatile uint64_t *p, uint64_t v) {
  uint64_t t0, t1;
  asm volatile(
      "rdcycle %0\n"
      "sd %3, 0(%2)\n"
      "fence\n"
      "rdcycle %1\n"
      : "=&r"(t0), "=&r"(t1)
      : "r"(p), "r"(v)
      : "memory");
  return t1 - t0;
}

static void report(const char *what, const uint64_t *s) {
  uint64_t min = s[0], max = s[0], sum = 0;
  for (int i = 0; i < N_SAMPLES; i++) {
    if (s[i] < min) min = s[i];
    if (s[i] > max) max = s[i];
    sum += s[i];
  }
  printf("%s:", what);
  for (int i = 0; i < N_SAMPLES; i++)
    printf(" %lu", s[i]);
  // Mean in hundredths of a cycle, since printf may lack floats.
  uint64_t mean100 = sum * 100 / N_SAMPLES;
  printf("\n%s: min %lu mean %lu.%02lu max %lu\n", what, min,
         mean100 / 100, mean100 % 100, max);
}

int main(void) {
  *DONE_FLAG = 0;
  int chip_id = reg_read64(CHIP_ID_ADDR);
  int peer_id = 3 - chip_id;
  uintptr_t peer = OFFCHIP_OFFSET * peer_id;

#ifdef UCIE_PHY
  uint64_t t = rdcycle();
  setup_ucie_digital(UCIE_BASE);
  printf("Chip %d: link up after %lu cycles\n", chip_id, rdcycle() - t);
#endif

  program_router(0, peer_id, 0);
  idle(SETTLE_CYCLES);

  if (chip_id == 2) {
    // Serve chip 1's requests until it says it's done.
    while (*DONE_FLAG != DONE_MAGIC)
      ;
    printf("Chip 2: done\n");
    return 0;
  }

  volatile uint64_t *remote = (volatile uint64_t *)((uintptr_t)TARGET + peer);
  volatile uint64_t *local = TARGET;
  uint64_t loads[N_SAMPLES], stores[N_SAMPLES], local_loads[N_SAMPLES];

  // Idle gaps of different lengths between samples, so the link is quiet
  // (credits returned) and the clock crossings are sampled at different
  // phases.
  for (int i = 0; i < N_SAMPLES; i++) {
    idle(500 + 37 * i);
    stores[i] = timed_store(&remote[i], 0xD2D00000u + i);
  }
  for (int i = 0; i < N_SAMPLES; i++) {
    idle(500 + 37 * i);
    loads[i] = timed_load(&remote[i]);
  }
  for (int i = 0; i < N_SAMPLES; i++) {
    idle(500 + 37 * i);
    local_loads[i] = timed_load(&local[i]);
  }
  for (int i = 0; i < N_SAMPLES; i++) {
    if (remote[i] != 0xD2D00000u + i) {
      printf("Chip 1: FAIL at word %d: read back 0x%lx\n", i, remote[i]);
      exit(1);
    }
  }

  report("Chip 1: remote store", stores);
  report("Chip 1: remote load", loads);
  report("Chip 1: local load", local_loads);

  *(volatile uint32_t *)((uintptr_t)DONE_FLAG + peer) = DONE_MAGIC;
  printf("Chip 1: PASS\n");
  return 0;
}
