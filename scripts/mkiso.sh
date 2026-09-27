#!/bin/bash
# mkiso.sh: assemble the melon live/installer ISO (BIOS + UEFI hybrid) from the melon package repo.
#
# The ISO carries two squashfs layers:
#   melon/rootfs.sqfs  a pristine, apk-installed melon system (exactly what gets installed)
#   melon/live.sqfs    the small live-session layer on top (autologin, hostname, live services, sounds)
# The initramfs stacks them with a RAM overlay. The installers copy rootfs.sqfs to disk (fast, and no
# second copy of every package on the ISO) and use the repo on the ISO only for extra packages.
set -euo pipefail
. /home/claude/melon/scripts/env.sh
APK=$M/hosttools/bin/apk
ROOT=$WORK/liveroot LIVE=$WORK/livelayer ISO=$WORK/iso INITRD=$WORK/initrd
DATE=$(date +%Y%m%d)
OUTISO=$M/out/melon-$DATE-x86_64.iso
PKGS=${PKGS:-"melon-base linux-melon linux-firmware wpa_supplicant xfsprogs grub zstd openssl ncurses-terminfo musl-utils alsa-utils mpg123 kmod"}
DESKTOP_PKGS=${DESKTOP_PKGS:-}
step(){ printf '\033[1;35m== %s\033[0m\n' "$*"; }
apkx(){ $APK --arch x86_64 --keys-dir $M/keys/trusted --repositories-file /dev/null --repository $REPO/x86_64/Packages.adb --no-cache "$@"; }

step "system layer (what gets installed)"
rm -rf $ROOT $LIVE $ISO $INITRD; mkdir -p $ROOT $LIVE $ISO/boot/grub $ISO/melon $INITRD
apkx --root $ROOT --initdb add $PKGS
rm -rf $ROOT/var/cache/apk/*
mkdir -p $ROOT/usr/share/melon/profiles
printf '%s\n' $PKGS > $ROOT/usr/share/melon/profiles/base
[ -n "$DESKTOP_PKGS" ] && printf '%s\n' $PKGS $DESKTOP_PKGS > $ROOT/usr/share/melon/profiles/desktop
cp $ROOT/boot/vmlinuz-melon $ISO/boot/vmlinuz

step "live layer"
mkdir -p $LIVE/etc/sv/getty-tty1 $LIVE/etc/sv/getty-ttyS0 $LIVE/var/service $LIVE/etc/apk $LIVE/usr/share/melon
echo melon-live > $LIVE/etc/hostname
sed 's/^root:[^:]*:/root::/' $ROOT/etc/shadow > $LIVE/etc/shadow; chmod 640 $LIVE/etc/shadow
echo 'GETTY_ARGS="-n -l /usr/bin/melon-autologin"' > $LIVE/etc/sv/getty-tty1/conf
echo 'GETTY_ARGS="-n -l /usr/bin/melon-autologin"' > $LIVE/etc/sv/getty-ttyS0/conf
for s in getty-tty1 getty-tty2 getty-tty3 getty-ttyS0 mdevd syslogd klogd dhcp; do ln -sfn /etc/sv/$s $LIVE/var/service/$s; done
echo "/media/melon/melon/repo/x86_64/Packages.adb" > $LIVE/etc/apk/repositories
install -m644 $M/recipes/melon-sounds/ice.mp3 $LIVE/usr/share/melon/.ice
cat > $LIVE/etc/motd <<'MOTD'

  Welcome to the melon live system.

    melonfetch        show system info
    melon-svc list    list services
    wpa_passphrase "SSID" "password" >> /etc/wpa_supplicant/wpa_supplicant.conf
    melon-svc enable wpa_supplicant dhcp-wifi      connect to Wi-Fi

MOTD

step "squashfs"
mksquashfs $ROOT $ISO/melon/rootfs.sqfs -comp zstd -Xcompression-level 15 -noappend -quiet
mksquashfs $LIVE $ISO/melon/live.sqfs -comp zstd -Xcompression-level 15 -noappend -quiet -all-root

step "package repository (extras only; the base system comes from rootfs.sqfs)"
mkdir -p $ISO/melon/repo/x86_64
if [ -n "$DESKTOP_PKGS" ]; then
  apkx fetch --recursive --output $ISO/melon/repo/x86_64 $DESKTOP_PKGS >/dev/null
fi
# always carry the small, commonly wanted extras so an offline install can still add them
apkx fetch --recursive --output $ISO/melon/repo/x86_64 melon-base bash busybox musl apk-tools >/dev/null
( cd $ISO/melon/repo/x86_64 && $APK --keys-dir $M/keys/trusted --sign-key $M/keys/melon-signing.rsa mkndx -d "melon $DATE" -o Packages.adb *.apk )

step "initramfs"
mkdir -p $INITRD/bin $INITRD/lib $INITRD/dev
cp $ROOT/usr/bin/busybox $INITRD/bin/busybox; ln -s busybox $INITRD/bin/sh
cp $ROOT/usr/lib/libc.so $INITRD/lib/ld-musl-x86_64.so.1
cp $M/iso-files/init $INITRD/init
(cd $INITRD && find . | cpio -o -H newc --quiet | zstd -q -19 > $ISO/boot/initramfs.img)

step "grub"
cat > $ISO/boot/grub/grub.cfg <<'CFG'
set timeout=5
set default=0
serial --unit=0 --speed=115200
terminal_input console serial
terminal_output console serial
insmod all_video
set menu_color_normal=light-gray/black
set menu_color_highlight=black/light-magenta
menuentry 'melon live' {
  linux /boot/vmlinuz quiet
  initrd /boot/initramfs.img
}
menuentry 'melon live (copy to RAM)' {
  linux /boot/vmlinuz quiet toram
  initrd /boot/initramfs.img
}
menuentry 'melon live (safe graphics)' {
  linux /boot/vmlinuz nomodeset
  initrd /boot/initramfs.img
}
menuentry 'melon live (serial console)' {
  linux /boot/vmlinuz console=tty0 console=ttyS0,115200
  initrd /boot/initramfs.img
}
CFG
mkdir -p $M/out; rm -f $M/out/melon-*-x86_64.iso
$M/hosttools/grub/bin/grub-mkrescue -o $OUTISO $ISO -- -volid MELON 2>&1 | grep -v -E '^xorriso|^Drive|^Media|^libisofs|^Added|^ISO image|^Writing|^Written|^$' || true
( cd $M/out && sha256sum $(basename $OUTISO) > SHA256SUMS )
ls -la $OUTISO
