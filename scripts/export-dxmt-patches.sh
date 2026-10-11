#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# host/dxmt/patches/ is generated: the commits of StonkGuy/dxmt branch "steamac" between the upstream base
# (DXMT_COMMIT) and the pinned fork commit (DXMT_FORK_COMMIT), one patch per commit, authors kept. History,
# credits and the adopt/reject ledger live in that branch (STEAMAC-README.md). Never edit the patches by hand:
# change the fork, move DXMT_FORK_COMMIT in host/dxmt/pin.env, run this.
#   scripts/export-dxmt-patches.sh [OUTDIR]     default OUTDIR: host/dxmt/patches (replaced)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
. "$ROOT/host/dxmt/pin.env"
out=${1:-$ROOT/host/dxmt/patches}
repo=${DXMT_FORK_CHECKOUT:-$ROOT/work/cache/dxmt-fork.git}
[[ -d $repo ]] || git init -q --bare "$repo"
git -C "$repo" cat-file -e "$DXMT_FORK_COMMIT^{commit}" 2>/dev/null \
    || git -C "$repo" fetch -q "$DXMT_FORK_REPO" "$DXMT_FORK_BRANCH" "$DXMT_COMMIT"
git -C "$repo" merge-base --is-ancestor "$DXMT_COMMIT" "$DXMT_FORK_COMMIT" \
    || { echo "DXMT_FORK_COMMIT is not based on DXMT_COMMIT" >&2; exit 1; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# stable output: no signature, no commit ids in the From lines, full index lines
git -C "$repo" format-patch -q --zero-commit --no-signature --full-index -o "$tmp" "$DXMT_COMMIT..$DXMT_FORK_COMMIT"
rm -rf "$out"; mkdir -p "$out"; cp "$tmp"/*.patch "$out"/
echo "exported $(ls "$out" | wc -l | tr -d ' ') patches ($DXMT_COMMIT..$DXMT_FORK_COMMIT) to $out"
