#!/usr/bin/env python3
"""dxmt_member_order_check.py DXMT_SRC: ArgumentEncodingContext's command contexts build pipelines through
the context in their constructors (ClearUAV -> getComputePipeline -> ctx.device_), so `device_` and `lib`
must be declared before every command-context member (members are constructed in declaration order).
Fails on DXMT before upstream fb451568 (formerly our patch 0003)."""
import re, sys
src = open(sys.argv[1] + "/src/dxmt/dxmt_context.hpp").read()
body = src[src.index("class ArgumentEncodingContext {"):]
body = body[:body.index("\n};")]
def pos(pattern):
    m = re.search(pattern, body)
    return m.start() if m else None
dev, lib = pos(r"\n\s*WMT::Device device_;"), pos(r"\n\s*InternalCommandLibrary &lib;")
users = {n: pos(r"\n\s*[\w<>:]+\s+%s;" % n) for n in
         ("emulated_cmd", "clear_rt_cmd", "blit_depth_stencil_cmd", "clear_uav_cmd", "mv_scale_cmd", "tile_barrier_cmd")}
bad = [n for n, p in users.items() if p is not None and (dev is None or p < dev or lib is None or p < lib)]
print("dxmt_member_order_check:", "FAILED (declared before device_/lib: %s)" % ", ".join(bad) if bad else "passed")
sys.exit(1 if bad else 0)
