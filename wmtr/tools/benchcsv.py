#!/usr/bin/env python3
"""benchcsv.py CSV...: summarise d3d11bench per-frame CSVs (frame,cpu_ms,present_ms,t_s).
The first second is excluded (warm-up). Frame time = difference of consecutive t_s.
Prints: label frames avg_fps p50_ms p95_ms p99_ms low1_fps low01_fps max_ms hitches>50ms cpu_p50_ms."""
import sys, csv, os

def pct(v, p):
    return v[min(len(v) - 1, int(p * len(v)))]

def summarise(path):
    rows = list(csv.DictReader(open(path)))
    t = [float(r["t_s"]) for r in rows]
    cpu = [float(r["cpu_ms"]) for r in rows]
    keep = [i for i in range(1, len(t)) if t[i - 1] >= 1.0]
    ft = sorted((t[i] - t[i - 1]) * 1000 for i in keep)
    if not ft:
        return None
    n = len(ft); span = t[keep[-1]] - t[keep[0] - 1]
    worst1 = ft[int(n * 0.99):] or ft[-1:]
    worst01 = ft[int(n * 0.999):] or ft[-1:]
    return dict(label=os.path.basename(path)[:-4], frames=n, avg_fps=n / span, p50=pct(ft, .5), p95=pct(ft, .95),
                p99=pct(ft, .99), low1=1000 / (sum(worst1) / len(worst1)), low01=1000 / (sum(worst01) / len(worst01)),
                max=ft[-1], hitch50=sum(1 for x in ft if x > 50), cpu50=sorted(cpu[i] for i in keep)[len(keep) // 2])

if __name__ == "__main__":
    print("label frames avg_fps p50_ms p95_ms p99_ms low1_fps low01_fps max_ms hitch>50ms cpu_p50_ms")
    for p in sys.argv[1:]:
        s = summarise(p)
        if s:
            print("{label} {frames} {avg_fps:.1f} {p50:.2f} {p95:.2f} {p99:.2f} {low1:.1f} {low01:.1f} {max:.1f} {hitch50} {cpu50:.3f}".format(**s))
