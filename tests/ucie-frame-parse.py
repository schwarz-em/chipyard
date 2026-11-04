#!/usr/bin/env python3
"""Counts the frames each die sends in a UCIe latency monitor log
(`ucie-frm <module path> <A|D|C> <cycle>` lines, `UcieTLParams.latencyMonitor`):
A and D frames carry a TL beat, C frames only return credits.

Splits each die's frames into bursts wherever it sends nothing for more than
GAP cycles, and prints each burst's counts by kind, span and frames per cycle.

Usage: ucie-frame-parse.py <sim log> [min frames]
"""

import re
import sys
from collections import defaultdict

GAP = 500
pat = re.compile(r"ucie-frm (\S+) ([ADC])\s+(\d+)")
min_frames = int(sys.argv[2]) if len(sys.argv) > 2 else 64
frames = defaultdict(list)  # die -> [(cycle, kind)]
for line in open(sys.argv[1], errors="replace"):
    m = pat.search(line)
    if m:
        # The die is the chip top on the path.
        path = m.group(1).split(".")
        die = next((p for p in path if p.startswith("chiptop")), path[0])
        frames[die].append((int(m.group(3)), m.group(2)))

for die in sorted(frames):
    fs = sorted(frames[die])
    total = defaultdict(int)
    for _, k in fs:
        total[k] += 1
    print(f"{die}: {len(fs)} frames: " +
          " ".join(f"{k} {total[k]}" for k in "ADC"))
    start = 0
    for i in range(1, len(fs) + 1):
        if i == len(fs) or fs[i][0] - fs[i - 1][0] > GAP:
            b = fs[start:i]
            start = i
            if len(b) < min_frames:
                continue
            n = defaultdict(int)
            for _, k in b:
                n[k] += 1
            span = b[-1][0] - b[0][0] + 1
            print(f"  from {b[0][0]} over {span} cycles: " +
                  " ".join(f"{k} {n[k]}" for k in "ADC") +
                  f", {len(b) / span:.3f} frames/cycle")
