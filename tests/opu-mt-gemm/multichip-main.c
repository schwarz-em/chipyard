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

#define N_CHIPS         2
#define OFFCHIP_OFFSET  0x800000000L
#define SCRATCHPAD_BASE 0x08000000L
#define TCM_BASE_C0     0x70000000

int8_t *b_c0 = (int8_t*)TCM_BASE_C0;

static int32_t c_opu[M_DIM*N_DIM];

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
  // Park every hart except hart 0.
  size_t mhartid = read_csr(mhartid);
  if (mhartid != 0) while (1);

  uint64_t chip_id  = reg_read64(CHIP_ID_ADDR);
  uint64_t chip_idx = chip_id - 1;                 // 0-indexed
  size_t   rows_per_chip = M_DIM / N_CHIPS;
  size_t   m_start       = chip_idx * rows_per_chip;

  printf("Chip %lu starting\n", chip_id);

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
    // Ship this chip's M-slice of c_opu into chip 1's c_opu over the
    // chiplet link, using the standard offchip-alias trick.
    size_t slice_bytes = rows_per_chip * N_DIM * sizeof(int32_t);
    size_t slice_off   = m_start * N_DIM * sizeof(int32_t);
    void *remote_c_slice =
      (void*)((uint8_t*)c_opu + slice_off + OFFCHIP_OFFSET * 1);
    size_t memcpy_start = read_csr(mcycle);
    memcpy(remote_c_slice, c_opu + m_start * N_DIM, slice_bytes);

    // Order the C write before the done-flag write.
    __sync_synchronize();
    size_t memcpy_end = read_csr(mcycle);
    printf("Chip %lu: chiplet memcpy %lu cycles\n", chip_id, memcpy_end - memcpy_start);

    // Set our done flag in chip 1's scratchpad.
    volatile uint64_t *remote_flag =
      (volatile uint64_t*)(SCRATCHPAD_BASE + (chip_id - 2) * 8
                           + OFFCHIP_OFFSET * 1);
    *remote_flag = 1;

    // Park; chip 1 owns simulation termination.
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
}

int main(void) {
  __main();
  return 0;
}
