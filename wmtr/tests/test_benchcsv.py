#!/usr/bin/env python3
"""benchcsv: known synthetic frame series -> known numbers (warm-up second excluded)."""
import importlib.util, os, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("b", os.path.join(here, "..", "tools", "benchcsv.py"))
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
# 1 s of 100 ms warm-up frames, then 999 frames of 10 ms and one 100 ms hitch
t, rows = 0.0, []
for i in range(10): t += 0.1; rows.append(t)
for i in range(999): t += 0.010; rows.append(t)
t += 0.100; rows.append(t)
with tempfile.NamedTemporaryFile("w", suffix=".csv", delete=False) as f:
    f.write("frame,cpu_ms,present_ms,t_s\n")
    for i, x in enumerate(rows): f.write("%d,0.5,1.0,%.6f\n" % (i, x))
s = b.summarise(f.name); os.unlink(f.name)
ok = (s["frames"] == 1000 and abs(s["p50"] - 10) < 1e-6 and abs(s["max"] - 100) < 1e-6 and s["hitch50"] == 1
      and abs(s["avg_fps"] - 1000 / 10.09) < 0.01 and abs(s["low01"] - 10) < 1e-6)
print("test_benchcsv:", "passed" if ok else "FAILED %r" % s)
sys.exit(0 if ok else 1)
