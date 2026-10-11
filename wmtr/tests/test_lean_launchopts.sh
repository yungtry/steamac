#!/bin/bash
# lean-launchopts.sh against stub systemctl/pgrep: Steam (steam.service) keeps running when only the gamescope
# session target is stopped, as in the guest. The script must stop steam.service itself, edit only then, start
# the units again, and "off" must give back the backup's launch options byte for byte.
set -u
here=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir -p "$T/bin" "$T/home/.local/share/Steam/userdata/1/config" "$T/state"
cp "$here/../guest/lean-launchopts.sh" "$here/../guest/lean-vdfopts.py" "$T/bin/"
touch "$T/state/steam-running"
cat > "$T/bin/systemctl" <<EOF
#!/bin/bash
echo "\$*" >> "$T/state/calls"
case "\$*" in *stop*steam.service*) rm -f "$T/state/steam-running" ;; *start*steam.service*) touch "$T/state/steam-running" ;; esac
EOF
cat > "$T/bin/pgrep" <<EOF
#!/bin/bash
[ "\$*" = "-x steam" ] && [ -f "$T/state/steam-running" ]
EOF
chmod +x "$T/bin/"*
C=$T/home/.local/share/Steam/userdata/1/config/localconfig.vdf
printf '"UserLocalConfigStore"\n{\n\t"438100"\n\t{\n\t\t"LaunchOptions"\t\t"WINEDEBUG=-all %%command%%"\n\t}\n}\n' > "$C"
cp "$C" "$T/orig"
fails=0
run() { HOME=$T/home PATH=$T/bin:/usr/bin:/bin XDG_RUNTIME_DIR=$T "$T/bin/lean-launchopts.sh" "$@"; }
run on >/dev/null || { fails=$((fails+1)); echo "FAIL: on returned non-zero"; }
grep -q 'lean-wrap.sh %command%' "$C" || { fails=$((fails+1)); echo "FAIL: on did not insert the wrapper"; }
grep -q 'stop steam.service' "$T/state/calls" || { fails=$((fails+1)); echo "FAIL: steam.service not stopped"; }
[ -f "$T/state/steam-running" ] || { fails=$((fails+1)); echo "FAIL: Steam not started again"; }
out=$(run off) || { fails=$((fails+1)); echo "FAIL: off returned non-zero"; }
case "$out" in *"restored: identical"*) ;; *) fails=$((fails+1)); echo "FAIL: off: $out" ;; esac
cmp -s "$C" "$T/orig" || { fails=$((fails+1)); echo "FAIL: off did not restore the file byte for byte"; }
echo "test_lean_launchopts: $([ $fails = 0 ] && echo passed || echo FAILED)"
exit $((fails > 0))
