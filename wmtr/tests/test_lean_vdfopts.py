#!/usr/bin/env python3
"""lean-vdfopts: 'on' then 'off' must give back the file byte for byte; only the target app changes."""
import importlib.util, os, sys
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("v", os.path.join(here, "..", "guest", "lean-vdfopts.py"))
v = importlib.util.module_from_spec(spec); spec.loader.exec_module(v)
SRC = '''"UserLocalConfigStore"
{
\t"Software" { "Valve" { "Steam" { "apps" {
\t\t"2073850" { "LaunchOptions"\t\t"FOO=1 %command%" }
\t\t"438100"
\t\t{
\t\t\t"LastPlayed"\t\t"1"
\t\t\t"LaunchOptions"\t\t"env A=\\"x y\\" PROTON_EAC_RUNTIME='/p q' %command% -screen"
\t\t}
\t} } } }
}
'''
W = "/home/u/vrc/lean/lean-wrap.sh "
fails = 0
on, o = v.edit(SRC, "on", "438100", W)
if o != "env A=\\\"x y\\\" PROTON_EAC_RUNTIME='/p q' " + W + "%command% -screen": fails += 1; print("FAIL on:", o)
if '"FOO=1 %command%"' not in on: fails += 1; print("FAIL: other app changed")
on2, _ = v.edit(on, "on", "438100", W)
if on2 != on: fails += 1; print("FAIL: on is not idempotent")
off, _ = v.edit(on, "off", "438100", W)
if off != SRC: fails += 1; print("FAIL: off does not restore the file")
print("test_lean_vdfopts:", "FAILED" if fails else "passed")
sys.exit(1 if fails else 0)
