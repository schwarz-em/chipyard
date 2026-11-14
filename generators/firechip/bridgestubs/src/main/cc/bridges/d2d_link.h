// See LICENSE for license details

#ifndef __D2D_LINK_H
#define __D2D_LINK_H

#include "core/bridge_driver.h"
#include "core/stream_engine.h"

#include <cstdint>
#include <string>
#include <utility>
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
 *
 * Runtime latency switch (fast boot, then real latency):
 *
 *   +d2d_latency_switch=<cycle>:<L>[,<cycle>:<L>...]
 *   +d2d_latency_switch_<node>_<link>=...   per-link override
 *
 * The latency sets simulation speed (the drivers exchange one latency-sized
 * batch per host round trip), so a run can boot at a high latency and switch
 * to the real one before the link is used. <cycle> counts this link's tokens
 * (D2D link clock cycles since the start of simulation, reset included); it is
 * the same on every node, because the link runs in lockstep.
 *
 * Both ends of a link must use the same schedule, and loopback links follow
 * theirs too. Since each side sends a whole batch and then reads the peer's,
 * each switch must land on a batch boundary of the latency before it:
 * (cycle - previous switch cycle, or 0) % previous latency == 0. When the
 * switch fires, rx is already queued through cycle s + L1 - 1.
 *   - Raise L1 -> L2: push L2 - L1 idle tokens, then use batch L2. Nothing is
 *     lost, and peer tx t arrives at rx t + L1 (t < s) or t + L2 (t >= s).
 *   - Lower L1 -> L2: the peer's tokens for cycles [s, s + L1 - L2) are
 *     DROPPED (their rx slots are already taken by old-latency tokens); peer
 *     tx t >= s + L1 - L2 arrives at rx t + L2. Neither target stalls, since
 *     each still has L1 cycles of queued rx when the switch fires. Every
 *     dropped token's bit 0 (the valid bit of a UCIe token, see
 *     UcieLinkAdapter) is checked, and the simulator fails if one is set. So
 *     the drop check is only meaningful for UCIe links. The next switch must
 *     come after the drop window ends.
 * Every L in the schedule obeys the same rules as +d2d_latency.
 *
 * Software contract: neither side may put a valid frame on the link in the
 * drop window [s, s + L1 - L2). UCIe credits travel in-band, so a lost frame
 * corrupts data or loses credits, and the link hangs later. Responses and
 * credit returns count: every transaction must complete before s, so the last
 * request goes out before s - (2*L1 + 35 + margin), since the UCIe RTT is
 * 2L+35. In practice, keep the link unused from boot until after
 * s + L1 - L2 + margin: one side waits on its
 * local cycle counter until past the window, then initializes the router and
 * stores a ready flag remotely, while the other side polls locally. Note that
 * <cycle> is in link clock cycles, which may differ from the core's mcycle.
 */
class d2d_link_t final : public streaming_bridge_driver_t {
public:
  static char KIND;
  d2d_link_t(simif_t &simif,
             HostStreamEngine &stream,
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
  uint64_t latency = 0;      // current latency, in target cycles
  size_t batch_beats = 0;    // latency / tokens_per_beat
  size_t batch_bytes = 0;
  size_t max_batch_bytes = 0; // over the initial latency and the schedule

  // Latency switch schedule (+d2d_latency_switch): (cycle, new latency).
  std::vector<std::pair<uint64_t, uint64_t>> schedule;
  size_t next_switch = 0;
  uint64_t tx_cycles = 0;      // tokens pulled from the target so far
  uint64_t drop_remaining = 0; // received tokens still to drop (lowering)
  uint64_t drop_start = 0, drop_end = 0; // cycles of the current drop window
  size_t pad_bytes = 0;        // idle tokens still to push (raising)
  char *zero_buf = nullptr;    // max_batch_bytes of zeros, for padding
  // Part of `buf` to hand to the target: dropping skips its first tokens.
  size_t push_off = 0;
  size_t push_bytes = 0;

  std::string rx_fifo_path; // peer -> me
  std::string tx_fifo_path; // me -> peer
  int rx_fd = -1;
  int tx_fd = -1;
  bool rx_connected = false; // peer has written at least once
  // A batch was sent to the peer and the peer's batch for the same cycles is
  // still arriving into rx_buf (rx_done bytes so far). The read is resumed on
  // later tick()s instead of blocking, so a node never waits on one link while
  // a peer waits on another of its links (rings would deadlock).
  bool awaiting_rx = false;
  size_t rx_done = 0;
  // The peer exited (EOF on rx or EPIPE on tx). Nodes finish at slightly
  // different times, so a node that is still running sees its peer leave
  // after the final barrier. From then on the link carries idle (zero)
  // tokens so this node can reach its own exit; if it is still running
  // closed_grace_cycles later, the peer most likely died mid-run, so fail.
  bool peer_closed = false;
  // Heartbeat (+d2d_heartbeat=<seconds>, default 30, 0 = off): every period of
  // host wall time, print this link's batch and non-idle beat counters, so a
  // stuck multi-node run shows whether tokens still flow on each link.
  unsigned heartbeat_secs = 30;
  uint64_t hb_last_ns = 0;
  uint64_t tx_batches = 0, rx_batches = 0;       // batches sent to / received from the peer
  uint64_t tx_busy_beats = 0, rx_busy_beats = 0; // beats with any non-zero byte (non-idle)
  uint64_t push_stalls = 0, pull_empty = 0;      // ticks blocked on the target side
  void heartbeat();
  static uint64_t count_busy_beats(const char *b, size_t beats);
  uint64_t closed_cycles = 0;
  uint64_t closed_grace_cycles = 200000000; // +d2d_closed_grace=<cycles>

  // `buf` holds the batch being sent to the target; `rx_buf` receives the
  // peer's batch while `buf` holds mine, then the two swap.
  char *buf = nullptr;
  char *rx_buf = nullptr;
  // A batch is waiting in `buf` for the from-host stream to have room. With
  // FPGA-managed (F2 PCIM) streams, push() returns 0 under H2C backpressure
  // instead of blocking, so retry it on the next tick.
  bool pending_push = false;
  // Consecutive tick()s with no full batch to pull. The F2 C2H engine hands
  // data to the host in whole 4 KiB pages; a batch that ends mid-page is only
  // released by pull_flush(), so call it after a few empty polls.
  unsigned empty_pull_polls = 0;
  static constexpr unsigned flush_poll_limit = 8;
  // Batches moved per tick(), so other bridges still get serviced.
  static constexpr unsigned max_batches_per_tick = 4;

  std::string prefix() const;
  void open_rx_fifo();
  void open_tx_fifo();
  void send_to_peer();
  bool recv_from_peer();
  void mark_peer_closed();
  void set_latency(uint64_t l);
  void check_latency(uint64_t l, const char *what) const;
  void parse_schedule(const std::string &spec);
  void apply_switch();
  void deliver_rx();
};

#endif // __D2D_LINK_H
