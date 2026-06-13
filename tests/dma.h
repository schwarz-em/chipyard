// Minimal driver for the dsp25 DMA engine (CPU-channel, mem-to-mem, polling).
// Mirrors the register sequence of generators/dsp25-audio/.../dma_utils.c
// set_DMA_C/start_DMA, but drops the cross-core busy lock (single programmer)
// and the interrupt/PLIC path in favor of polling DMA_INFLIGHT_STATUS.

#ifndef __DMA_H__
#define __DMA_H__

#include <stdint.h>
#include <riscv-pk/encoding.h>
#include "mmio.h"

#define DMA_MMIO_BASE        0x8810000UL
#define DMA_RESET            (DMA_MMIO_BASE)
#define DMA_INFLIGHT_STATUS  (DMA_MMIO_BASE + 0x1)

// Channel 0 register block (CHANNEL_BASE = DMA_MMIO_BASE + 0x100).
#define DMA_CH0              (DMA_MMIO_BASE + 0x100)
#define DMA_START            (DMA_CH0 + 0x00)
#define DMA_CORE_ID          (DMA_CH0 + 0x04)
#define DMA_TRANSACTION_ID   (DMA_CH0 + 0x08)
#define DMA_PRIORITY         (DMA_CH0 + 0x0C)
#define DMA_MODE             (DMA_CH0 + 0x0E)
#define DMA_ADDR_R           (DMA_CH0 + 0x10)
#define DMA_ADDR_W           (DMA_CH0 + 0x18)
#define DMA_LEN              (DMA_CH0 + 0x20)
#define DMA_LOGW             (DMA_CH0 + 0x22)
#define DMA_INC_R            (DMA_CH0 + 0x24)
#define DMA_INC_W            (DMA_CH0 + 0x26)

// Program and launch a contiguous read->write copy on CPU channel 0.
// len_bytes must be a multiple of (1 << logw).
static inline void dma_c_memcpy(uint64_t addr_r, uint64_t addr_w,
                                uint16_t len_bytes, uint8_t logw) {
  uint16_t width = (uint16_t)(1u << logw);
  reg_write8 (DMA_CORE_ID,        (uint8_t)read_csr(mhartid));
  reg_write16(DMA_TRANSACTION_ID, 0);
  reg_write8 (DMA_PRIORITY,       1);
  reg_write8 (DMA_LOGW,           logw);
  reg_write64(DMA_ADDR_R,         addr_r);
  reg_write64(DMA_ADDR_W,         addr_w);
  reg_write16(DMA_LEN,            len_bytes / width);
  reg_write16(DMA_INC_R,          width);
  reg_write16(DMA_INC_W,          width);
  reg_write8 (DMA_MODE,           0);  // R->W, no interrupt, no address gate
  reg_write8 (DMA_START,          1);
}

static inline int dma_inflight(void) {
  return reg_read8(DMA_INFLIGHT_STATUS);
}

// Spin until the DMA has been idle (no in-flight requests) for `settle`
// consecutive polls, matching dma_utils.c's dma_wait_till_inactive.
static inline void dma_wait_inactive(int settle) {
  while (1) {
    volatile int t = 0;
    while (t < settle && dma_inflight() == 0) t++;
    if (t == settle) break;
  }
}

static inline void dma_reset(void) {
  reg_write8(DMA_RESET, 1);
}

#endif // __DMA_H__
