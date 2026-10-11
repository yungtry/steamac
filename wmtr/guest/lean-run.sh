#!/bin/bash
# lean-run.sh dxvk|wmtr EXE [ARGS...]: run a Windows test program in a scratch prefix under the Steam FEX tool,
# with Proton Experimental's Wine (dxvk: stock Proton files + DXVK dlls) or the lean copy (wmtr: DXMT PE dlls,
# winemetal.dll and the wmtr client winemetal.so). Never touches Steam's compatdata or settings.
# Based on the coordinator's runwine-s3.sh. Env: LEAN_DISPLAY (default :1), FEX (default: the Steam FEX tool).
set -u
MODE=$1; shift
L=$HOME/vrc/lean
case "$MODE" in
  dxvk) P="$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/files"; export WINEPREFIX=$L/pfx-dxvk
        export WINEDLLOVERRIDES="mscoree,mshtml=;d3d11,dxgi,d3d10core=n,b" ;;
  wmtr) P=$L/proton-lean/files; export WINEPREFIX=$L/pfx-wmtr
        export WINEDLLOVERRIDES="mscoree,mshtml=;d3d11,dxgi,d3d10core,winemetal=b" ;;
  *) echo "usage: $0 dxvk|wmtr EXE [ARGS]"; exit 2 ;;
esac
FEX=${FEX:-$HOME/.local/share/Steam/steamapps/common/FEX-Emu/usr/bin/FEX}
export FEX_ROOTFS=/usr/share/guestos/fex-mesa
export FEX_NEEDSSECCOMP=1 FEX_DISKCACHE=0
export LD_LIBRARY_PATH="$P/lib/x86_64-linux-gnu:$P/lib:$P/lib64"
export DISPLAY=${LEAN_DISPLAY:-:1}
# what Steam gives a game in the gamescope session: the gamescope Vulkan WSI layer (without it DXVK's
# swapchain creation fails) and, for DXVK, VRChat's dxvk.conf
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
export ENABLE_GAMESCOPE_WSI=1 GAMESCOPE_WAYLAND_DISPLAY=gamescope-0
[ "$MODE" = dxvk ] && [ -f "$HOME/vrc/dxvk.conf" ] && export DXVK_CONFIG_FILE=$HOME/vrc/dxvk.conf
export WINEDEBUG=${WINEDEBUG:--all}
exec "$FEX" "$P/bin/wine" "$@"
