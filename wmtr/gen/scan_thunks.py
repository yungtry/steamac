#!/usr/bin/env python3
"""Map each winemetal unix-call index to its PE thunk function and params type (DXMT 4ddb20e)."""
import re, sys
src = open(sys.argv[1]).read() + "\n" + open(sys.argv[2]).read()
funcs = re.split(r'\n(?=WINEMETAL_API|static |int |void |NTSTATUS |[A-Za-z_][A-Za-z0-9_ \*]*\n[A-Za-z_][A-Za-z0-9_]*\()', src)
res = {}
for f in funcs:
    m = re.search(r'\n?([A-Za-z_][A-Za-z0-9_]*)\s*\(', f)
    calls = re.findall(r'UNIX_CALL\((\w+),\s*&?(\w+)\)', f)
    if not calls: continue
    name = m.group(1) if m else '?'
    for code, var in calls:
        t = re.search(r'struct (\w+) ' + var + r'\s*;', f) or re.search(r'(\w+) ' + var + r'\s*[;=]', f)
        ptype = t.group(1) if t else ('param:' + var)
        res.setdefault(code, []).append((name, ptype))
for k in sorted(res, key=lambda x: int(x) if x.isdigit() else 999):
    print(k, res[k])
