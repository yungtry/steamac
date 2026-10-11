#!/usr/bin/env python3
"""lean-vdfopts.py FILE on|off|show APPID WRAP: edit one app's LaunchOptions in a Steam localconfig.vdf:
'on' inserts WRAP in front of %command%, 'off' removes exactly that insertion, 'show' prints the options.
Every other byte of the file is kept."""
import re, sys

def edit(text, act, app, wrap):
    i = text.find('"%s"' % app)
    if i < 0:
        raise ValueError("no app entry")
    m = re.compile(r'("LaunchOptions"\s+")((?:[^"\\]|\\.)*)(")').search(text, i)
    if not m:
        raise ValueError("no LaunchOptions")
    opts = m.group(2)
    if act == "show":
        return text, opts
    if act == "on":
        if wrap in opts:
            return text, opts
        if "%command%" not in opts:
            raise ValueError("no %command%")
        new = opts.replace("%command%", wrap + "%command%", 1)
    else:
        new = opts.replace(wrap, "", 1)
    return text[:m.start(2)] + new + text[m.end(2):], new

if __name__ == "__main__":
    path, act, app, wrap = sys.argv[1:5]
    s = open(path, encoding="utf-8", errors="surrogateescape").read()
    out, opts = edit(s, act, app, wrap)
    if act != "show":
        open(path, "w", encoding="utf-8", errors="surrogateescape").write(out)
    print(opts)
