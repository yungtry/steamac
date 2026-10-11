#!/bin/bash
# render_check.sh: d3d11bench --dump through the wmtr two-process path must equal direct DXMT byte for byte
# (macOS; Sikarugir-style Wine engine with DXMT). Fails on DXMT without patch 0002 (wmtr frame = clear only).
#   WINE=.../bin/wine WINEPREFIX=... BENCH=d3d11bench.exe DIRECT=<dxmt dir, stock winemetal.so>
#   WMTR=<same dlls, wmtr client winemetal.so> RENDERER=.../renderer.sh SERVER=build/wmtr-server tests/render_check.sh
set -u
O=$(mktemp -d); export WMTR_SOCKET=$O/wmtr.sock WINEDEBUG=-all
"$SERVER" > "$O/server.log" 2>&1 & SP=$!
trap 'kill $SP 2>/dev/null; rm -rf "$O"' EXIT
dump() { "$RENDERER" dxmt "$2" >/dev/null && "$WINE" "$BENCH" --seconds 2 --draws 200 --overdraw 2 --dump "Z:${O//\//\\}\\$1.ppm" > "$O/$1.out" 2>&1; }
dump direct "$DIRECT"; dump wmtr "$WMTR"
"$RENDERER" dxmt "$DIRECT" >/dev/null
if [ -s "$O/direct.ppm" ] && cmp -s "$O/direct.ppm" "$O/wmtr.ppm"; then echo "render_check: passed"; exit 0; fi
echo "render_check: FAILED"; grep -a DUMP "$O"/*.out; exit 1
