#!/usr/bin/env python3
"""callstats_report: per-frame numbers come from the difference of the window's first and last snapshot."""
import contextlib, importlib.util, io, os, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("c", os.path.join(here, "..", "tools", "callstats_report.py"))
c = importlib.util.module_from_spec(spec); spec.loader.exec_module(c)
rows = []
for t, k in ((1000, 0), (2000, 1), (3000, 2)):
    rows += [f"{t},47,MTLCommandBuffer_presentDrawable,0,{10 * k},0,{10 * k * 1000}",
             f"{t},32,MTLCommandBuffer_renderCommandEncoder,1,{30 * k},{30 * k},{30 * k * 70000}",
             f"{t},200,_executor_sleeps,9,{5 * k},0,0", f"{t},201,_kicks,9,{4 * k},0,0"]
with tempfile.NamedTemporaryFile("w", suffix=".csv", delete=False) as f:
    f.write("\n".join(rows) + "\nbroken line\n")
out = io.StringIO()
with contextlib.redirect_stdout(out):
    rc = c.main([f.name, "--from", "2000"])
os.unlink(f.name)
got = dict(l.split(",", 1) for l in out.getvalue().splitlines() if l.count(",") == 1)
want = {"frames": "10", "calls_per_frame": "4.0", "sync_per_frame": "3.0", "executor_sleeps_per_frame": "0.50",
        "kicks_per_frame": "0.40"}
fails = [k for k, v in want.items() if got.get(k) != v]
if "32,S,MTLCommandBuffer_renderCommandEncoder,3.00,3.00,70.0" not in out.getvalue(): fails.append("encoder row")
if rc != 0: fails.append("rc")
print("test_callstats_report:", "FAILED " + str(fails) + "\n" + out.getvalue() if fails else "passed")
sys.exit(1 if fails else 0)
