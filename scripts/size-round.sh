#!/bin/bash
# One command for the desktop ISO size round (AGENTS.md, "Desktop ISO size goal"): build what changed, rebuild every
# package that has translations to split off (rule 61), build both ISOs, run the install tests, print the sizes.
#
#   scripts/size-round.sh            everything
#   scripts/size-round.sh packages   only the packages
#   scripts/size-round.sh isos       only the ISOs (after the packages)
#   scripts/size-round.sh tests      only the QEMU tests (the ISOs must exist)
#
# Run it in WSL by absolute path, in a checkout that is on this branch, with the signing key plugged in (MELON_SIGN_KEY).
# The PC must not sleep. Rebuilding the translation packages reuses their source trees where work/ still has them;
# a package whose tree is gone is built from scratch (Qt and KDE ones take hours), so start it in the evening.
# Log: logs/size-round.log. Re-running skips what is already built.
. "$(dirname "$(readlink -f "$0")")/env.sh"
cd "$M"; mkdir -p logs out
exec > >(tee -a logs/size-round.log) 2>&1
step=${1:-all}
KEEP=$(grep -v '^#' recipes/melon-base/files/usr/share/melon/lang-keep | tr '\n' ' ')
APK=$M/hosttools/bin/apk

packages(){
  echo "##### $(date +%T) packages that changed"
  MELON_AUTO_RESUME=1 scripts/build-all.sh linux-firmware breeze melon-base melon-lang-more calamares-melon
  echo "##### $(date +%T) packages with translations to split off"
  local list=() warn=() f n r lang base
  for f in $REPO/$APK_ARCH/*.apk; do
    [ -s "$f" ] || continue
    n=$(basename "$f" | sed 's/-[0-9][^-]*-r[0-9]*\.apk$//')
    case $n in *-lang-more|*-doc) continue ;; esac
    # the languages this package ships: usr/share/locale/<lang>/ in its file list
    lang=$($APK manifest "$f" 2>/dev/null | sed -n 's|.*usr/share/locale/\([^/]*\)/.*|\1|p' | sort -u) || true
    [ -n "$lang" ] || continue
    local other=
    for l in $lang; do base=${l%%[_@.]*}; case " en $KEEP " in *" $base "*) ;; *) other=1; break ;; esac; done
    [ -n "$other" ] || continue
    r=$n
    while [ ! -d recipes/$r ] && [ "$r" != "${r%-*}" ]; do r=${r%-*}; done   # subpackage -> its recipe
    if [ -d recipes/$r ]; then list+=("$r"); else warn+=("$n"); fi
  done
  list=($(printf '%s\n' "${list[@]}" | sort -u))
  echo "${#list[@]} recipes to rebuild: ${list[*]}"
  [ ${#warn[@]} -eq 0 ] || echo "no recipe found for (build their recipe by hand): ${warn[*]}"
  [ ${#list[@]} -gt 0 ] || { echo "none found: is '$APK manifest' working on a v3 .apk? Try: $APK manifest <file>.apk"; return 1; }
  # packages that were not rebuilt this round yet: their file is older than melon-build's split (newer than this script's marker)
  local mark=$M/work/.size-round-started
  [ -f $mark ] || touch $mark
  local todo=() p
  for p in "${list[@]}"; do
    find $REPO/$APK_ARCH -maxdepth 1 -name "$p-[0-9]*.apk" -newer $mark | grep -q . || todo+=("$p"); done
  echo "${#todo[@]} still to do"
  MELON_AUTO_RESUME=1 MELON_KEEP_GOING=1 scripts/build-all.sh "${todo[@]}"
}
isos(){
  echo "##### $(date +%T) console ISO";  scripts/mkiso.sh
  echo "##### $(date +%T) desktop ISO";  MELON_EDITION=desktop scripts/mkiso.sh
  ls -l out/*.iso
}
tests(){
  local iso desk disk=/tmp/size-round-disk.img
  iso=$(ls -t out/melon-[0-9]*-x86_64.iso 2>/dev/null | head -n1); desk=$(ls -t out/melon-desktop-*-x86_64.iso | head -n1)
  rm -f $disk; qemu-img create -f raw $disk 12G
  scripts/qemu-test.py live "$iso" $disk && scripts/qemu-test.py disk $disk && scripts/qemu-test.py disk $disk --uefi \
    && echo "console ISO: PASS" || echo "console ISO: FAIL (see logs/qemu-*.log)"
  rm -f $disk; qemu-img create -f raw $disk 16G
  scripts/qemu-test.py desktop-install "$desk" $disk && echo "desktop ISO: PASS" \
    || echo "desktop ISO: FAIL or no KVM (Plasma can't finish on Windows 10; see logs/qemu-*.log)"
}
case $step in
  all) packages; isos; tests ;;
  packages) packages ;; isos) isos ;; tests) tests ;;
  *) echo "usage: $0 [all|packages|isos|tests]"; exit 1 ;;
esac
echo "##### $(date +%T) done"
