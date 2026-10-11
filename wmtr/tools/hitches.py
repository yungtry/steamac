#!/usr/bin/env python3
"""hitches.py FRAMES_CSV START_EPOCH_MS END_EPOCH_MS: count long frames (hitches) in a window of a steamac
per-frame export (STEAMAC_PERF_FRAMES / WMTR_FRAMES format: t_ns on the mach clock, first column).
Intervals between consecutive frames inside the window, plus the gaps from the window start to the first
frame and from the last frame to the window end (a frozen picture is a hitch even with no frame after it).
Prints: hitch_ge_0.25s, hitch_ge_0.5s, hitch_ge_1s, hitch_ge_2s, hitch_time_ge_0.25s_s, longest_s.
Clock: like frame_stats.py (current offset, or FRAME_ANCHOR_MS)."""
import csv, os, sys, time

def hitches(t_ns, start_ms, end_ms, offset_ns):
    w = [(t + offset_ns) / 1e6 for t in t_ns]
    inside = [x for x in w if start_ms <= x <= end_ms]
    edges = [start_ms] + inside + [end_ms]
    gaps = [(b - a) / 1000 for a, b in zip(edges, edges[1:])]
    out = {"hitch_ge_%gs" % th: sum(1 for g in gaps if g >= th) for th in (0.25, 0.5, 1, 2)}
    out["hitch_time_ge_0.25s_s"] = round(sum(g for g in gaps if g >= 0.25), 2)
    out["longest_s"] = round(max(gaps), 2) if gaps else 0
    out["frames"] = len(inside)
    return out

def main(argv):
    rows = []
    with open(argv[1], newline="", errors="replace") as f:
        for r in csv.reader(f):
            if r and r[0].isdigit():
                rows.append(int(r[0]))
    anchor = os.environ.get("FRAME_ANCHOR_MS")
    off = float(anchor) * 1e6 - rows[0] if anchor and rows else time.time_ns() - time.monotonic_ns()
    for k, v in hitches(rows, float(argv[2]), float(argv[3]), off).items():
        print("%s,%s" % (k, v))

if __name__ == "__main__":
    main(sys.argv)
