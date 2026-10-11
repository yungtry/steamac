#!/bin/bash
# lean-pfx-fix.sh PFX: make a proton prefix use the lean (wmtr) graphics dlls: install winemetal.dll
# into system32 and make sure d3d11/dxgi/d3d10core there are proton-lean's (DXMT). No-op before the
# prefix exists (proton creates it on first run; run again afterwards).
set -u
PFX=$1; W=$HOME/vrc/lean/proton-lean/files/lib/wine
S=$PFX/drive_c/windows/system32
[ -d "$S" ] || exit 0
for f in winemetal.dll; do cmp -s "$W/x86_64-windows/$f" "$S/$f" || cp "$W/x86_64-windows/$f" "$S/$f"; done
for f in d3d11.dll dxgi.dll d3d10core.dll; do cmp -s "$W/dxvk/x86_64-windows/$f" "$S/$f" || cp "$W/dxvk/x86_64-windows/$f" "$S/$f"; done
