// See LICENSE for license details

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // F_SETPIPE_SZ
#endif

#include "d2d_link.h"
#include "core/simif.h"

#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
                       StreamEngine &stream,
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
  if (latency == 0 || latency % tokens_per_beat != 0) {
    fail(prefix(),
         "latency %lu must be a positive multiple of %u (tokens per beat)",
         latency,
         tokens_per_beat);
  }
  batch_beats = latency / tokens_per_beat;
  if (batch_beats > static_cast<size_t>(stream_from_cpu_depth)) {
    fail(prefix(),
         "latency %lu needs %zu beats in flight, but the from-host stream "
         "holds %d",
         latency,
         batch_beats,
         stream_from_cpu_depth);
  }
  batch_bytes = batch_beats * BEAT_BYTES;

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

  buf = static_cast<char *>(aligned_alloc(BEAT_BYTES, batch_bytes));
  memset(buf, 0, batch_bytes);
}

d2d_link_t::~d2d_link_t() { free(buf); }

// Opens both FIFOs without depending on the order the peers start in, so any
// topology (rings, multiple links per node) comes up without deadlock:
// read ends open immediately with O_NONBLOCK; a write end only opens once the
// peer has its read end open, so retry until it does.
void d2d_link_t::open_fifos() {
  for (auto &path : {rx_fifo_path, tx_fifo_path}) {
    if (mkfifo(path.c_str(), 0666) != 0 && errno != EEXIST) {
      fail(prefix(), "mkfifo %s: %s", path.c_str(), strerror(errno));
    }
  }

  rx_fd = open(rx_fifo_path.c_str(), O_RDONLY | O_NONBLOCK);
  if (rx_fd < 0) {
    fail(prefix(), "open %s: %s", rx_fifo_path.c_str(), strerror(errno));
  }

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

  for (int fd : {rx_fd, tx_fd}) {
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
  }

  // Each side writes a whole batch before reading the peer's, so the pipe must
  // hold one batch or both sides block in write().
  int pipe_bytes = fcntl(tx_fd, F_SETPIPE_SZ, static_cast<int>(batch_bytes));
  if (pipe_bytes < 0 || static_cast<size_t>(pipe_bytes) < batch_bytes) {
    fail(prefix(),
         "cannot size %s to %zu bytes (%s); lower the latency or raise "
         "/proc/sys/fs/pipe-max-size",
         tx_fifo_path.c_str(),
         batch_bytes,
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
    open_fifos();
  }

  // Pre-load a full batch of empty tokens: the target's first `latency`
  // cycles see an idle link, and from then on every token it receives is the
  // peer's token from `latency` cycles earlier.
  size_t pushed = push(stream_from_cpu_idx, buf, batch_bytes, batch_bytes);
  if (pushed != batch_bytes) {
    fail(prefix(),
         "could not pre-load %zu beats (pushed %zu bytes)",
         batch_beats,
         pushed);
  }
}

void d2d_link_t::tick() {
  while (true) {
    // Move a whole batch worth of tokens at a time; return until the
    // target has produced that many.
    size_t pulled = pull(stream_to_cpu_idx, buf, batch_bytes, batch_bytes);
    if (pulled == 0) {
      return;
    }
    if (pulled != batch_bytes) {
      fail(prefix(), "pulled %zu bytes, expected %zu", pulled, batch_bytes);
    }

    if (!loopback) {
      // Send mine, then receive the peer's batch for the same cycles. read()
      // and write() on a pipe may be partial, so loop until done.
      for (size_t done = 0; done < batch_bytes;) {
        ssize_t n = ::write(tx_fd, buf + done, batch_bytes - done);
        if (n < 0 && errno == EINTR)
          continue;
        if (n <= 0) {
          fail(prefix(), "write to peer failed: %s", strerror(errno));
        }
        done += n;
      }
      for (size_t done = 0; done < batch_bytes;) {
        ssize_t n = ::read(rx_fd, buf + done, batch_bytes - done);
        if (n < 0 && errno == EINTR)
          continue;
        if (n < 0) {
          fail(prefix(), "read from peer failed: %s", strerror(errno));
        }
        if (n == 0) {
          // We opened the read end before the peer opened its write end, and
          // a FIFO with no writer reads as EOF. Once the peer has written
          // anything, EOF means it exited.
          if (!rx_connected) {
            usleep(1000);
            continue;
          }
          fail(prefix(), "peer closed the link");
        }
        rx_connected = true;
        done += n;
      }
    }
    // In loopback, buf already holds this link's own tokens.

    size_t pushed = push(stream_from_cpu_idx, buf, batch_bytes, batch_bytes);
    if (pushed != batch_bytes) {
      fail(prefix(), "pushed %zu bytes, expected %zu", pushed, batch_bytes);
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
