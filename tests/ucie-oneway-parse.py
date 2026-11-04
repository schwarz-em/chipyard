#!/usr/bin/env python3
"""One-way latency of every TL beat in a UCIe latency monitor log, under load.

Uses the die-tagged lines from `UcieTLParams.latencyMonitor`:
  ucie-frm <path> <A|D|C> <cycle>   a frame sent (C: credits only)
  ucie-frx <path> <A|D> <cycle>     a beat received
Frames cross the link in order and every beat sent is received, so the n-th
beat one die sends is the n-th beat the other receives. Pairs them, checks the
channels agree, and prints, per burst of the sender's frames, the latency's
min / mean / max, then the mean latency against the load on the link: how many
frames each die sent in the WINDOW cycles before the beat.

Usage: ucie-oneway-parse.py <sim log> [min beats]
"""

import bisect
import re
import sys
from collections import defaultdict
from statistics import mean

GAP = 500
WINDOW = 32
pat = re.compile(r"ucie-fr([mx]) (\S+) ([ADC])\s+(\d+)")
min_beats = int(sys.argv[2]) if len(sys.argv) > 2 else 64

sent = defaultdict(list)  # die -> [(cycle, kind)], every frame
recv = defaultdict(list)  # die -> [(cycle, kind)], beats only
for line in open(sys.argv[1], errors="replace"):
    m = pat.search(line)
    if m:
        path = m.group(2).split(".")
        die = next((p for p in path if p.startswith("chiptop")), path[0])
        (sent if m.group(1) == "m" else recv)[die].append(
            (int(m.group(4)), m.group(3)))

dies = sorted(set(sent) | set(recv))
if len(dies) != 2:
    sys.exit(f"expected two dies, found {dies}")
for d in dies:
    sent[d].sort()
    recv[d].sort()
sent_cycles = {d: [c for c, _ in sent[d]] for d in dies}


def frames_before(die, t):
    """Frames `die` sent in the WINDOW cycles before cycle t."""
    xs = sent_cycles[die]
    return bisect.bisect_left(xs, t) - bisect.bisect_left(xs, t - WINDOW)


for src, dst in [(dies[0], dies[1]), (dies[1], dies[0])]:
    beats = [(c, k) for c, k in sent[src] if k != "C"]
    rx = recv[dst]
    if len(beats) != len(rx):
        sys.exit(f"{src} sent {len(beats)} beats, {dst} got {len(rx)}")
    lat = []
    for (tc, tk), (rc, rk) in zip(beats, rx):
        if tk != rk:
            sys.exit(f"{src} -> {dst}: sent {tk} at {tc}, got {rk} at {rc}")
        lat.append((tc, rc - tc))
    print(f"{src} -> {dst}: {len(lat)} beats, latency min "
          f"{min(l for _, l in lat)} mean {mean(l for _, l in lat):.2f} "
          f"max {max(l for _, l in lat)}")

    start = 0
    for i in range(1, len(lat) + 1):
        if i == len(lat) or lat[i][0] - lat[i - 1][0] > GAP:
            b = lat[start:i]
            start = i
            if len(b) >= min_beats:
                ls = [l for _, l in b]
                print(f"  from {b[0][0]}: {len(b)} beats, min {min(ls)} "
                      f"mean {mean(ls):.2f} max {max(ls)}")

    # Mean latency by load, own direction and the reverse one.
    for who, die in [("own", src), ("reverse", dst)]:
        by = defaultdict(list)
        for t, l in lat:
            by[frames_before(die, t)].append(l)
        row = "  ".join(f"{n}:{mean(v):.1f}(x{len(v)})"
                        for n, v in sorted(by.items()))
        print(f"  by {who} frames in prior {WINDOW} cycles: {row}")
