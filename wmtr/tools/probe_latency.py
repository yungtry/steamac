#!/usr/bin/env python3
"""probe_latency.py FRAMES_CSV INPUTS [--gap-ms MS]: input-to-present latency from d3d11bench --probe runs.

FRAMES_CSV: the host's presented-frame times (WMTR_FRAMES or STEAMAC_PERF_FRAMES; first column t_ns on the mach
clock, a header line is skipped). INPUTS: one mach-clock ns time per line, when the host injected an input
(python's time.monotonic_ns() on macOS uses the same clock). The probe holds back the frame after each input by
MS; the first frame that follows a gap of at least 0.8*MS after an input is the response, so
latency = that frame - input - MS. Prints one latency per input (ms) and min/median/p90/max."""
import argparse, statistics, sys


def frames(path):
    out = []
    for line in open(path):
        f = line.split(",")[0].strip()
        if f.isdigit():
            out.append(int(f))
    return sorted(out)


def latencies(ts, inputs, gap_ms):
    gap = gap_ms * 1e6
    res = []
    for ti in inputs:
        lat = None
        for i in range(1, len(ts)):
            if ts[i] <= ti:
                continue
            if ts[i] - ts[i - 1] >= 0.8 * gap and ts[i] - gap > ti - 2e6:
                lat = (ts[i] - ti - gap) / 1e6
                break
            if ts[i] - ti > gap + 500e6:  # no response within 0.5 s after the hold
                break
        res.append(lat)
    return res


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("frames"); ap.add_argument("inputs"); ap.add_argument("--gap-ms", type=float, default=60)
    a = ap.parse_args(argv)
    ins = [int(l) for l in open(a.inputs) if l.strip().isdigit()]
    lat = latencies(frames(a.frames), ins, a.gap_ms)
    for t, l in zip(ins, lat):
        print(f"{t},{'' if l is None else f'{l:.2f}'}")
    ok = sorted(l for l in lat if l is not None)
    if ok:
        p90 = ok[min(len(ok) - 1, int(0.9 * len(ok)))]
        print(f"n={len(ok)}/{len(lat)} min={ok[0]:.2f} median={statistics.median(ok):.2f} p90={p90:.2f} max={ok[-1]:.2f} ms",
              file=sys.stderr)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
