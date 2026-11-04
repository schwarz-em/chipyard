#!/usr/bin/env python3
"""Breaks each UCIe request's round trip into steps, from the `ucie-req` lines
of `UcieTLParams.latencyMonitor`:

  ucie-req <path> <ev> <source> <cycle>

with ev, in order: av (request reaches the requester's protocol), af (its A
frame is sent), ra (A lands in the partner's rx buffer), ca (partner's SoC
takes it), dv (partner's SoC has the response), cd (partner sends the D
frame), rd (D lands in the requester's rx buffer), md (requester's SoC takes
the response).

av, af, rd and md are logged on the requester; ra, ca, dv and cd on the
partner. Older logs without dv lines are not supported.
Requests are matched by TL source: each step goes to the oldest request from
that die and source still missing it.

Prints, per burst of each die's requests (split where it starts none for GAP
cycles), the mean of each step and of the whole round trip.

Usage: ucie-req-parse.py <sim log> [min requests]
"""

import re
import sys
from collections import defaultdict, deque
from statistics import mean

GAP = 500
STEPS = ["av", "af", "ra", "ca", "dv", "cd", "rd", "md"]
REQUESTER = {"av", "af", "rd", "md"}
SEGMENTS = [
    ("wait", "av", "af"),     # in the protocol, before the A frame goes
    ("linkA", "af", "ra"),    # A frame over the link
    ("rxA", "ra", "ca"),      # partner's rx buffer
    ("soc", "ca", "dv"),      # partner's SoC, until the response is ready
    ("dwait", "dv", "cd"),    # partner waiting to send the D frame
    ("linkD", "cd", "rd"),    # D frame over the link
    ("rxD", "rd", "md"),      # requester's rx buffer
    ("total", "av", "md"),
]
pat = re.compile(r"ucie-req (\S+) (\w\w)\s+(\d+)\s+(\d+)")
min_reqs = int(sys.argv[2]) if len(sys.argv) > 2 else 64

events = []
for line in open(sys.argv[1], errors="replace"):
    m = pat.search(line)
    if m:
        path = m.group(1).split(".")
        die = next((p for p in path if p.startswith("chiptop")), path[0])
        events.append((int(m.group(4)), STEPS.index(m.group(2)), die,
                       m.group(2), int(m.group(3))))
events.sort()
dies = sorted({e[2] for e in events})
if len(dies) != 2:
    sys.exit(f"expected two dies, found {dies}")
other = {dies[0]: dies[1], dies[1]: dies[0]}

reqs = defaultdict(list)            # requester die -> [request dict]
open_ = defaultdict(deque)          # (requester, source) -> open requests
unlogged = defaultdict(int)         # requester -> fires with no logged offer
for cycle, _, die, ev, src in events:
    requester = die if ev in REQUESTER else other[die]
    q = open_[(requester, src)]
    # The protocol's A input may switch to another request without firing (an
    # arbiter upstream), so a request can be offered more than once, or fire
    # without its first offer having been logged. Keep the earliest offer, and
    # take the fire cycle for one that wasn't logged.
    if ev == "av":
        if not any("af" not in r for r in q):
            r = {"av": cycle}
            q.append(r)
            reqs[requester].append(r)
        continue
    if ev == "dv" and any("dv" in r and "cd" not in r for r in q):
        continue
    if ev == "af" and all("af" in r for r in q):
        r = {"av": cycle}
        q.append(r)
        reqs[requester].append(r)
        unlogged[requester] += 1
    for r in q:
        if ev not in r:
            r[ev] = cycle
            break
    else:
        sys.exit(f"{ev} from {die} source {src} at {cycle} matches no request")
    while q and "md" in q[0]:
        q.popleft()

for die in dies:
    rs = [r for r in reqs[die] if all(s in r for s in STEPS)]
    print(f"{die} requests: {len(reqs[die])} ({len(rs)} complete, "
          f"{unlogged[die]} with no logged offer)")
    print("  " + " ".join(f"{n:>8}" for n, _, _ in SEGMENTS) + "  (mean cycles)")
    start = 0
    for i in range(1, len(rs) + 1):
        if i == len(rs) or rs[i]["av"] - rs[i - 1]["av"] > GAP:
            b = rs[start:i]
            start = i
            if len(b) < min_reqs:
                continue
            row = " ".join(f"{mean(r[e] - r[s] for r in b):8.2f}"
                           for _, s, e in SEGMENTS)
            print(f"  {row}  {len(b)} from {b[0]['av']}")
