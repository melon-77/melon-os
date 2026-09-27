#!/bin/bash
# build a list of recipes in order; stop at the first failure
# (MELON_KEEP_GOING=1: note the failure and carry on with the rest; the list of failures is printed at the end)
. "$(dirname "$(readlink -f "$0")")/env.sh"
failed=()
# MELON_SKIP_BUILT=1: skip recipes whose current version-release is already in the repo (to resume a queue)
built(){ ( pkgrel=0; . $M/recipes/$1/MELONBUILD >/dev/null 2>&1; [ -f $REPO/$APK_ARCH/$pkgname-$pkgver-r$pkgrel.apk ] ); }
for p in "$@"; do
  if [ "${MELON_SKIP_BUILT:-0}" = 1 ] && built $p; then echo "##### $p already built"; continue; fi
  echo "##### $(date +%T) $p"
  if ! $M/scripts/melon-build $p > $M/logs/pkg-$p.log 2>&1; then
    echo "##### FAILED $p"; tail -25 $M/logs/pkg-$p.log
    [ "${MELON_KEEP_GOING:-0}" = 1 ] || exit 1
    failed+=("$p"); continue
  fi
  grep 'created' $M/logs/pkg-$p.log | sed 's/\x1b\[[0-9;]*m//g'
done
[ ${#failed[@]} -eq 0 ] || { echo "##### FAILED: ${failed[*]}"; exit 1; }
echo "##### ALL DONE $(date +%T)"
