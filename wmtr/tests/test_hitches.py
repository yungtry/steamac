#!/usr/bin/env python3
"""hitches: known frame series -> known counts (edge gaps included)."""
import importlib.util, os, sys
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("h", os.path.join(here, "..", "tools", "hitches.py"))
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
ms = 1_000_000
t = [i * 16 * ms for i in range(100)]           # 0..1584 ms at 16 ms
t += [t[-1] + 1500 * ms]                          # one 1.5 s freeze
t += [t[-1] + i * 16 * ms for i in range(1, 50)]  # then smooth again
end = t[-1] / 1e6 + 2500                          # 2.5 s frozen at the end of the window
r = h.hitches(t, 0, end, 0)
ok = r["hitch_ge_1s"] == 2 and r["hitch_ge_2s"] == 1 and r["hitch_ge_0.25s"] == 2 and r["longest_s"] == 2.5 and r["frames"] == 150
print("test_hitches:", "passed" if ok else "FAILED %r" % r)
sys.exit(0 if ok else 1)
