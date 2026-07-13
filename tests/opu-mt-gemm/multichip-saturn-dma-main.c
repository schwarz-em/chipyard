#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <riscv-pk/encoding.h>
#include <riscv_vector.h>
#include "bme.h"
#include "kernel.h"
#include "dataset.h"
#include "marchid.h"
#include "mmio.h"
#include "router.h"
#include "ucie.h"
#include "rvv_memcpy.h"

#define N_CHIPS         2
#define OFFCHIP_OFFSET  0x800000000L
#define SCRATCHPAD_BASE 0x08000000L
#define TCM_BASE_C0     0x70000000
#define UCIE_REG_BASE   0x8000UL

// Same structure as multichip-dma-main.c / multichip-mempress-main.c, except
// the cross-chip copy is driven by the Saturn DMA-only rocket core (see
// UcieSaturnDMASinglecoreIrisConfig / UcieSaturnDMADualIrisConfig) instead of
// a RoCC/MMIO accelerator. That core is a second hart, not a peripheral hart
// 0 can just command inline -- it has to independently reach the copy code
// itself. So hart 0 (shuttle/OPU) runs the GEMM, then hands off to
// DMA_HART_ID via a chip-local ready flag and parks; DMA_HART_ID does
// nothing until that flag is set, then vector-copies the result across UCIe
// and is the one that signals this chip's success. Tunable: -DDMA_HART_ID=<n>.
#ifndef DMA_HART_ID
#define DMA_HART_ID 1
#endif

int8_t *b_c0 = (int8_t*)TCM_BASE_C0;

// Aligned to the 32-byte beat so each transfer is naturally aligned.
static int32_t c_opu[M_DIM*N_DIM] __attribute__((aligned(64)));

// Chip-local handoff from hart 0 (GEMM result ready) to DMA_HART_ID (ship
// it). This never crosses UCIe -- it's not one of the done_flags below.
static volatile int compute_ready = 0;

// Done-flags live in chip 1's MBUS scratchpad. flags[i] is the "I'm done"
// flag for chip with chip_id == i + 2 (chip 1 is the verifier, so it has no
// flag of its own). Non-chip-1 chips write 1 to their flag via the offchip
// alias; chip 1 polls them locally.
static volatile uint64_t * const done_flags =
  (volatile uint64_t*)SCRATCHPAD_BASE;

int i32_compare(int32_t* c_opu, int32_t* c_ref, size_t m, size_t n) {
  for (size_t i = 0; i < m; i++) {
    for (size_t j = 0; j < n; j++) {
      size_t index = i * n + j;
      if (c_opu[index] != c_ref[index]) {
        printf("DIVERGENCE at index (%ld, %ld): opu=0x%x =/= ref=0x%x\n", i, j, c_opu[index], c_ref[index]);
        printf("opu:\n");
        for (size_t ii = 0; ii < m; ii++) {
          for (size_t jj = 0; jj < n; jj++) {
            printf("0x%x ", c_opu[ii*n + jj]);
          }
          printf("\n");
        }
        printf("reference:\n");
        for (size_t ii = 0; ii < m; ii++) {
          for (size_t jj = 0; jj < n; jj++) {
            printf("0x%x ", c_ref[ii*n + jj]);
          }
          printf("\n");
        }
        return 1;
      }
    }
  }
  return 0;
}

void __main(void) {
  size_t mhartid = read_csr(mhartid);
  // Router chip_id was pre-loaded by the harness via +init_write; valid
  // immediately regardless of which hart reads it or in what order.
  uint64_t chip_id = reg_read64(CHIP_ID_ADDR);

  if (mhartid == DMA_HART_ID) {
    // ============================================================
    // Saturn DMA core: ships this chip's GEMM result across UCIe.
    // Chip 1 is the verifier -- it never ships anything anywhere, so
    // its DMA core has nothing to do.
    // ============================================================
    if (chip_id == 1) while (1);

    uint64_t chip_idx      = chip_id - 1;                 // 0-indexed
    size_t   rows_per_chip = M_DIM / N_CHIPS;
    size_t   m_start       = chip_idx * rows_per_chip;
    size_t   slice_bytes   = rows_per_chip * N_DIM * sizeof(int32_t);
    size_t   slice_off     = m_start * N_DIM * sizeof(int32_t);
    void *remote_c_slice =
      (void*)((uint8_t*)c_opu + slice_off + OFFCHIP_OFFSET * 1);

    // Wait for hart 0's GEMM pass to land in c_opu.
    while (!compute_ready) ;
    __sync_synchronize();

    size_t memcpy_start = read_csr(mcycle);
    memcpy_vec(remote_c_slice, c_opu + m_start * N_DIM, slice_bytes);
    asm volatile("fence");
    size_t memcpy_end = read_csr(mcycle);

    // Order the C write before the done-flag write.
    __sync_synchronize();
    printf("Chip %lu: chiplet Saturn DMA memcpy %lu cycles\n", chip_id, memcpy_end - memcpy_start);

    // Set our done flag in chip 1's scratchpad.
    volatile uint64_t *remote_flag =
      (volatile uint64_t*)(SCRATCHPAD_BASE + (chip_id - 2) * 8
                           + OFFCHIP_OFFSET * 1);
    *remote_flag = 1;

    // Exit cleanly so this chip signals success. WithANDSuccessFn requires
    // every chip to finish, and hart 0 on this chip parked after handing
    // off -- this hart is the one that must complete. Explicit exit(0), not
    // a bare return: the crt's _start_secondary tail-calls into exit() with
    // whatever's left in a0, and a bare-string printf right before this
    // would otherwise leave iprintf's (nonzero) return value there instead
    // of a clean 0.
    printf("Chip %lu: done, exiting\n", chip_id);
    exit(0);
  }

  // ============================================================
  // Default compute core (hart 0): runs the GEMM on every chip,
  // including chip 1 (the verifier).
  // ============================================================
  if (mhartid != 0) while (1); // any hart besides 0 and DMA_HART_ID parks

  uint64_t chip_idx      = chip_id - 1;                 // 0-indexed
  size_t   rows_per_chip = M_DIM / N_CHIPS;
  size_t   m_start       = chip_idx * rows_per_chip;

  printf("Chip %lu starting\n", chip_id);

  // Bring up this chip's UCIe link (PHY bypass clocks, driver/skew control,
  // FSM reset, then switch the mainband to TileLink mode) before any
  // chip-to-chip traffic crosses the link. DMA_HART_ID waits on
  // compute_ready before touching the link, so a single setup here suffices.
  setup_ucie(UCIE_REG_BASE);

  // Chip 1 clears the done flags before peers can have routed any traffic
  // here (peers haven't programmed their router yet, and even after they do
  // they spend many millions of cycles computing before signaling done).
  if (chip_id == 1) {
    for (size_t i = 0; i < N_CHIPS - 1; i++) done_flags[i] = 0;
  }

  // Program a single routing-table entry so this chip can reach the peer
  // via port 0 (matches the existing router/ring tests).
  uint64_t peer_id = (chip_id == 1) ? 2 : 1;
  program_router(/*table_entry=*/0, /*chip_id=*/peer_id, /*port=*/0);

  // Initialize the vector unit (the kernel reads maxvl via vsetvli).
  size_t maxvl;
  asm volatile("vsetvli %[vl], zero, e32, m4, ta, ma" : [vl]"=r"(maxvl));

  // Stage B in local TCM.
  memcpy(b_c0, b, sizeof(b));

  // Warmup pass (primes caches; also exercises the kernel).
  i8_mm_bme_2x2(c_bias, c_opu + m_start * N_DIM, at + m_start,
                b_c0, rows_per_chip, N_DIM, K_DIM, M_DIM);

  // Timed pass — each chip computes only its M-slice of C.
  size_t cycles_start = read_csr(mcycle);
  i8_mm_bme_2x2(c_bias, c_opu + m_start * N_DIM, at + m_start,
                b_c0, rows_per_chip, N_DIM, K_DIM, M_DIM);
  size_t cycles_end = read_csr(mcycle);
  printf("Chip %lu: %lu cycles\n", chip_id, cycles_end - cycles_start);

  if (chip_id != 1) {
    // Hand off to the Saturn DMA core and park. DMA_HART_ID does the
    // cross-chip copy and signals this chip's success itself; hart 0 must
    // not also race to exit(), or this chip's tohost could fire before the
    // copy (and the peer's verify) actually happens.
    __sync_synchronize();
    compute_ready = 1;
    while (1);
  }

  // Chip 1: wait for every peer to signal done.
  for (size_t i = 0; i < N_CHIPS - 1; i++) {
    while (done_flags[i] == 0) ;
  }
  __sync_synchronize();

  int r = i32_compare(c_opu, verify_data, M_DIM, N_DIM);
  if (r) {
    printf("FAILURE; M, N, K = %d %d %d\n", M_DIM, N_DIM, K_DIM);
    exit(1);
  }
  printf("SUCCESS; M, N, K = %d %d %d\n", M_DIM, N_DIM, K_DIM);
  exit(0);
}

int main(void) {
  __main();
  return 0;
}
