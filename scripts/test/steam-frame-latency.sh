#!/bin/bash
# SPDX-License-Identifier: MIT
# guest/layer/usr/lib/steamac/steam-frame-latency (steamac.frame_latency= on the kernel command line):
#   off     no config file: nothing is created
#   on      creates the file with DXVK's stock option and the marker; the option actually reaches a
#           DXVK-style reader (DXVK's own semantics: the last assignment of a key wins)
#   again   a second run changes nothing (idempotent)
#   off-2   removes exactly the two lines it wrote, a game's own keys around them stay byte for byte
#   own     a dxvk.conf that already sets dxgi.maxFrameLatency is never given a duplicate
#   env     DXVK_CONFIG_FILE (what the lean run scripts and the fork's VRChat setup export) is used
#   on-2    turning it back on restores the option
# usage: scripts/test/steam-frame-latency.sh
set -u
S=$(cd "$(dirname "$0")/../.." && pwd)/guest/layer/usr/lib/steamac/steam-frame-latency
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fails=0; fail() { fails=$((fails + 1)); echo "FAIL [$1]: $2"; }

run() { # cmdline conf
    printf '%s\n' "$1" > "$T/cmdline"
    STEAMAC_CMDLINE=$T/cmdline STEAMAC_HOME=$T/home DXVK_CONFIG_FILE=$2 bash "$S" >/dev/null 2>&1
}
contains() { grep -qxF "$2" "$1" 2>/dev/null; }

# 1. off, no file: nothing is created
run "" "$T/none.conf"
[ -e "$T/none.conf" ] && fail off "created a config with frame latency off"

# 2. on: the file appears with the option, and a DXVK-style reader sees maxFrameLatency=1
C=$T/dxvk.conf
run "console=hvc0 steamac.frame_latency=1 rootwait" "$C"
[ -f "$C" ] || fail on "no config written"
contains "$C" "dxgi.maxFrameLatency = 1" || fail on "option missing"
last=$(grep -E '^[[:space:]]*dxgi\.maxFrameLatency[[:space:]]*=' "$C" | tail -1 | sed 's/.*=[[:space:]]*//')
[ "$last" = "1" ] || fail on "reader would apply $last, not 1"

# 3. idempotent
before=$(cksum < "$C"); run "steamac.frame_latency=1" "$C"
[ "$before" = "$(cksum < "$C")" ] || fail again "second run changed the file"

# 4. off removes only our block; the game's own keys survive byte for byte
own=$'# the game config\nmaxDeviceMemory = 8192\ndxgi.syncInterval = 1'
printf '%s\n' "$own" > "$C"
run "steamac.frame_latency=1" "$C"
grep -q maxFrameLatency "$C" || fail off-2 "enabling left no option behind"
run "" "$C"
[ "$(cat "$C")" = "$own" ] || fail off-2 "own keys: $(tr '\n' '|' < "$C")"
grep -q maxFrameLatency "$C" && fail off-2 "option left behind"

# 5. a conf that already sets the key is respected, never duplicated
printf 'dxgi.maxFrameLatency = 2\n' > "$C"
run "steamac.frame_latency=1" "$C"
[ "$(grep -c maxFrameLatency "$C")" = 1 ] || fail own "duplicated the game's option"
[ "$(grep -E 'maxFrameLatency' "$C" | sed 's/.*=[[:space:]]*//')" = "2" ] || fail own "changed the game's value"

# 6. DXVK_CONFIG_FILE is honoured (same file the lean scripts export)
E=$T/vrc-dxvk.conf
printf 'dxgi.syncInterval = 0\n' > "$E"
run "steamac.frame_latency=1" "$E"
contains "$E" "dxgi.maxFrameLatency = 1" || fail env "option not written through DXVK_CONFIG_FILE"
contains "$E" "dxgi.syncInterval = 0" || fail env "clobbered the existing file"

# 7. on again after off
run "" "$E"; grep -q maxFrameLatency "$E" && fail off-2 "off left the option in the env file"
run "steamac.frame_latency=1" "$E"
contains "$E" "dxgi.maxFrameLatency = 1" || fail on-2 "re-enabling did not restore the option"

echo "steam-frame-latency: $([ $fails = 0 ] && echo passed || echo FAILED)"
exit $((fails > 0))
