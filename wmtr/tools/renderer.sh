#!/bin/bash
# renderer.sh dxmt|d3dmetal|wine [DXMT_DIR]: switch the D3D11 implementation of the extracted Sikarugir
# Wine engine given by ENGINE (a local test copy only). DXMT_DIR defaults to Sikarugir's DXMT build
# (layout: wine/x86_64-windows/*.dll, wine/x86_64-unix/winemetal.so).
set -eu
E=${ENGINE:?set ENGINE to the lib/wine directory of the Wine engine copy}
ORIG=${ENGINE_ORIG:-$E/../engine-orig}
T="$HOME/Library/Application Support/Sikarugir/Template/Template-1.0.15.app/Contents/Frameworks/renderer"
files="x86_64-windows/d3d11.dll x86_64-windows/dxgi.dll x86_64-windows/d3d10core.dll x86_64-windows/winemetal.dll x86_64-windows/d3d12.dll x86_64-windows/nvapi64.dll x86_64-windows/nvngx.dll x86_64-windows/atidxx64.dll x86_64-unix/winemetal.so x86_64-unix/d3d11.so x86_64-unix/dxgi.so x86_64-unix/d3d12.so x86_64-unix/nvapi64.so x86_64-unix/nvngx.so x86_64-unix/atidxx64.so"
if [ ! -d "$ORIG" ]; then
  mkdir -p "$ORIG/x86_64-windows" "$ORIG/x86_64-unix"
  for f in $files; do [ -e "$E/$f" ] && cp -p "$E/$f" "$ORIG/$f"; done
  echo saved original engine files
fi
restore() { for f in $files; do rm -f "$E/$f"; [ -e "$ORIG/$f" ] && cp -p "$ORIG/$f" "$E/$f"; done; return 0; }
case "$1" in
  wine) restore ;;
  dxmt) restore; S=${2:-$T/dxmt/wine}
        for f in d3d11.dll dxgi.dll d3d10core.dll winemetal.dll; do cp -p "$S/x86_64-windows/$f" "$E/x86_64-windows/$f"; done
        cp -p "$S/x86_64-unix/winemetal.so" "$E/x86_64-unix/winemetal.so" ;;
  d3dmetal) restore; S="$T/d3dmetal/wine"
        for f in "$S"/x86_64-windows/*.dll; do cp -p "$f" "$E/x86_64-windows/"; done
        for f in "$S"/x86_64-unix/*.so; do cp -p "$f" "$E/x86_64-unix/"; done ;;
  *) echo "usage: $0 dxmt|d3dmetal|wine [DXMT_DIR]"; exit 2 ;;
esac
# a prefix only finds builtins that are not in its fake-dll set through system32: mirror them there
if [ -n "${WINEPREFIX:-}" ] && [ -d "$WINEPREFIX/drive_c/windows/system32" ]; then
  for f in d3d11.dll dxgi.dll d3d10core.dll winemetal.dll d3d12.dll nvapi64.dll nvngx.dll atidxx64.dll; do
    [ -e "$E/x86_64-windows/$f" ] && cp -p "$E/x86_64-windows/$f" "$WINEPREFIX/drive_c/windows/system32/$f"
  done
fi
md5 -q "$E/x86_64-windows/d3d11.dll" | sed "s/^/$1 d3d11.dll md5 /"
