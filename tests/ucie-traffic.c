// Two-chip UCIe traffic, for checking the link model (UcieLinkModel) against
// the full controller + PHY link (see UCIE_LINK_MODEL_PLAN.md). Run the same
// binary family on both and compare the cycle counts:
// - ucie-traffic (DualUcie1GHzConfig): brings the link up first.
// - ucie-traffic-nophy (DualUcieNoPhyModel1GHzConfig).
//
// Scripts, all timed with rdcycle on the chip that issues them:
// 1. isolated remote loads (unloaded round trip)
// 2. a burst of back-to-back remote stores, then a load that waits for them
// 3. a burst of back-to-back remote loads
// 4. scripts 2 and 3 from both chips at once, so A and D frames share each
//    direction
//
// All times are in core cycles, which are link cycles here.

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

#define N_ISOLATED 16
#define N_BURST 64
#define N_REPEAT 4

// Fixed mailbox in each chip's DRAM, past the program image. The peer writes
// these remotely.
#define MAILBOX_BASE 0x88000000UL
#define TARGET ((volatile uint64_t *)MAILBOX_BASE)
#define GO_FLAG ((volatile uint32_t *)(MAILBOX_BASE + 0x1000))
#define DONE_FLAG ((volatile uint32_t *)(MAILBOX_BASE + 0x1008))
#define DONE_MAGIC 0xD2D0D04Eu

static void idle(uint64_t cycles) {
  uint64_t end = rdcycle() + cycles;
  while (rdcycle() < end)
    ;
}

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

// N_BURST stores, eight per iteration, then a load from the same device,
// which can't complete until the stores have.
static uint64_t store_burst(volatile uint64_t *p, uint64_t v) {
  uint64_t t0 = rdcycle();
  for (int i = 0; i < N_BURST; i += 8) {
    p[i + 0] = v + i + 0;
    p[i + 1] = v + i + 1;
    p[i + 2] = v + i + 2;
    p[i + 3] = v + i + 3;
    p[i + 4] = v + i + 4;
    p[i + 5] = v + i + 5;
    p[i + 6] = v + i + 6;
    p[i + 7] = v + i + 7;
  }
  uint64_t last = p[N_BURST - 1];
  asm volatile("add %0, %0, zero" : "+r"(last));
  uint64_t t1 = rdcycle();
  if (last != v + N_BURST - 1) {
    printf("FAIL: store burst read back 0x%lx\n", last);
    exit(1);
  }
  return t1 - t0;
}

static uint64_t load_burst(volatile uint64_t *p) {
  uint64_t sum = 0;
  uint64_t t0 = rdcycle();
  for (int i = 0; i < N_BURST; i += 8) {
    sum += p[i + 0] + p[i + 1] + p[i + 2] + p[i + 3] + p[i + 4] + p[i + 5] +
           p[i + 6] + p[i + 7];
  }
  asm volatile("add %0, %0, zero" : "+r"(sum));
  uint64_t t1 = rdcycle();
  return t1 - t0;
}

static void report(int chip, const char *what, const uint64_t *s, int n) {
  uint64_t min = s[0], max = s[0], sum = 0;
  for (int i = 0; i < n; i++) {
    if (s[i] < min) min = s[i];
    if (s[i] > max) max = s[i];
    sum += s[i];
  }
  printf("Chip %d: %s:", chip, what);
  for (int i = 0; i < n; i++)
    printf(" %lu", s[i]);
  uint64_t mean100 = sum * 100 / n;
  printf("\nChip %d: %s: min %lu mean %lu.%02lu max %lu\n", chip, what, min,
         mean100 / 100, mean100 % 100, max);
}

// Both chips run the same bursts at once, after chip 1 says go.
static void concurrent(int chip_id, volatile uint64_t *remote) {
  uint64_t stores[N_REPEAT], loads[N_REPEAT];
  for (int r = 0; r < N_REPEAT; r++) {
    stores[r] = store_burst(remote + 128, 0xC0000000u * chip_id + r * 256);
    loads[r] = load_burst(remote + 128);
  }
  report(chip_id, "concurrent store burst", stores, N_REPEAT);
  report(chip_id, "concurrent load burst", loads, N_REPEAT);
}

int main(void) {
  *GO_FLAG = 0;
  *DONE_FLAG = 0;
  int chip_id = reg_read64(CHIP_ID_ADDR);
  int peer_id = 3 - chip_id;
  uintptr_t peer = OFFCHIP_OFFSET * peer_id;
  volatile uint64_t *remote = (volatile uint64_t *)((uintptr_t)TARGET + peer);
  volatile uint32_t *peer_go = (volatile uint32_t *)((uintptr_t)GO_FLAG + peer);
  volatile uint32_t *peer_done =
      (volatile uint32_t *)((uintptr_t)DONE_FLAG + peer);

#ifdef UCIE_PHY
  setup_ucie_digital(UCIE_BASE);
#endif

  program_router(0, peer_id, 0);
  idle(SETTLE_CYCLES);

  if (chip_id == 2) {
    while (*GO_FLAG != DONE_MAGIC)
      ;
    concurrent(chip_id, remote);
    *peer_done = DONE_MAGIC;
    return 0;
  }

  uint64_t isolated[N_ISOLATED], stores[N_REPEAT], loads[N_REPEAT];
  for (int i = 0; i < N_ISOLATED; i++) {
    idle(500 + 37 * i);
    isolated[i] = timed_load(&remote[i]);
  }
  for (int r = 0; r < N_REPEAT; r++) {
    idle(500 + 37 * r);
    stores[r] = store_burst(remote, 0xD2D00000u + r * 256);
  }
  for (int r = 0; r < N_REPEAT; r++) {
    idle(500 + 37 * r);
    loads[r] = load_burst(remote);
  }
  report(1, "isolated load", isolated, N_ISOLATED);
  report(1, "store burst", stores, N_REPEAT);
  report(1, "load burst", loads, N_REPEAT);

  idle(1000);
  *peer_go = DONE_MAGIC;
  concurrent(chip_id, remote);
  while (*DONE_FLAG != DONE_MAGIC)
    ;
  printf("Chip 1: PASS\n");
  return 0;
}
