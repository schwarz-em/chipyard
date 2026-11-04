// Two-chip UCIe bulk traffic from the Saturn DMA vector unit, for checking
// the link model's rate against the full controller + PHY link (see
// UCIE_LINK_MODEL_PLAN.md). Same comparison as ucie-traffic.c, but the vector
// unit keeps many requests in flight:
// - ucie-dma (DualUcieSaturn1GHzConfig): brings the link up first.
// - ucie-dma-nophy (DualUcieNoPhyModelSaturn1GHzConfig).
//
// Scripts, all timed with rdcycle on the chip that issues them, each copy
// ending in a fence that waits for every response:
// 1. vector copy local -> remote (stores: A frames carry data out)
// 2. vector copy remote -> local (loads: D frames carry data back)
// 3. script 1 from both chips at once, so A and D frames share each direction
//
// All times are in core cycles, which are link cycles here.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <riscv-pk/encoding.h>
#include <riscv_vector.h>
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

#define COPY_BYTES 4096
#define N_REPEAT 3

// Fixed mailbox and buffers in each chip's DRAM, past the program image. The
// peer writes the flags remotely.
#define MAILBOX_BASE 0x88000000UL
#define GO_FLAG ((volatile uint32_t *)(MAILBOX_BASE + 0x0))
#define DONE_FLAG ((volatile uint32_t *)(MAILBOX_BASE + 0x8))
#define SRC ((uint8_t *)(MAILBOX_BASE + 0x10000))
#define DST ((uint8_t *)(MAILBOX_BASE + 0x20000))
#define DONE_MAGIC 0xD2D0D04Eu

static void idle(uint64_t cycles) {
  uint64_t end = rdcycle() + cycles;
  while (rdcycle() < end)
    ;
}

// Unit-stride vector copy, then a fence so every store is acknowledged (or
// every load returned) before the time is taken.
static uint64_t vcopy(uint8_t *dst, const uint8_t *src, size_t n) {
  uint64_t t0 = rdcycle();
  for (size_t vl; n > 0; n -= vl, src += vl, dst += vl) {
    vl = __riscv_vsetvl_e8m8(n);
    vuint8m8_t v = __riscv_vle8_v_u8m8(src, vl);
    __riscv_vse8_v_u8m8(dst, v, vl);
  }
  asm volatile("fence" ::: "memory");
  return rdcycle() - t0;
}

static void check(int chip, const uint8_t *p, uint8_t seed) {
  for (int i = 0; i < COPY_BYTES; i++) {
    if (p[i] != (uint8_t)(i + seed)) {
      printf("Chip %d: FAIL at byte %d: got 0x%02x\n", chip, i, p[i]);
      exit(1);
    }
  }
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

int main(void) {
  set_csr(mstatus, MSTATUS_VS);
  *GO_FLAG = 0;
  *DONE_FLAG = 0;
  int chip_id = reg_read64(CHIP_ID_ADDR);
  int peer_id = 3 - chip_id;
  uintptr_t peer = OFFCHIP_OFFSET * peer_id;
  uint8_t *remote_dst = (uint8_t *)((uintptr_t)DST + peer);
  uint8_t *remote_src = (uint8_t *)((uintptr_t)SRC + peer);
  volatile uint32_t *peer_go = (volatile uint32_t *)((uintptr_t)GO_FLAG + peer);
  volatile uint32_t *peer_done =
      (volatile uint32_t *)((uintptr_t)DONE_FLAG + peer);

  for (int i = 0; i < COPY_BYTES; i++)
    SRC[i] = (uint8_t)(i + chip_id);
  asm volatile("fence" ::: "memory");

#ifdef UCIE_PHY
  setup_ucie_digital(UCIE_BASE);
#endif

  program_router(0, peer_id, 0);
  idle(SETTLE_CYCLES);

  uint64_t both[N_REPEAT];
  if (chip_id == 2) {
    while (*GO_FLAG != DONE_MAGIC)
      ;
    for (int r = 0; r < N_REPEAT; r++)
      both[r] = vcopy(remote_dst, SRC, COPY_BYTES);
    report(2, "concurrent store copy", both, N_REPEAT);
    *peer_done = DONE_MAGIC;
    return 0;
  }

  uint64_t stores[N_REPEAT], loads[N_REPEAT];
  for (int r = 0; r < N_REPEAT; r++) {
    idle(1000);
    stores[r] = vcopy(remote_dst, SRC, COPY_BYTES);
  }
  for (int r = 0; r < N_REPEAT; r++) {
    idle(1000);
    loads[r] = vcopy(DST, remote_src, COPY_BYTES);
  }
  check(1, DST, 2);
  report(1, "store copy", stores, N_REPEAT);
  report(1, "load copy", loads, N_REPEAT);

  idle(1000);
  *peer_go = DONE_MAGIC;
  for (int r = 0; r < N_REPEAT; r++)
    both[r] = vcopy(remote_dst, SRC, COPY_BYTES);
  report(1, "concurrent store copy", both, N_REPEAT);
  while (*DONE_FLAG != DONE_MAGIC)
    ;
  // Chip 2's copies into chip 1 are done; check them here.
  check(1, DST, 2);
  printf("Chip %d: copy %d bytes; PASS\n", chip_id, COPY_BYTES);
  return 0;
}
