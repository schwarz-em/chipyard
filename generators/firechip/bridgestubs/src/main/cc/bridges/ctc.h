// See LICENSE for license details

#include "core/simif.h"

char ctc_t::KIND;

// Using TSI to make my life easier
struct ctcBRIDGEMODULE_struct {
  uint64_t in_bits;
  uint64_t in_valid;
  uint64_t in_ready;
  uint64_t out_bits;
  uint64_t out_valid;
  uint64_t out_ready;
};

class ctc_t : public bridge_driver_t {
public:
  /// The identifier for the bridge type used for casts.
  static char KIND;
  ctc_t::ctc_t(simif_t &simif,
                    loadmem_t &loadmem_widget,
                    const ctcBRIDGEMODULE_struct &mmio_addrs,
                    int chip0no, // YOU
                    const std::vector<std::string> &args,
                    int chip1no); // OTHER CHIP
  virtual void ctc_t::init();
  virtual void ctc_t::tick();
  virtual void ctc_t::finish();

private:
  std::string fifo0_path;
  std::string fifo1_path;
  int fifo0_fd;
  int fifo1_fd;
}
