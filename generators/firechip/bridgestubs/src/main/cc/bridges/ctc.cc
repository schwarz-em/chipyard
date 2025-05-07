// See LICENSE for license details

#include "ctc.h"
#include "core/simif.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <cassert>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

char ctc_t::KIND;

// "Serial" tilelink

ctc_t::ctc_t(simif_t &simif,
                    const CTCBRIDGEMODULE_struct &mmio_addrs,
                    int chipno, // maybe this is a stupid naming scheme
                    const std::vector<std::string> &args)
    : bridge_driver_t(simif, &KIND), mmio_addrs(mmio_addrs) {

  // Read plusargs
  const std::string num_equals = std::to_string(chipno) + std::string("=");
  //const std::string num_equals1 = std::to_string(chip1no) + std::string("=");
  const std::string chip1_arg = std::string("+connectid") + num_equals;
  int chip1no = 0;

  for (auto &arg : args) {
    if(arg.find(chip1_arg) == 0) {
      chip1no = std::stoi(arg.c_str());
      printf("CHIP%d: got connectid %d", chipno, chip1no);
    }
  }

  const std::string chip0fifo_arg = std::string("+fifofile") + num_equals;
  const std::string chip1fifo_arg = std::string("+fifofile") + std::to_string(chip1no) + std::string("=");

  fifo0_path = "";
  fifo1_path = "";
  // fifo0_fd = 0
  // fifo1_fd = 0

  for (auto &arg : args) {
    if(arg.find(chip0fifo_arg) == 0) {
      fifo0_path = arg.c_str();
    }
    if(arg.find(chip1fifo_arg) == 0) {
      fifo1_path = arg.c_str();
    }
  }

  printf("CHIP%d: got fifo0 file %s", chipno, chip0fifo_arg);
  printf("CHIP%d: got fifo1 file %s", chipno, chip1fifo_arg);
}

// Uh idk if I need this anymore...
void ctc_t::init() {
  // Open my fifo as RO
  fifo0_fd = open(fifo0_path.c_str(), O_RDONLY);
  // Open the other chip's fifo as WO
  fifo1_fd = open(fifo1_path.c_str(), O_WRONLY);

  assert(fifo0_fd != -1 && "fifofile0 couldn't be opened");
  assert(fifo1_fd != -1 && "fifofile1 couldn't be opened");
}

void ctc_t::tick() {
  // Read RO (or "output") mmios and cast everything into a char array, should be 32b+32b+32b = 12 chars total
  // ALWAYS write because the other chip must read from my fifo. This is the easiest way to synchronize.
  uint32_t buf_out[6];
  buf_out[0] = bridge_driver_t::read(mmio_addrs.client_out_valid);
  buf_out[1] = bridge_driver_t::read(mmio_addrs.client_in_ready);
  buf_out[2] = bridge_driver_t::read(mmio_addrs.client_out_bits);
  buf_out[3] = bridge_driver_t::read(mmio_addrs.manager_out_valid);
  buf_out[4] = bridge_driver_t::read(mmio_addrs.manager_in_ready);
  buf_out[5] = bridge_driver_t::read(mmio_addrs.manager_out_bits);

  // Write "out" char array to the other chips's fifo
  int bytes_written = ::write(fifo1_fd, buf_out, sizeof(buf_out));
  if (bytes_written != sizeof(buf_out)) {
    printf("it's so over. writing to fifo failed.");
    exit(1);
  }

  // Read my own fifo until I read all the "in" chars
  uint32_t buf_in[6];
  int bytes_read = ::read(fifo0_fd, buf_in, sizeof(buf_in));
  if (bytes_read != sizeof(buf_in)) {
    printf("it's so over. reading from fifo failed.");
    exit(1);
  }

  // Write "in" chars to mmio
  bridge_driver_t::write(mmio_addrs.client_in_valid,   buf_in[0]);
  bridge_driver_t::write(mmio_addrs.client_out_ready,  buf_in[1]);
  bridge_driver_t::write(mmio_addrs.client_in_bits,    buf_in[2]);
  bridge_driver_t::write(mmio_addrs.manager_in_valid,  buf_in[3]);
  bridge_driver_t::write(mmio_addrs.manager_out_ready, buf_in[4]);
  bridge_driver_t::write(mmio_addrs.manager_in_bits,   buf_in[5]);
}

void ctc_t::finish() {
  // Close the fifos
  close(fifo0_fd);
  close(fifo1_fd);
}


