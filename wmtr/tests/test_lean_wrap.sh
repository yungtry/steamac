#!/bin/bash
# lean-wrap.sh: the launcher's steamac.gfx= (kernel command line) picks the path; ~/vrc/lean/MODE only without it.
# dxmt -> the proton path is swapped for proton-lean; vulkan (or nothing and MODE != wmtr) -> command unchanged.
set -u
here=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
L=$T/home/vrc/lean; mkdir -p "$L/proton-lean" "$T/exp/Proton - Experimental"
printf '#!/bin/sh\necho "lean $*" > %s/ran\n' "$T" > "$L/proton-lean/proton"; chmod +x "$L/proton-lean/proton"
printf '#!/bin/sh\necho "exp $*" > %s/ran\n' "$T" > "$T/exp/Proton - Experimental/proton"; chmod +x "$T/exp/Proton - Experimental/proton"
printf '#!/bin/sh\nexit 0\n' > "$L/lean-pfx-fix.sh"; chmod +x "$L/lean-pfx-fix.sh"
mkdir -p "$L/compatdata-438100/pfx" "$T/real/438100/pfx"   # the prefix copy the lean path runs on
fails=0
check() { # cmdline MODE expected
  echo "$1" > "$T/cmdline"; echo "$2" > "$L/MODE"; : > "$T/ran"
  HOME=$T/home LEAN_CMDLINE=$T/cmdline STEAM_COMPAT_DATA_PATH=$T/real/438100 bash "$here/../guest/lean-wrap.sh" "$T/exp/Proton - Experimental/proton" waitforexitandrun game.exe >/dev/null 2>&1
  read -r got _ < "$T/ran"
  [ "$got" = "$3" ] || { fails=$((fails + 1)); echo "FAIL: cmdline '$1' MODE '$2': ran $got, want $3"; }
}
check "quiet steamac.gfx=dxmt" dxvk lean
check "quiet steamac.gfx=vulkan" wmtr exp
check "quiet" wmtr lean
check "quiet" dxvk exp
echo "test_lean_wrap: $([ $fails = 0 ] && echo passed || echo FAILED)"
exit $((fails > 0))
