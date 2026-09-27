#!/bin/bash
# run long builds back to back so the 2 cores never sit idle
. "$(dirname "$(readlink -f "$0")")/env.sh"
until grep -q '^EXIT' $M/logs/build-llvm.log; do sleep 60; done
grep -q '^EXIT 0' $M/logs/build-llvm.log || { echo "llvm failed; stopping queue"; exit 1; }
$M/scripts/build-all.sh mesa libepoxy xwayland > $M/logs/build-mesa.log 2>&1; echo "EXIT $?" >> $M/logs/build-mesa.log
$M/scripts/host-qt.sh > $M/logs/host-qt.log 2>&1; echo "EXIT $?" >> $M/logs/host-qt.log
echo QUEUE-1-DESKTOP-PART-DONE

# ---- 32-bit (i686) console edition ----
export MELON_ARCH=x86
. "$(dirname "$(readlink -f "$0")")/env.sh"
ln -sf $SYSROOT/usr/lib/libc.so /lib/ld-musl-i386.so.1
echo $SYSROOT/usr/lib > /etc/ld-musl-i386.path
$M/scripts/toolchain.sh > $M/logs/toolchain-x86.log 2>&1; echo "EXIT $?" >> $M/logs/toolchain-x86.log
grep -q '^EXIT 0' $M/logs/toolchain-x86.log || { echo "x86 toolchain failed"; exit 1; }
$M/hosttools/bin/apk --root $SYSROOT --arch x86 --initdb --keys-dir $M/keys/trusted --repositories-file /dev/null add >/dev/null 2>&1
$M/scripts/build-all.sh melon-layout linux-headers musl gcc-runtime zlib zstd openssl apk-tools busybox ncurses bash runit \
   util-linux userspace-rcu inih bsd-compat-headers xfsprogs grub libnl3 wpa_supplicant alsa-lib alsa-utils mpg123 kmod \
   opendoas linux-firmware melon-base melon-sounds linux-melon > $M/logs/build-x86.log 2>&1; echo "EXIT $?" >> $M/logs/build-x86.log
$M/scripts/hostgrub.sh > $M/logs/hostgrub-x86.log 2>&1
$M/scripts/mkiso.sh > $M/logs/mkiso-x86.log 2>&1; echo "EXIT $?" >> $M/logs/mkiso-x86.log
echo QUEUE-1-ALL-DONE
