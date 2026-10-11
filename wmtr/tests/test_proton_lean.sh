#!/bin/bash
# Proton-lean wrapper (guest/proton-lean/proton) against a fake Proton Experimental, DXMT file set and prefix:
#   vulkan  runs Experimental's own proton, prefix untouched
#   dxmt    runs the overlay's proton; overlay dlls are DXMT's (dxvk/ copies without the builtin marker),
#           Experimental's files unchanged; the prefix gets DXMT + winemetal.dll and the marker
#   back    vulkan after dxmt: the prefix gets Experimental's DXVK dlls back, winemetal.dll and marker go
#   no-dxmt-files  steamac.gfx=dxmt without the DXMT files falls back to Experimental
set -u
here=$(cd "$(dirname "$0")" && pwd); P=$here/../guest/proton-lean/proton
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fails=0; fail() { fails=$((fails + 1)); echo "FAIL [$1]: $2"; }
blob() { head -c 200 /dev/zero | tr '\0' "$2" > "$1"; }   # 200-byte file filled with one character
E="$T/Proton - Experimental"; mkdir -p "$E/files/lib/wine/x86_64-windows" "$E/files/lib/wine/dxvk/x86_64-windows" "$E/files/lib/wine/x86_64-unix"
printf '#!/bin/sh\necho "$0 $*" >> "%s/ran"\n' "$T" > "$E/proton"; chmod +x "$E/proton"; echo "exp-1" > "$E/version"
for f in d3d11 dxgi d3d10core; do blob "$E/files/lib/wine/x86_64-windows/$f.dll" w; blob "$E/files/lib/wine/dxvk/x86_64-windows/$f.dll" v; done
L=$T/lean; mkdir -p "$L/x86_64-windows" "$L/x86_64-unix"
for f in d3d11 dxgi d3d10core winemetal; do blob "$L/x86_64-windows/$f.dll" m; done; blob "$L/x86_64-unix/winemetal.so" s
C=$T/compat; S=$C/pfx/drive_c/windows/system32; mkdir -p "$S"
for f in d3d11 dxgi d3d10core; do cp "$E/files/lib/wine/dxvk/x86_64-windows/$f.dll" "$S/"; done
sum_exp=$(cat "$E"/files/lib/wine/*/*.dll "$E"/files/lib/wine/dxvk/*/*.dll | cksum)
run() { # cmdline
  echo "$1" > "$T/cmdline"; : > "$T/ran"
  HOME=$T/home PROTON_EXPERIMENTAL=$E LEAN_DIR=$L LEAN_CMDLINE=$T/cmdline LEAN_STATE=$T/state STEAM_COMPAT_DATA_PATH=$C \
    bash "$P" waitforexitandrun game.exe -arg 2>"$T/err"; }

run "quiet steamac.gfx=vulkan"
grep -q "^$E/proton waitforexitandrun game.exe -arg" "$T/ran" || fail vulkan "did not exec Experimental: $(cat "$T/ran")"
cmp -s "$S/d3d11.dll" "$E/files/lib/wine/dxvk/x86_64-windows/d3d11.dll" || fail vulkan "prefix changed"

run "quiet steamac.gfx=dxmt"
O=$T/state/proton-lean-base
grep -q "^$O/proton waitforexitandrun game.exe -arg" "$T/ran" || fail dxmt "did not exec the overlay: $(cat "$T/ran") $(cat "$T/err")"
cmp -s "$O/files/lib/wine/x86_64-windows/d3d11.dll" "$L/x86_64-windows/d3d11.dll" || fail dxmt "overlay builtin d3d11 is not DXMT's"
[ "$(head -c 64 "$O/files/lib/wine/dxvk/x86_64-windows/dxgi.dll" | tr -d m)" = "" ] && [ "$(tail -c +65 "$O/files/lib/wine/dxvk/x86_64-windows/dxgi.dll" | head -c 16 | tr -d '\0')" = "" ] \
  || fail dxmt "overlay dxvk/dxgi.dll: not DXMT with the builtin marker cleared"
cmp -s "$O/files/lib/wine/x86_64-unix/winemetal.so" "$L/x86_64-unix/winemetal.so" || fail dxmt "overlay winemetal.so"
[ "$(cat "$E"/files/lib/wine/*/*.dll "$E"/files/lib/wine/dxvk/*/*.dll | cksum)" = "$sum_exp" ] || fail dxmt "Experimental's files were modified"
cmp -s "$S/d3d11.dll" "$O/files/lib/wine/dxvk/x86_64-windows/d3d11.dll" || fail dxmt "prefix d3d11 is not the overlay's DXMT"
cmp -s "$S/winemetal.dll" "$L/x86_64-windows/winemetal.dll" || fail dxmt "prefix lacks winemetal.dll"
[ "$(cat "$C/pfx/.steamac-gfx" 2>/dev/null)" = dxmt ] || fail dxmt "no prefix marker"
ino=$(ls -i "$O/files/lib/wine/x86_64-windows/dxgi.dll" | awk '{print $1}')
run "steamac.gfx=dxmt"
[ "$(ls -i "$O/files/lib/wine/x86_64-windows/dxgi.dll" | awk '{print $1}')" = "$ino" ] || fail dxmt "overlay rebuilt although nothing changed"

run "steamac.gfx=vulkan"
grep -q "^$E/proton " "$T/ran" || fail back "did not exec Experimental"
for f in d3d11 dxgi d3d10core; do cmp -s "$S/$f.dll" "$E/files/lib/wine/dxvk/x86_64-windows/$f.dll" || fail back "$f.dll not restored"; done
[ ! -e "$S/winemetal.dll" ] && [ ! -e "$C/pfx/.steamac-gfx" ] || fail back "winemetal.dll or marker left"

mv "$L" "$L.off"
run "steamac.gfx=dxmt"
grep -q "^$E/proton " "$T/ran" || fail no-dxmt-files "did not fall back to Experimental"
echo "test_proton_lean: $([ $fails = 0 ] && echo passed || echo FAILED)"
exit $((fails > 0))
