#!/usr/bin/env python3
"""sample_busy: a thread whose samples all end in a blocking wait counts as idle, busy leaves count."""
import importlib.util, os, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("s", os.path.join(here, "..", "tools", "sample_busy.py"))
s = importlib.util.module_from_spec(spec); spec.loader.exec_module(s)
TXT = """Call graph:
    100 Thread_1: idle-worker
    + 100 thread_start  (in libsystem_pthread.dylib) + 8  [0x1]
    +   100 __psynch_cvwait  (in libsystem_kernel.dylib) + 8  [0x2]
    100 Thread_2: spinner
    + 100 thread_start  (in libsystem_pthread.dylib) + 8  [0x1]
    +   70 spin_loop  (in a.dylib) + 4  [0x3]
    +   30 __recvmsg  (in libsystem_kernel.dylib) + 8  [0x4]
    17 Thread_<multiple>   DispatchQueue_2158: com.Metal.CommandQueueDispatch  (serial)
    + 17 mach_msg2_trap  (in libsystem_kernel.dylib) + 8  [0x5]

Total number in stack:
"""
with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
    f.write(TXT)
rows = {name: busy for busy, name, total in s.parse(f.name)}
os.unlink(f.name)
fails = 0
for name, want in (("idle-worker", 0), ("spinner", 70)):
    if rows.get(name) != want: fails += 1; print("FAIL", name, rows.get(name), "want", want)
if not any("CommandQueueDispatch" in n for n in rows): fails += 1; print("FAIL: <multiple> thread header not parsed")
print("test_sample_busy:", "FAILED" if fails else "passed")
sys.exit(1 if fails else 0)
