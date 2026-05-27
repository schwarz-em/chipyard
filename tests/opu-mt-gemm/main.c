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
static int32_t c_opu_c0[M_DIM*N_DIM];

void __main(void) {
  size_t mhartid = read_csr(mhartid);
  if (mhartid >= n_cores) while (1);

  size_t maxvl;
  asm volatile("vsetvli %[vl], zero, e32, m4, ta, ma" : [vl]"=r"(maxvl));
  size_t dl = maxvl / 2;
  size_t rows_per_core = M_DIM / n_cores;

  for (size_t i = 0; i < n_cores; i++) {
    if (mhartid == i) {
      printf("Core %lu ALIVE\n", mhartid);
    }
    barrier();
  }
  
  //barrier();
  memcpy(b_c0, b, sizeof(b));
  for (size_t i = 0; i < n_cores; i++) {
    if (mhartid == i) {
      printf("Core %lu memcpy complete\n", mhartid);
    }
    barrier();
  }
  size_t m_start = mhartid * rows_per_core;
  size_t cycles_start = read_csr(mcycle);
  i8_mm_bme_2x2(c_bias, c_opu + m_start * N_DIM, at + m_start, b, rows_per_core, N_DIM, K_DIM, M_DIM);
  barrier();
  size_t cycles_end = read_csr(mcycle);
  size_t cycles_warmup = cycles_end - cycles_start;

  barrier();
  cycles_start = read_csr(mcycle);
  i8_mm_bme_2x2(c_bias, c_opu + m_start * N_DIM, at + m_start, b_c0, rows_per_core, N_DIM, K_DIM, M_DIM);
  barrier();
  cycles_end = read_csr(mcycle);
  size_t cycles_tcm = cycles_end - cycles_start;
  for (size_t i = 0; i < n_cores; i++) {
    if (mhartid == i) {
      printf("Core %lu: %lu cycles (warmup)\n", mhartid, cycles_warmup);
      printf("Core %lu: %lu cycles (tcm)\n", mhartid, cycles_tcm);
    }
    barrier();
  }

  if (mhartid == 0) {
    i8_mm_bme_2x2(c_bias, c_opu_c0, at, b_c0, M_DIM, N_DIM, K_DIM, M_DIM);
    cycles_start = read_csr(mcycle);
    i8_mm_bme_2x2(c_bias, c_opu_c0, at, b_c0, M_DIM, N_DIM, K_DIM, M_DIM);
    cycles_end = read_csr(mcycle);
    size_t cycles_c0 = cycles_end - cycles_start;
    printf("Single Core, Core %lu: %lu cycles (c0)\n", mhartid, cycles_c0);
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
