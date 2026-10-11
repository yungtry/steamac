#!/usr/bin/env python3
"""probe_latency: a 60 ms hold after an input is found and the response latency is measured from it."""
import importlib.util, os, sys
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("p", os.path.join(here, "..", "tools", "probe_latency.py"))
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)
ms = 1_000_000
ts = [i * 16 * ms for i in range(100)]                 # 16 ms frames
# input at 205 ms; the app holds 60 ms; frames already queued keep coming until 240, then the gap, response at 240+60+33
inp = 205 * ms
ts = [t for t in ts if t <= 240 * ms] + [t + 93 * ms for t in ts if t > 240 * ms]
lat = p.latencies(sorted(ts), [inp], 60)
fails = 0
want = (ts[ts.index(256 * ms + 93 * ms)] - inp - 60 * ms) / ms
if lat[0] is None or abs(lat[0] - want) > 0.01: fails += 1; print("FAIL latency", lat, "want", want)
if p.latencies(sorted(range(0, 2000 * ms, 16 * ms)), [500 * ms], 60) != [None]: fails += 1; print("FAIL: found a response without a gap")
print("test_probe_latency:", "FAILED" if fails else "passed")
sys.exit(1 if fails else 0)
