#!/bin/bash
# One command for the desktop ISO size round (AGENTS.md, "Desktop ISO size"): build the recipes that changed, build both
# ISOs, run the install tests, print the sizes.
#
#   scripts/size-round.sh            everything
#   scripts/size-round.sh packages   only the packages
#   scripts/size-round.sh isos       only the ISOs (after the packages)
#   scripts/size-round.sh tests      only the QEMU tests (the ISOs must exist)
#
# Run it in WSL by absolute path, in a checkout that is on this branch, with the signing key plugged in (MELON_SIGN_KEY).
# The PC must not sleep. Log: logs/size-round.log. Re-running skips what is already built.
. "$(dirname "$(readlink -f "$0")")/env.sh"
cd "$M"; mkdir -p logs out
exec > >(tee -a logs/size-round.log) 2>&1
step=${1:-all}

packages(){
  echo "##### $(date +%T) packages that changed"
  MELON_AUTO_RESUME=1 scripts/build-all.sh linux-firmware breeze melon-base calamares-melon
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
