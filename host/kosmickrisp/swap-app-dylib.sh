#!/bin/sh
# Swap the KosmicKrisp driver inside an installed steamac app, and put the original back.
#
#   host/kosmickrisp/swap-app-dylib.sh swap [--resign] [--force] [dylib]
#                                        default dylib: work/out/host/lib/libvulkan_kosmickrisp.dylib
#   host/kosmickrisp/swap-app-dylib.sh restore [--force]
#   host/kosmickrisp/swap-app-dylib.sh status
#
# The app: STEAMAC_APP, else /Applications/FX Steam Launcher.app, else the first app Spotlight finds
# with bundle id es.fxgam.steamac.
#
# swap replaces Contents/Frameworks/libvulkan_kosmickrisp.dylib (temp file + rename: a running VM may
# have the old one mapped) and leaves the app's own signature alone. The release app is signed with
# com.apple.security.cs.disable-library-validation, so its unchanged main executable loads an ad-hoc
# signed driver, and macOS keeps the microphone/keychain permissions (they follow the executable's
# signature). The bundle's seal no longer matches (codesign --verify fails) until restore.
# --resign also re-signs the app ad hoc with its own entitlements, for a macOS that refuses to launch
# the modified bundle; macOS then treats it as a new app (microphone and keychain prompts again).
#
# The first swap copies the original dylib (and, with --resign, the main executable and the bundle's
# CodeResources files) to STEAMAC_KK_BACKUP (default ~/Library/Application Support/steamac-kk-swap);
# later swaps keep that copy. restore puts every backed-up file back, which makes the bundle
# byte-identical to the release again, and checks the signature.
#
# Writing into an app in /Applications needs the App Management permission (System Settings > Privacy &
# Security) for the app this script runs in; without it swap and restore stop before changing anything.
#
# swap and restore refuse while the app's VM is running (--force overrides; the running VM keeps the
# driver it loaded until it restarts). Select KosmicKrisp in the app's settings (or
# --vulkan-driver kosmickrisp) for the swapped driver to be used.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
backup=${STEAMAC_KK_BACKUP:-$HOME/Library/Application Support/steamac-kk-swap}
name=libvulkan_kosmickrisp.dylib

die() { echo "$*" >&2; exit 1; }

app=${STEAMAC_APP:-}
if [ -z "$app" ]; then
	app="/Applications/FX Steam Launcher.app"
	[ -d "$app" ] || app=$(mdfind "kMDItemCFBundleIdentifier == 'es.fxgam.steamac'" 2> /dev/null | head -n 1)
fi
[ -n "$app" ] && [ -d "$app/Contents" ] || die "no steamac app found; set STEAMAC_APP to its .app"
fw=$app/Contents/Frameworks
lib=$fw/$name
exe_rel=Contents/MacOS/$(defaults read "$app/Contents/Info.plist" CFBundleExecutable)
# Files --resign rewrites (relative to the app); the dylib is always backed up
sig_files="$exe_rel Contents/_CodeSignature/CodeResources Contents/CodeResources"

authority() { codesign -dvv "$1" 2>&1 | sed -n 's/^Authority=//p' | head -n 1; }
signature() { a=$(authority "$1"); if [ -n "$a" ]; then echo "$a"; elif codesign -dv "$1" 2>&1 | grep -q 'Signature=adhoc'; then echo "ad hoc"; else echo "unsigned"; fi; }
sha() { shasum -a 256 "$1" | cut -c1-16; }
vm_running() { pgrep -f "$app/$exe_rel" > /dev/null 2>&1; }

# macOS (App Management, Privacy & Security) only lets allowed apps write into another app's bundle;
# a blocked write fails with "Operation not permitted" even for the bundle's owner.
check_writable() {
	probe=$fw/.swap-probe.$$
	if (: > "$probe") 2> /dev/null; then
		rm -f "$probe"
		return
	fi
	die "macOS does not let this process write into $app (App Management).
Allow the app you run this script from (Terminal, iTerm, VS Code, ...) in
System Settings > Privacy & Security > App Management, quit and reopen it, and run this again.
Nothing was changed."
}

# Copy to a temp file next to dst and rename over it (a mapped dylib is never rewritten in place)
install_file() {
	cp -p "$1" "$2.swap.$$"
	mv -f "$2.swap.$$" "$2"
}

status() {
	echo "app:       $app ($(defaults read "$app/Contents/Info.plist" CFBundleShortVersionString 2> /dev/null || echo '?'))"
	echo "driver:    $(sha "$lib")  signed: $(signature "$lib")"
	if [ -f "$backup/$name" ]; then
		echo "original:  $(sha "$backup/$name")  (backup in $backup)"
		if cmp -s "$lib" "$backup/$name"; then echo "state:     original driver"; else echo "state:     SWAPPED"; fi
	else
		echo "original:  no backup yet (nothing swapped)"
	fi
	echo "app sig:   $(signature "$app")"
	if codesign --verify --deep --strict "$app" 2> /dev/null; then
		echo "seal:      valid"
	else
		echo "seal:      does not match (expected while swapped without --resign)"
	fi
	if vm_running; then echo "VM:        running"; else echo "VM:        not running"; fi
}

cmd=${1:-status}
[ $# -gt 0 ] && shift
resign=0 force=0 new=
for a in "$@"; do
	case $a in
	--resign) resign=1 ;;
	--force) force=1 ;;
	-*) die "unknown option $a" ;;
	*) new=$a ;;
	esac
done

case $cmd in
status)
	status
	;;
swap)
	new=${new:-$root/work/out/host/lib/$name}
	[ -f "$new" ] || die "no driver at $new (build it: host/kosmickrisp/build.sh)"
	[ "$(otool -D "$new" | sed -n 2p)" = "@rpath/$name" ] || die "$new: install name is not @rpath/$name"
	nm -gU "$new" | grep -q ' _vk_icdGetInstanceProcAddr$' || die "$new: not a Vulkan ICD (no vk_icdGetInstanceProcAddr)"
	[ "$force" = 1 ] || ! vm_running || die "the VM is running; quit it first (or --force: it keeps the old driver until it restarts)"
	check_writable

	mkdir -p "$backup"
	if [ ! -f "$backup/$name" ]; then
		# Never take a driver that is not the release's for the original
		case $(signature "$lib") in
		"ad hoc" | unsigned) die "$lib is not the release's signed driver and there is no backup of the original in $backup" ;;
		esac
		cp -p "$lib" "$backup/$name"
		echo ">> backed up the original driver to $backup"
	fi
	if [ "$resign" = 1 ]; then
		for f in $sig_files; do
			[ -f "$app/$f" ] || continue
			mkdir -p "$backup/$(dirname "$f")"
			[ -f "$backup/$f" ] || cp -p "$app/$f" "$backup/$f"
		done
		[ -f "$backup/entitlements.plist" ] ||
			codesign -d --entitlements :- "$backup/$exe_rel" > "$backup/entitlements.plist" 2> /dev/null
	fi

	# The driver must be signed (ad hoc is enough) for an arm64 process to map it
	tmp=$fw/$name.swap.$$
	cp "$new" "$tmp"
	codesign -v "$tmp" 2> /dev/null || codesign --force -s - "$tmp"
	mv -f "$tmp" "$lib"
	echo ">> installed $new ($(sha "$lib"))"

	if [ "$resign" = 1 ]; then
		codesign --force -s - --options runtime --entitlements "$backup/entitlements.plist" "$app"
		codesign --verify --strict "$app"
		echo ">> re-signed the app ad hoc (macOS will ask for microphone/keychain access again)"
	fi
	if [ -f "$root/work/out/host/KOSMICKRISP.txt" ] && [ "$new" = "$root/work/out/host/lib/$name" ]; then
		sed -n 's/^patch revision: *//p' "$root/work/out/host/KOSMICKRISP.txt" | sed 's/^/>> patch revision: /'
	fi
	if vm_running; then echo ">> the running VM still uses the old driver until it restarts"; fi
	echo ">> select KosmicKrisp in the app's settings for it to be used; restore with: $0 restore"
	;;
restore)
	[ -f "$backup/$name" ] || die "no backup in $backup: nothing to restore"
	[ "$force" = 1 ] || ! vm_running || die "the VM is running; quit it first (or --force)"
	check_writable
	install_file "$backup/$name" "$lib"
	for f in $sig_files; do
		if [ -f "$backup/$f" ]; then install_file "$backup/$f" "$app/$f"; fi
	done
	if codesign --verify --deep --strict "$app" 2> /dev/null; then
		echo ">> restored the original driver; the app's signature is valid ($(signature "$app"))"
	else
		die "restored the backed-up files, but the app's signature does not verify: reinstall the app"
	fi
	;;
*)
	die "usage: $0 swap [--resign] [--force] [dylib] | restore [--force] | status"
	;;
esac
