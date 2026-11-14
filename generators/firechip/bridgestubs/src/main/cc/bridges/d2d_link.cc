// See LICENSE for license details

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // F_SETPIPE_SZ
#endif

#include "d2d_link.h"
#include "core/simif.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

char d2d_link_t::KIND;

#define BEAT_BYTES streaming_bridge_driver_t::STREAM_WIDTH_BYTES

namespace {

bool find_arg(const std::vector<std::string> &args,
              const std::string &key,
              std::string &value) {
  const std::string prefix = "+" + key + "=";
  for (auto &arg : args) {
    if (arg.rfind(prefix, 0) == 0) {
      value = arg.substr(prefix.size());
      return true;
    }
  }
  return false;
}

[[noreturn]] void fail(const std::string &who, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "%s ", who.c_str());
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}

std::string fifo_name(const std::string &dir,
                      int src_node,
                      int src_link,
                      int dst_node,
                      int dst_link) {
  return dir + "/d2d_" + std::to_string(src_node) + "_" +
         std::to_string(src_link) + "_to_" + std::to_string(dst_node) + "_" +
         std::to_string(dst_link);
}

} // namespace

std::string d2d_link_t::prefix() const {
  return "[D2D node " + std::to_string(node_id) + " link " +
         std::to_string(link_idx) + "]";
}

d2d_link_t::d2d_link_t(simif_t &simif,
                       HostStreamEngine &stream,
                       const D2DLINKBRIDGEMODULE_struct &mmio_addrs,
                       int link_idx,
                       const std::vector<std::string> &args,
                       int stream_to_cpu_idx,
                       int stream_to_cpu_depth,
                       int stream_from_cpu_idx,
                       int stream_from_cpu_depth,
                       uint32_t token_bits,
                       uint32_t tokens_per_beat)
    : streaming_bridge_driver_t(simif, stream, &KIND), mmio_addrs(mmio_addrs),
      link_idx(link_idx), stream_to_cpu_idx(stream_to_cpu_idx),
      stream_from_cpu_idx(stream_from_cpu_idx),
      stream_from_cpu_depth(stream_from_cpu_depth), token_bits(token_bits),
      tokens_per_beat(tokens_per_beat) {
  std::string v;

  // Node id. Every node gets the same plusarg string, so the per-node
  // distinction comes from the MAC address the FireSim manager assigns each
  // node (00:12:6D:00:00:02 for node 0, ...). To launch a simulator by hand,
  // pass it a +macaddr0 the same way.
  if (!find_arg(args, "macaddr0", v)) {
    fail("[D2D]", "no +macaddr0 to identify this node");
  }
  unsigned int oct[6];
  char junk;
  if (sscanf(v.c_str(),
             "%x:%x:%x:%x:%x:%x%c",
             &oct[0],
             &oct[1],
             &oct[2],
             &oct[3],
             &oct[4],
             &oct[5],
             &junk) != 6) {
    fail("[D2D]", "invalid +macaddr0=%s", v.c_str());
  }
  node_id = static_cast<int>(oct[5]) - 2;

  const std::string key =
      std::to_string(node_id) + "_" + std::to_string(link_idx);

  // Peer
  if (!find_arg(args, "d2d_peer_" + key, v)) {
    fail(prefix(),
         "no +d2d_peer_%s=<node>:<link> (or =loopback)",
         key.c_str());
  }
  if (v == "loopback") {
    loopback = true;
  } else if (sscanf(v.c_str(), "%d:%d", &peer_node, &peer_link) != 2) {
    fail(prefix(), "invalid +d2d_peer_%s=%s", key.c_str(), v.c_str());
  }

  // Latency
  latency = tokens_per_beat;
  if (find_arg(args, "d2d_latency_" + key, v) ||
      find_arg(args, "d2d_latency", v)) {
    latency = std::stoull(v, nullptr, 0);
  }
  check_latency(latency, "latency");
  set_latency(latency);
  max_batch_bytes = batch_bytes;
  if (find_arg(args, "d2d_latency_switch_" + key, v) ||
      find_arg(args, "d2d_latency_switch", v)) {
    parse_schedule(v);
  }
  if (find_arg(args, "d2d_heartbeat", v)) {
    heartbeat_secs = std::stoul(v, nullptr, 0);
  }
  if (find_arg(args, "d2d_closed_grace", v)) {
    closed_grace_cycles = std::stoull(v, nullptr, 0);
  }

  // FIFOs
  if (!loopback) {
    std::string dir;
    if (!find_arg(args, "d2d_fifodir", dir)) {
      fail(prefix(), "no +d2d_fifodir=<dir>");
    }
    rx_fifo_path = fifo_name(dir, peer_node, peer_link, node_id, link_idx);
    tx_fifo_path = fifo_name(dir, node_id, link_idx, peer_node, peer_link);
  }

  printf("%s %u-bit tokens, %u per beat, latency %lu cycles (%zu beats), %s\n",
         prefix().c_str(),
         token_bits,
         tokens_per_beat,
         latency,
         batch_beats,
         loopback ? "loopback"
                  : ("peer node " + std::to_string(peer_node) + " link " +
                     std::to_string(peer_link))
                        .c_str());

  for (auto &sw : schedule) {
    printf("%s latency switch scheduled at link cycle %lu: -> %lu cycles\n",
           prefix().c_str(),
           sw.first,
           sw.second);
  }

  for (char **b : {&buf, &rx_buf, &zero_buf}) {
    if (posix_memalign(
            reinterpret_cast<void **>(b), BEAT_BYTES, max_batch_bytes) != 0) {
      fail(prefix(), "cannot allocate %zu-byte batch buffer", max_batch_bytes);
    }
    memset(*b, 0, max_batch_bytes);
  }
}

d2d_link_t::~d2d_link_t() {
  free(buf);
  free(rx_buf);
  free(zero_buf);
}

void d2d_link_t::check_latency(uint64_t l, const char *what) const {
  if (l == 0 || l % tokens_per_beat != 0) {
    fail(prefix(),
         "%s %lu must be a positive multiple of %u (tokens per beat)",
         what,
         l,
         tokens_per_beat);
  }
  if (l / tokens_per_beat > static_cast<uint64_t>(stream_from_cpu_depth)) {
    fail(prefix(),
         "%s %lu needs %lu beats in flight, but the from-host stream holds %d",
         what,
         l,
         l / tokens_per_beat,
         stream_from_cpu_depth);
  }
}

void d2d_link_t::set_latency(uint64_t l) {
  latency = l;
  batch_beats = l / tokens_per_beat;
  batch_bytes = batch_beats * BEAT_BYTES;
}

// <cycle>:<L>[,<cycle>:<L>...]; see the header comment for the rules.
void d2d_link_t::parse_schedule(const std::string &spec) {
  uint64_t prev_cycle = 0, prev_l = latency, drop_until = 0;
  size_t pos = 0;
  while (pos <= spec.size()) {
    size_t end = spec.find(',', pos);
    if (end == std::string::npos)
      end = spec.size();
    const std::string item = spec.substr(pos, end - pos);
    unsigned long long c, l;
    char junk;
    if (sscanf(item.c_str(), "%llu:%llu%c", &c, &l, &junk) != 2) {
      fail(prefix(),
           "invalid +d2d_latency_switch entry '%s' (want <cycle>:<latency>)",
           item.c_str());
    }
    check_latency(l, "switch latency");
    if (!schedule.empty() && c <= prev_cycle) {
      fail(prefix(), "latency switch cycles must increase (%llu)", c);
    }
    if ((c - prev_cycle) % prev_l != 0) {
      fail(prefix(),
           "latency switch at cycle %llu is not on a batch boundary: "
           "(%llu - %lu) %% %lu != 0",
           c,
           c,
           prev_cycle,
           prev_l);
    }
    if (c < drop_until) {
      fail(prefix(),
           "latency switch at cycle %llu falls inside the previous switch's "
           "drop window (ends at cycle %lu)",
           c,
           drop_until);
    }
    if (l < prev_l)
      drop_until = c + (prev_l - l);
    schedule.emplace_back(c, l);
    max_batch_bytes =
        std::max(max_batch_bytes, size_t(l / tokens_per_beat) * BEAT_BYTES);
    prev_cycle = c;
    prev_l = l;
    pos = end + 1;
  }
}

// Called with rx queued through cycle tx_cycles + latency - 1, before the
// batch starting at tx_cycles is pulled.
void d2d_link_t::apply_switch() {
  const uint64_t s = schedule[next_switch].first;
  const uint64_t l2 = schedule[next_switch].second;
  const uint64_t l1 = latency;
  ++next_switch;
  if (l2 > l1) {
    pad_bytes = (l2 - l1) / tokens_per_beat * BEAT_BYTES;
    printf("%s latency switch at link cycle %lu: %lu -> %lu (%lu idle tokens "
           "padded)\n",
           prefix().c_str(),
           s,
           l1,
           l2,
           l2 - l1);
  } else if (l2 < l1) {
    drop_remaining = l1 - l2;
    drop_start = s;
    drop_end = s + drop_remaining;
    printf("%s latency switch at link cycle %lu: %lu -> %lu (dropping %s tx "
           "for link cycles [%lu, %lu); each must be idle)\n",
           prefix().c_str(),
           s,
           l1,
           l2,
           loopback ? "loopback" : "peer",
           drop_start,
           drop_end);
  }
  fflush(stdout);
  set_latency(l2);
}

// `buf` holds a received batch (the peer's tokens for link cycles
// [tx_cycles - latency, tx_cycles)). Drop its head if a lowering switch is
// still discarding tokens, and mark the rest for pushing to the target.
void d2d_link_t::deliver_rx() {
  uint64_t drop = std::min<uint64_t>(drop_remaining, latency);
  const uint64_t first = tx_cycles - latency;
  for (uint64_t k = 0; k < drop; ++k) {
    const uint64_t bit = (k % tokens_per_beat) * token_bits;
    const unsigned char byte = static_cast<unsigned char>(
        buf[(k / tokens_per_beat) * BEAT_BYTES + bit / 8]);
    if ((byte >> (bit % 8)) & 1) {
      fail(prefix(),
           "latency switch dropped a VALID token: %s tx at link cycle %lu "
           "(drop window [%lu, %lu)). Link traffic in the drop window breaks "
           "the latency-switch contract; data or credits are lost",
           loopback ? "loopback" : "peer",
           first + k,
           drop_start,
           drop_end);
    }
  }
  drop_remaining -= drop;
  if (drop > 0 && drop_remaining == 0) {
    printf("%s latency switch drop window [%lu, %lu) done, all idle\n",
           prefix().c_str(),
           drop_start,
           drop_end);
    fflush(stdout);
  }
  push_off = drop / tokens_per_beat * BEAT_BYTES;
  push_bytes = batch_bytes - push_off;
  pending_push = push_bytes > 0;
}

// Opens the FIFOs without depending on the order the peers start in, so any
// topology (rings, multiple links per node) comes up without deadlock. Read
// ends open immediately with O_NONBLOCK, and every link opens its read end in
// init(). A write end only opens once the peer has its read end open, so it is
// opened (with retry) on the first tick(), after all of this node's links have
// opened their read ends. Opening it in init() deadlocks rings: each node
// would block on link 0's write end before opening link 1's read end.
void d2d_link_t::open_rx_fifo() {
  for (auto &path : {rx_fifo_path, tx_fifo_path}) {
    if (mkfifo(path.c_str(), 0666) != 0 && errno != EEXIST) {
      fail(prefix(), "mkfifo %s: %s", path.c_str(), strerror(errno));
    }
  }

  rx_fd = open(rx_fifo_path.c_str(), O_RDONLY | O_NONBLOCK);
  if (rx_fd < 0) {
    fail(prefix(), "open %s: %s", rx_fifo_path.c_str(), strerror(errno));
  }
}

void d2d_link_t::open_tx_fifo() {
  for (unsigned waited_ms = 0;; waited_ms += 100) {
    tx_fd = open(tx_fifo_path.c_str(), O_WRONLY | O_NONBLOCK);
    if (tx_fd >= 0)
      break;
    if (errno != ENXIO) {
      fail(prefix(), "open %s: %s", tx_fifo_path.c_str(), strerror(errno));
    }
    if (waited_ms % 10000 == 0) {
      printf("%s waiting for peer to open %s\n",
             prefix().c_str(),
             tx_fifo_path.c_str());
      fflush(stdout);
    }
    usleep(100 * 1000);
  }

  // The write end blocks; the read end stays non-blocking (see
  // recv_from_peer()).
  int flags = fcntl(tx_fd, F_GETFL);
  fcntl(tx_fd, F_SETFL, flags & ~O_NONBLOCK);

  // A side sends batch k+1 only after it has received the peer's batch k,
  // which the peer sent only after receiving batch k-1. So at most two of my
  // batches are unread in the pipe; size it for two so write() never blocks
  // on a peer that is itself waiting.
  size_t pipe_need = 2 * max_batch_bytes;
  int pipe_bytes = fcntl(tx_fd, F_SETPIPE_SZ, static_cast<int>(pipe_need));
  if (pipe_bytes < 0 || static_cast<size_t>(pipe_bytes) < pipe_need) {
    fail(prefix(),
         "cannot size %s to %zu bytes (%s); lower the latency or raise "
         "/proc/sys/fs/pipe-max-size",
         tx_fifo_path.c_str(),
         pipe_need,
         pipe_bytes < 0 ? strerror(errno) : "too small");
  }

  printf("%s connected: tx %s, rx %s\n",
         prefix().c_str(),
         tx_fifo_path.c_str(),
         rx_fifo_path.c_str());
}

void d2d_link_t::init() {
  // A dead peer should fail the write with EPIPE, not kill us with SIGPIPE.
  signal(SIGPIPE, SIG_IGN);

  if (!loopback) {
    open_rx_fifo();
  }

  // Pre-load a full batch of empty tokens (buf is zeroed): the target's first
  // `latency` cycles see an idle link, and from then on every token it
  // receives is the peer's token from `latency` cycles earlier. tick() pushes
  // it, retrying while the from-host stream applies backpressure.
  push_off = 0;
  push_bytes = batch_bytes;
  pending_push = true;
}

// Send my batch (in buf) to the peer. write() on a pipe may be partial, so
// loop until done; the pipe holds two batches, so this never waits on a peer
// that is blocked (see open_tx_fifo()).
void d2d_link_t::send_to_peer() {
  if (peer_closed)
    return;
  for (size_t done = 0; done < batch_bytes;) {
    ssize_t n = ::write(tx_fd, buf + done, batch_bytes - done);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0 && errno == EPIPE) {
      mark_peer_closed();
      return;
    }
    if (n <= 0) {
      fail(prefix(), "write to peer failed: %s", strerror(errno));
    }
    done += n;
  }
}

// Receive as much of the peer's batch for the same cycles into rx_buf as is
// available without blocking. Returns true once the whole batch is in.
void d2d_link_t::mark_peer_closed() {
  if (peer_closed)
    return;
  peer_closed = true;
  printf("%s peer closed the link; sending idle tokens until this node exits "
         "(fails after %lu cycles)\n",
         prefix().c_str(),
         closed_grace_cycles);
  fflush(stdout);
}

bool d2d_link_t::recv_from_peer() {
  if (peer_closed) {
    memset(rx_buf + rx_done, 0, batch_bytes - rx_done);
    rx_done = batch_bytes;
    closed_cycles += latency;
    if (closed_cycles > closed_grace_cycles) {
      fail(prefix(),
           "peer closed the link %lu cycles ago and this node is still "
           "running; the peer probably died",
           closed_cycles);
    }
    return true;
  }
  while (rx_done < batch_bytes) {
    ssize_t n = ::read(rx_fd, rx_buf + rx_done, batch_bytes - rx_done);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return false;
    if (n < 0) {
      fail(prefix(), "read from peer failed: %s", strerror(errno));
    }
    if (n == 0) {
      // We opened the read end before the peer opened its write end, and
      // a FIFO with no writer reads as EOF. Once the peer has written
      // anything, EOF means it exited.
      if (!rx_connected)
        return false;
      mark_peer_closed();
      return recv_from_peer();
    }
    rx_connected = true;
    rx_done += n;
  }
  return true;
}

void d2d_link_t::tick() {
  heartbeat();
  if (!loopback && tx_fd < 0) {
    open_tx_fifo();
  }
  for (unsigned batches = 0; batches < max_batches_per_tick; ++batches) {
    if (awaiting_rx) {
      if (!recv_from_peer()) {
        return; // Peer's batch not fully here yet; resume next tick.
      }
      awaiting_rx = false;
      ++rx_batches;
      rx_busy_beats += count_busy_beats(rx_buf, batch_beats);
      std::swap(buf, rx_buf);
      deliver_rx();
    }

    // Hand the previous batch to the target first. Until it is accepted the
    // target cannot advance past it, so there is nothing new to pull.
    if (pending_push) {
      size_t pushed =
          push(stream_from_cpu_idx, buf + push_off, push_bytes, push_bytes);
      if (pushed == 0) {
        ++push_stalls;
        return; // From-host stream full or H2C DMA busy; retry next tick.
      }
      if (pushed != push_bytes) {
        fail(prefix(), "pushed %zu bytes, expected %zu", pushed, push_bytes);
      }
      pending_push = false;
    }

    // Latency switch, once rx is queued through tx_cycles + latency - 1.
    if (next_switch < schedule.size() &&
        tx_cycles == schedule[next_switch].first) {
      apply_switch();
    }
    if (pad_bytes > 0) {
      size_t pushed = push(stream_from_cpu_idx, zero_buf, pad_bytes, pad_bytes);
      if (pushed == 0) {
        ++push_stalls;
        return; // retry next tick
      }
      if (pushed != pad_bytes) {
        fail(prefix(), "pushed %zu bytes, expected %zu", pushed, pad_bytes);
      }
      pad_bytes = 0;
    }

    // Move a whole batch worth of tokens at a time; return until the
    // target has produced that many.
    size_t pulled = pull(stream_to_cpu_idx, buf, batch_bytes, batch_bytes);
    if (pulled == 0) {
      ++pull_empty;
      if (++empty_pull_polls == flush_poll_limit) {
        pull_flush(stream_to_cpu_idx);
        empty_pull_polls = 0;
      }
      return;
    }
    empty_pull_polls = 0;
    if (pulled != batch_bytes) {
      fail(prefix(), "pulled %zu bytes, expected %zu", pulled, batch_bytes);
    }
    tx_cycles += latency;

    if (loopback) {
      // buf already holds this link's own tokens.
      deliver_rx();
    } else {
      ++tx_batches;
      tx_busy_beats += count_busy_beats(buf, batch_beats);
      send_to_peer();
      rx_done = 0;
      awaiting_rx = true;
    }
  }
}

void d2d_link_t::finish() {
  if (rx_fd >= 0)
    close(rx_fd);
  if (tx_fd >= 0)
    close(tx_fd);
  rx_fd = tx_fd = -1;
}

uint64_t d2d_link_t::count_busy_beats(const char *b, size_t beats) {
  uint64_t n = 0;
  for (size_t i = 0; i < beats; ++i) {
    const char *p = b + i * BEAT_BYTES;
    for (size_t j = 0; j < BEAT_BYTES; ++j) {
      if (p[j]) {
        ++n;
        break;
      }
    }
  }
  return n;
}

void d2d_link_t::heartbeat() {
  if (heartbeat_secs == 0)
    return;
  const uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
  if (hb_last_ns == 0) {
    hb_last_ns = now;
    return;
  }
  if (now - hb_last_ns < uint64_t(heartbeat_secs) * 1000000000ull)
    return;
  hb_last_ns = now;
  printf("%s HB tx_batches %lu rx_batches %lu tx_busy_beats %lu rx_busy_beats %lu "
         "push_stalls %lu pull_empty %lu pending_push %d awaiting_rx %d rx_done %zu "
         "peer_closed %d\n",
         prefix().c_str(), tx_batches, rx_batches, tx_busy_beats, rx_busy_beats,
         push_stalls, pull_empty, pending_push, awaiting_rx, rx_done, peer_closed);
  fflush(stdout);
}
