#!/usr/bin/env python3
"""Frame rate of each burst in a UCIe latency monitor log (`ucie-lat` lines,
`UcieTLParams.latencyMonitor`), for tests/ucie-dma.c.

Takes the TL beats sent on each channel (A or D, both dies together), splits
them into bursts wherever the channel is quiet for more than GAP cycles, and
prints each burst's beat count, span and beats per cycle.

Usage: ucie-burst-parse.py <sim log> [min beats]
"""

import re
import sys

GAP = 500
pat = re.compile(r"ucie-lat tx ([AD])\s+(\d+)")
min_beats = int(sys.argv[2]) if len(sys.argv) > 2 else 64
tx = {"A": [], "D": []}
for line in open(sys.argv[1], errors="replace"):
    m = pat.search(line)
    if m:
        tx[m.group(1)].append(int(m.group(2)))

for ch, ts in tx.items():
    ts.sort()
    bursts, start = [], 0
    for i in range(1, len(ts) + 1):
        if i == len(ts) or ts[i] - ts[i - 1] > GAP:
            bursts.append(ts[start:i])
            start = i
    for b in bursts:
        if len(b) >= min_beats:
            span = b[-1] - b[0] + 1
            print(f"{ch}: {len(b)} beats from {b[0]} over {span} cycles: "
                  f"{len(b) / span:.3f} beats/cycle")
