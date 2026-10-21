// See LICENSE for license details

#ifndef __D2D_LINK_H
#define __D2D_LINK_H

#include "core/bridge_driver.h"
#include "core/stream_engine.h"

#include <cstdint>
#include <string>
#include <vector>

// Placeholder MMIO register (see D2DLinkBridgeModule)
struct D2DLINKBRIDGEMODULE_struct {
  uint64_t done;
};

/**
 * Host driver for D2DLinkBridgeModule: exchanges per-cycle link tokens with the
 * peer chip's driver through a pair of named FIFOs, one per direction.
 *
 * Every FireSim node runs the same plusargs, so they are keyed by node id and
 * link index (the bridge's instance number within the node):
 *
 *   +d2d_fifodir=<dir>              directory for the FIFOs (shared by all nodes)
 *   +d2d_latency=<cycles>           link latency for every link (default:
 *                                   tokens_per_beat cycles, i.e. one beat)
 *   +d2d_latency_<node>_<link>=<c>  per-link override
 *   +d2d_peer_<node>_<link>=<peer_node>:<peer_link>
 *   +d2d_peer_<node>_<link>=loopback   feed this link's tx back to its rx
 *
 * The node id comes from the +macaddr0 the FireSim manager assigns each node
 * (last octet - 2), as in the CTC driver.
 *
 * The latency must be a positive multiple of tokens_per_beat, and
 * latency / tokens_per_beat beats must fit in the from-host stream queue.
 */
class d2d_link_t final : public streaming_bridge_driver_t {
public:
  static char KIND;
  d2d_link_t(simif_t &simif,
             StreamEngine &stream,
             const D2DLINKBRIDGEMODULE_struct &mmio_addrs,
             int link_idx,
             const std::vector<std::string> &args,
             int stream_to_cpu_idx,
             int stream_to_cpu_depth,
             int stream_from_cpu_idx,
             int stream_from_cpu_depth,
             uint32_t token_bits,
             uint32_t tokens_per_beat);
  ~d2d_link_t() override;

  void init() override;
  void tick() override;
  void finish() override;

private:
  const D2DLINKBRIDGEMODULE_struct mmio_addrs;
  const int link_idx;
  const int stream_to_cpu_idx;
  const int stream_from_cpu_idx;
  const int stream_from_cpu_depth;
  const uint32_t token_bits;
  const uint32_t tokens_per_beat;

  int node_id = -1;
  int peer_node = -1;
  int peer_link = -1;
  bool loopback = false;
  uint64_t latency = 0;      // in target cycles
  size_t batch_beats = 0;    // latency / tokens_per_beat
  size_t batch_bytes = 0;

  std::string rx_fifo_path; // peer -> me
  std::string tx_fifo_path; // me -> peer
  int rx_fd = -1;
  int tx_fd = -1;
  bool rx_connected = false; // peer has written at least once

  char *buf = nullptr;

  std::string prefix() const;
  void open_fifos();
};

#endif // __D2D_LINK_H
