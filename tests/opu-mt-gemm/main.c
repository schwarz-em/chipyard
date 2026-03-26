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

static size_t n_cores = 2;

static void __attribute__((noinline)) barrier()
{
  static volatile int sense;
  static volatile int count;
  static __thread int threadsense;

  __sync_synchronize();

  threadsense = !threadsense;
  if (__sync_fetch_and_add(&count, 1) == n_cores-1)
  {
    count = 0;
    sense = threadsense;
  }
  else while(sense != threadsense)
    ;

  __sync_synchronize();
}

void i8_mm_scalar(int32_t* c_bias, int32_t* c_out, int8_t* at, int8_t* b, size_t M, size_t N, size_t K, size_t lda) {
  for (size_t i = 0; i < M; i++) {
    for (size_t j = 0; j < N; j++) {
      c_out[i*N+j] = c_bias[j];
      for (size_t k = 0; k < K; k++) {
        c_out[i*N+j] += at[k*lda+i] * b[k*N+j];
      }
    }
  }
}

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
#define TCM_BASE_C0 0x70000000
// #define TCM_BASE_C1 0x70020000
int8_t *b_c0 = (int8_t*)TCM_BASE_C0;
// int8_t at_c1[M_DIM*K_DIM] = TCM_BASE_C1;

static int32_t c_opu[M_DIM*N_DIM];

void __main(void) {
  size_t mhartid = read_csr(mhartid);
  if (mhartid >= n_cores) while (1);

  size_t maxvl;
  asm volatile("vsetvli %[vl], zero, e32, m4, ta, ma" : [vl]"=r"(maxvl));
  size_t dl = maxvl / 2;
  size_t rows_per_core = 2*maxvl;

  barrier();
  for (size_t i = 0; i < n_cores; i++) {
    if (mhartid == i) {
      printf("Hello world from core %lu\n", mhartid);
      printf("maxvl=%lu; dl=%lu; rows_per_core=%lu\n", maxvl, dl, rows_per_core);
      printf("Testing M=%d, N=%d, K=%d\n", M_DIM, N_DIM, K_DIM);
      memcpy(b_c0, b, sizeof(b));
    
    }
    barrier();
  }

  barrier();
  size_t cycles_start = read_csr(mcycle);
  for (size_t m_base = 0; m_base < M_DIM; m_base += n_cores * rows_per_core) {
    size_t m_start = m_base + mhartid * rows_per_core;
    size_t m_end = m_start + rows_per_core;
    if (m_end > M_DIM) m_end = M_DIM;
    if (m_start < M_DIM) {
      i8_mm_bme_2x2(c_bias, c_opu + m_start * N_DIM, at + m_start, b, m_end - m_start, N_DIM, K_DIM, M_DIM);
      // i8_mm_scalar(c_bias, c_opu + m_start * N_DIM, at + m_start, b, m_end - m_start, N_DIM, K_DIM, M_DIM);
    }
    barrier();
  }
  size_t cycles_end = read_csr(mcycle);
  size_t cycles = cycles_end - cycles_start;
  for (size_t i = 0; i < n_cores; i++) {
    if (mhartid == i) {
      printf("Core %lu: %lu cycles\n", mhartid, cycles);
    }
    barrier();
  }
  barrier();
  cycles_start = read_csr(mcycle);
  for (size_t m_base = 0; m_base < M_DIM; m_base += n_cores * rows_per_core) {
    size_t m_start = m_base + mhartid * rows_per_core;
    size_t m_end = m_start + rows_per_core;
    if (m_end > M_DIM) m_end = M_DIM;
    if (m_start < M_DIM) {
      i8_mm_bme_2x2(c_bias, c_opu + m_start * N_DIM, at + m_start, b_c0, m_end - m_start, N_DIM, K_DIM, M_DIM);
      // i8_mm_scalar(c_bias, c_opu + m_start * N_DIM, at + m_start, b, m_end - m_start, N_DIM, K_DIM, M_DIM);
    }
    barrier();
  }
  cycles_end = read_csr(mcycle);
  cycles = cycles_end - cycles_start;
  for (size_t i = 0; i < n_cores; i++) {
    if (mhartid == i) {
      printf("Core %lu: %lu cycles\n", mhartid, cycles);
    }
    barrier();
  }

  if (mhartid == 0) {
    int r = i32_compare(c_opu, verify_data, M_DIM, N_DIM);
    if (r) {
      printf("FAILURE; M, N, K = %d %d %d\n", M_DIM, N_DIM, K_DIM);
      exit(1);
    }
    printf("SUCCESS; M, N, K = %d %d %d\n", M_DIM, N_DIM, K_DIM);
  }

  if (mhartid > 0) while (1);
}

int main(void) {
  __main();
  return 0;
}
