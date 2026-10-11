#!/usr/bin/env python3
"""Summarise a wmtcount log (see tests/wmtcount.c) against the winemetal call table.

Usage:
  python3 -I wmtr/tools/wmtcount_report.py WMTCOUNT_LOG [src/wmtr_calls.inc]

Prints, per measurement block (the log writes one block every WMTCOUNT_PERIOD frames):
  frames, fps, calls/frame, sync calls/frame, and the calls that dominate blocking time.
Class letters come from src/wmtr_calls.inc: A = async, S = sync, B = sync-but-may-block,
C = sync-and-heavy, N = not part of the table.
"""
import re
import sys
from pathlib import Path

INC_DEFAULT = Path(__file__).resolve().parent.parent / "src" / "wmtr_calls.inc"
BLOCK = re.compile(r"^== frames=(\d+) wall_s=([\d.]+) fps=([\d.]+)")
CALL = re.compile(r"^call\s+(\d+) calls/frame=\s*([\d.]+) us/call=\s*([\d.]+) ms/frame=\s*([\d.]+)")
TOTAL = re.compile(r"^total calls/frame=([\d.]+) host_ms/frame=([\d.]+)")


def load_table(path):
    names, classes = {}, {}
    for line in path.read_text().splitlines():
        m = re.match(r"X\((\d+),\s*(\S+),\s*[^,]+,\s*([ASBCN])\)", line.strip())
        if m:
            idx = int(m.group(1))
            names[idx], classes[idx] = m.group(2), m.group(3)
    if not names:
        sys.exit(f"no calls parsed from {path}")
    return names, classes


def parse_log(path):
    blocks, cur = [], None
    for line in path.read_text().splitlines():
        m = BLOCK.match(line)
        if m:
            if cur:
                blocks.append(cur)
            cur = {"frames": int(m.group(1)), "fps": float(m.group(3)), "calls": {}}
            continue
        if cur is None:
            continue
        if (m := CALL.match(line)):
            cur["calls"][int(m.group(1))] = tuple(float(m.group(i)) for i in (2, 3, 4))
        elif (m := TOTAL.match(line)):
            cur["total_calls"] = float(m.group(1))
            cur["total_ms"] = float(m.group(2))
    if cur:
        blocks.append(cur)
    return blocks


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    names, classes = load_table(Path(sys.argv[2]) if len(sys.argv) > 2 else INC_DEFAULT)
    blocks = parse_log(Path(sys.argv[1]))
    if not blocks:
        sys.exit(f"no measurement blocks in {sys.argv[1]}")

    print(f"{'block':>5} {'frames':>7} {'fps':>7} {'calls/fr':>9} {'sync/fr':>8} {'host ms/fr':>10}")
    for n, b in enumerate(blocks):
        total = b.get("total_calls", sum(c for c, _, _ in b["calls"].values()))
        sync = sum(c for i, (c, _, _) in b["calls"].items() if classes.get(i, "N") in "SBC")
        print(f"{n:>5} {b['frames']:>7} {b['fps']:>7.1f} {total:>9.1f} {sync:>8.1f} {b.get('total_ms', 0):>10.3f}")

    # Steady-state = blocks after the first (the first usually covers warm-up / a short run).
    steady = blocks[1:] or blocks
    print(f"\ndominant blocking calls over blocks 1..{len(blocks) - 1} "
          f"({len(steady)} blocks; steady state)")
    print(f"{'call':>5}  {'class':<5} {'ms/frame':>9} {'us/call':>9} {'name'}")
    agg = {}
    for b in steady:
        for i, (c, us, ms) in b["calls"].items():
            a = agg.setdefault(i, [0.0, 0.0, 0.0])
            a[0] += ms
            a[1] += us
            a[2] += 1
    for i, (ms, us, k) in sorted(agg.items(), key=lambda kv: -kv[1][0])[:8]:
        print(f"{i:>5}  {classes.get(i, 'N'):<5} {ms / k:>9.3f} {us / k:>9.2f} {names.get(i, '?')}")


if __name__ == "__main__":
    main()
