#!/usr/bin/env python3
"""Summarizes the UCIe latency monitor's log (`UcieTLParams.latencyMonitor`).

Each line is `ucie-lat <tx|rx> <A|D> <cycle>`, from either die, with the cycle
counted since reset in the link's clock. With one request in flight at a time
(tests/ucie-latency.c), every beat sent on a channel is received before the
next is sent, so each tx pairs with the next rx on the same channel. Over a
zero-latency link a beat's rx can be logged before its tx in the same cycle,
so events are taken in cycle order, tx first.

Prints, for A (request) and D (response) beats separately, the one-way
latency rx - tx, plus the partner's turnaround (its D tx minus its A rx) and
the requester's round trip at the frame interface (its D rx minus its A tx).

Usage: ucie-latency-parse.py <sim log>
"""

import re
import sys
from statistics import mean

pat = re.compile(r"ucie-lat (tx|rx) ([AD])\s+(\d+)")
events = []  # (dir, chan, cycle)
for line in open(sys.argv[1], errors="replace"):
    m = pat.search(line)
    if m:
        events.append((m.group(1), m.group(2), int(m.group(3))))
events.sort(key=lambda e: (e[2], e[0] == "rx"))

pending = {}  # channel -> tx cycle awaiting its rx
lat = {"A": [], "D": []}
for d, ch, t in events:
    if d == "tx":
        if ch in pending:
            sys.exit(f"two {ch} beats in flight at cycle {t}: not isolated")
        pending[ch] = t
    else:
        if ch not in pending:
            sys.exit(f"{ch} beat received at cycle {t} with none sent")
        lat[ch].append(t - pending.pop(ch))

# Requests: tx A, rx A, tx D, rx D in that order.
turnaround, rtt = [], []
for i in range(len(events) - 3):
    seq = events[i : i + 4]
    if [(d, c) for d, c, _ in seq] == [("tx", "A"), ("rx", "A"), ("tx", "D"), ("rx", "D")]:
        turnaround.append(seq[2][2] - seq[1][2])
        rtt.append(seq[3][2] - seq[0][2])


def show(name, xs):
    if xs:
        print(f"{name}: n={len(xs)} min {min(xs)} mean {mean(xs):.2f} max {max(xs)}")
        print(f"  {xs}")
    else:
        print(f"{name}: none")


show("A one-way", lat["A"])
show("D one-way", lat["D"])
show("partner turnaround", turnaround)
show("round trip at frame interface", rtt)
