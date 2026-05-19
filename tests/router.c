#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <riscv-pk/encoding.h>
#include "marchid.h"
#include "router.h"

#define OFFCHIP_OFFSET 0x200000000L

uint32_t src[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
uint32_t dest[10];
uint32_t test[10];

int rw_mem(uint64_t offset) {
  // remote = dest on the other chip: same local address as our dest, tagged with target chip ID.
  // offset = OFFCHIP_OFFSET * target_chipId (a byte offset, added via uint8_t* cast).
  void *remote = (void *)((uint8_t *)dest + offset);

  size_t write_start = rdcycle();
  memcpy(remote, src, sizeof(src));
  size_t write_end = rdcycle();

  printf("Wrote %ld bytes in %ld cycles\n", sizeof(src), write_end - write_start);

  size_t read_start = rdcycle();
  memcpy(test, remote, sizeof(src));
  size_t read_end = rdcycle();

  for (int i = 0; i < 10; i++) {
      if (src[i] != test[i]) {
        printf("Remote write/read failed at index %d: wrote %x, read %x\n", i, src[i], test[i]);
        exit(1);
      }
  }

  printf("Read %ld bytes in %ld cycles\n", sizeof(src), read_end - read_start);

  return 0;
}

int main(void) {

  int chip_id = reg_read64(CHIP_ID_ADDR);

  printf("Got chip ID: %d\n", chip_id);

  if (chip_id == 1) {
    program_router(0, 2, 0);
    rw_mem(OFFCHIP_OFFSET * 2);
    printf("Chip 1 DONE\n");
  } else {
    program_router(0, 1, 0);
    rw_mem(OFFCHIP_OFFSET * 1);
    printf("Chip 2 DONE\n");
  }

  return 0;
}
