#!/bin/bash
# lean-cdcopy.sh APPID: make ~/vrc/lean/compatdata-APPID, a copy of the game's compatdata for the lean
# graphics path (lean-wrap.sh points STEAM_COMPAT_DATA_PATH at it), so the real prefix is never written.
# Unity's asset cache (Cache-WindowsPlayer, write-once __data bundles) is hard-linked to save space; its
# small per-entry files that Unity rewrites (__info, __lock) are real copies. Then the wmtr dlls go in.
set -eu
APP=$1; L=$HOME/vrc/lean; SRC=$HOME/.local/share/Steam/steamapps/compatdata/$APP; DST=$L/compatdata-$APP
[ -d "$SRC" ] || { echo "no $SRC"; exit 1; }
[ -e "$DST" ] && { echo "$DST exists"; exit 1; }
CACHE=pfx/drive_c/users/steamuser/AppData/LocalLow/VRChat/VRChat/Cache-WindowsPlayer
mkdir -p "$DST"
( cd "$SRC" && tar --exclude="./$CACHE" -cf - . ) | ( cd "$DST" && tar -xpf - )
if [ -d "$SRC/$CACHE" ]; then
  cp -al "$SRC/$CACHE" "$DST/$CACHE"
  find "$DST/$CACHE" -type f ! -name __data -print0 | while IFS= read -r -d '' f; do cp --remove-destination "$f" "$f.tmp$$" && mv -f "$f.tmp$$" "$f"; done
fi
"$L/lean-pfx-fix.sh" "$DST/pfx"
echo "made $DST: $(du -sh "$DST" | cut -f1) (hard links counted)"
