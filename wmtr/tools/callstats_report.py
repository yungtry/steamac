#!/usr/bin/env python3
"""callstats_report.py CSV [--from T0_NS] [--to T1_NS] [--top N]: per-frame call profile from WMTR_CALLSTATS.

WMTR_CALLSTATS (wmtr host) appends cumulative snapshots: t_ns,call,name,class,count,sync,exec_ns. Rows 200/201
(class 9) are executor sleeps and client kicks. This takes the first and last snapshot inside [T0, T1] (mach
clock, the same base as WMTR_FRAMES; default: the whole file), and prints per frame (frames = presentDrawable
calls 47 + 48 in the window): calls, sync calls, host execution time per call kind, plus sleeps and kicks.
Classes: 0 A (async), 1 S (sync), 2 B (blocking queue), 3 C (compile queue), 9 counters."""
import argparse, collections, sys

CLASS = {0: "A", 1: "S", 2: "B", 3: "C", 9: "-"}


def load(path):
    snaps = collections.OrderedDict()
    for line in open(path, errors="replace"):
        p = line.rstrip("\n").split(",")
        if len(p) != 7:
            continue
        try:
            t, call, name, cls, n, sy, ns = int(p[0]), int(p[1]), p[2], int(p[3]), int(p[4]), int(p[5]), int(p[6])
        except ValueError:
            continue
        snaps.setdefault(t, {})[call] = (name, cls, n, sy, ns)
    return snaps


def window(snaps, t0, t1):
    ts = [t for t in snaps if (t0 is None or t >= t0) and (t1 is None or t <= t1)]
    if len(ts) < 2:
        return None
    a, b = snaps[ts[0]], snaps[ts[-1]]
    out = {}
    for call, (name, cls, n, sy, ns) in b.items():
        n0, sy0, ns0 = a.get(call, (name, cls, 0, 0, 0))[2:]
        if n - n0:
            out[call] = (name, cls, n - n0, sy - sy0, ns - ns0)
    return ts[0], ts[-1], out


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--from", dest="t0", type=int)
    ap.add_argument("--to", dest="t1", type=int)
    ap.add_argument("--top", type=int, default=15)
    a = ap.parse_args(argv)
    w = window(load(a.csv), a.t0, a.t1)
    if not w:
        print("need two snapshots in the window", file=sys.stderr)
        return 1
    t0, t1, d = w
    frames = sum(d.get(c, (0, 0, 0))[2] for c in (47, 48))
    per = frames or 1
    calls = [(k, v) for k, v in d.items() if v[1] != 9]
    print(f"window_s,{(t1 - t0) / 1e9:.1f}")
    print(f"frames,{frames}")
    print(f"calls_per_frame,{sum(v[2] for _, v in calls) / per:.1f}")
    print(f"sync_per_frame,{sum(v[3] for _, v in calls) / per:.1f}")
    print(f"host_exec_ms_per_frame,{sum(v[4] for _, v in calls) / per / 1e6:.3f}")
    for c, label in ((200, "executor_sleeps_per_frame"), (201, "kicks_per_frame")):
        print(f"{label},{d.get(c, (0, 0, 0))[2] / per:.2f}")
    print("call,class,name,per_frame,sync_per_frame,us_per_call")
    calls.sort(key=lambda kv: -kv[1][3])  # most synchronous first: the round trips to remove
    for call, (name, cls, n, sy, ns) in calls[: a.top]:
        print(f"{call},{CLASS.get(cls, cls)},{name},{n / per:.2f},{sy / per:.2f},{ns / n / 1e3:.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
