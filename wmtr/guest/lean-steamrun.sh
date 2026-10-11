#!/bin/bash
# lean-steamrun.sh dxvk|wmtr EXE [ARGS...]: run a Windows program through the same chain Steam uses for a
# game in the gamescope session (FEX compat tool -> Steam Linux Runtime 4 -> proton waitforexitandrun),
# with its own scratch compatdata (~/vrc/lean/cd-<mode>; created by proton on first use).
#   dxvk: stock Proton Experimental (DXVK), VRChat's dxvk.conf
#   wmtr: ~/vrc/lean/proton-lean (DXMT PE dlls + the wmtr winemetal client), see lean-setup.sh
# Never touches Steam's compatdata, settings or the game's prefix.
set -u
MODE=$1; shift
L=$HOME/vrc/lean; C=$HOME/.local/share/Steam/steamapps/common
case "$MODE" in
  dxvk) PROTON="$C/Proton - Experimental/proton"; export DXVK_CONFIG_FILE=$HOME/vrc/dxvk.conf ;;
  wmtr) PROTON="$L/proton-lean/proton" ;;
  *) echo "usage: $0 dxvk|wmtr EXE [ARGS]"; exit 2 ;;
esac
mkdir -p "$L/cd-$MODE"
# proton builds new prefixes from its default_pfx template, which has no winemetal.dll; Wine finds a
# builtin outside that set only in system32 (see lean-setup.sh)
if [ "$MODE" = wmtr ]; then "$L/lean-pfx-fix.sh" "$L/cd-$MODE/pfx" || true; fi
export STEAM_COMPAT_DATA_PATH=$L/cd-$MODE
export STEAM_COMPAT_CLIENT_INSTALL_PATH=$HOME/.local/share/Steam
export STEAM_COMPAT_TOOL_PATHS="$(dirname "$PROTON"):$C/SteamLinuxRuntime_4:$C/FEX-Emu"
export SteamAppId=0 SteamGameId=0
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
export DISPLAY=${LEAN_DISPLAY:-:1} ENABLE_GAMESCOPE_WSI=1 GAMESCOPE_WAYLAND_DISPLAY=gamescope-0
export WINEDEBUG=${WINEDEBUG:--all} PROTON_USE_XALIA=0
export WINE_CPU_TOPOLOGY=16:0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3
exec "$C/FEX-Emu/fex-compat-tool" waitforexitandrun -- "$C/SteamLinuxRuntime_4/_v2-entry-point" \
  --verb=waitforexitandrun -- "$PROTON" waitforexitandrun "$@"
