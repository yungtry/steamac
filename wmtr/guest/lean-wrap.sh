#!/bin/bash
# lean-wrap.sh %command% : Steam launch-options wrapper for the lean graphics path. Steam passes
#   <...>/Proton - Experimental/proton waitforexitandrun <game exe> [args]
# This runs the same command with ~/vrc/lean/proton-lean/proton (DXMT + wmtr instead of DXVK) and a separate
# prefix copy (STEAM_COMPAT_DATA_PATH=~/vrc/lean/compatdata-<appid>), so the game's real prefix and Steam's
# compat-tool setting are untouched. Everything else (EAC runtime env, game args) passes through unchanged.
set -u
L=$HOME/vrc/lean
# The launcher's Settings > Advanced "Graphics path" (kernel command line steamac.gfx=dxmt|vulkan) picks the path
# per boot; without it (older launcher), ~/vrc/lean/MODE does: "wmtr" = lean path, anything else = today's
# path, the command unchanged
mode=
for w in $(cat "${LEAN_CMDLINE:-/proc/cmdline}" 2>/dev/null); do case $w in steamac.gfx=dxmt) mode=wmtr ;; steamac.gfx=*) mode=passthrough ;; esac; done
[ -n "$mode" ] || mode=$(cat "$L/MODE" 2>/dev/null)
if [ "$mode" != wmtr ]; then
  echo "lean-wrap: $(date '+%F %T') passthrough (mode ${mode:-unset})" >> "$L/lean-wrap.log"
  exec "$@"
fi
args=()
for a in "$@"; do
  case "$a" in
    */"Proton - Experimental"/proton) args+=("$L/proton-lean/proton") ;;
    *) args+=("$a") ;;
  esac
done
if [ -n "${STEAM_COMPAT_DATA_PATH:-}" ]; then
  appid=$(basename "$STEAM_COMPAT_DATA_PATH")
  # never fall back to the real prefix: without the copy (lean-cdcopy.sh) the launch stops here
  [ -d "$L/compatdata-$appid/pfx" ] || { echo "lean-wrap: no $L/compatdata-$appid" >> "$L/lean-wrap.log"; exit 1; }
  real=$STEAM_COMPAT_DATA_PATH
  export STEAM_COMPAT_DATA_PATH="$L/compatdata-$appid"
  # the game's EAC launcher dir (launch options) points into the real prefix: use the copy's
  case "${EAC_LAUNCHERDIR:-}" in "$real"/*) export EAC_LAUNCHERDIR="$STEAM_COMPAT_DATA_PATH${EAC_LAUNCHERDIR#"$real"}" ;; esac
  "$L/lean-pfx-fix.sh" "$STEAM_COMPAT_DATA_PATH/pfx"
fi
# debugging: a TRACE flag file next to the scripts turns on the wmtr client's per-call trace
[ -f "$L/TRACE" ] && export WMTR_TRACE=$L/wmtr-trace
# DXMT's own log (errors such as failed PSOs) next to the copy, per executable
export DXMT_LOG_PATH=${DXMT_LOG_PATH:-$L/dxmt-log}; mkdir -p "$DXMT_LOG_PATH"
echo "lean-wrap: $(date '+%F %T') ${args[*]} (compatdata $STEAM_COMPAT_DATA_PATH, EAC_LAUNCHERDIR ${EAC_LAUNCHERDIR:-unset})" >> "$L/lean-wrap.log"
exec "${args[@]}"
