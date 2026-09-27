#!/bin/bash
# mkiso.sh: assemble the melon live/installer ISO (BIOS + UEFI hybrid) from the melon package repo.
set -euo pipefail
. /home/claude/melon/scripts/env.sh
APK=$M/hosttools/bin/apk
ROOT=$WORK/liveroot ISO=$WORK/iso INITRD=$WORK/initrd
DATE=$(date +%Y%m%d)
OUTISO=$M/out/melon-$DATE-x86_64.iso
PKGS=${PKGS:-"melon-base linux-melon linux-firmware wpa_supplicant xfsprogs grub zstd openssl ncurses-terminfo musl-utils alsa-utils"}
LIVE_EXTRA=${LIVE_EXTRA:-"melon-sounds"}
step(){ printf '\033[1;35m== %s\033[0m\n' "$*"; }

step "root filesystem"
rm -rf $ROOT $ISO $INITRD; mkdir -p $ROOT $ISO/boot/grub $ISO/melon $INITRD
$APK --root $ROOT --initdb --arch x86_64 --keys-dir $M/keys/trusted --repositories-file /dev/null \
     --repository $REPO/x86_64/Packages.adb --no-cache add $PKGS $LIVE_EXTRA
echo "$REPO" >/dev/null

step "live tweaks"
echo melon-live > $ROOT/etc/hostname
sed -i 's/^root:[^:]*:/root::/' $ROOT/etc/shadow
echo 'GETTY_ARGS="-n -l /usr/bin/melon-autologin"' > $ROOT/etc/sv/getty-tty1/conf
echo 'GETTY_ARGS="-n -l /usr/bin/melon-autologin"' > $ROOT/etc/sv/getty-ttyS0/conf
for s in getty-tty1 getty-tty2 getty-tty3 getty-ttyS0 mdevd syslogd klogd dhcp; do ln -sfn /etc/sv/$s $ROOT/var/service/$s; done
mkdir -p $ROOT/usr/share/melon/profiles; printf "%s\n" $PKGS > $ROOT/usr/share/melon/profiles/base; [ -n "${DESKTOP_PKGS:-}" ] && printf "%s\n" $PKGS $DESKTOP_PKGS > $ROOT/usr/share/melon/profiles/desktop
echo "/media/melon/melon/repo/x86_64/Packages.adb" > $ROOT/etc/apk/repositories
cat > $ROOT/etc/motd <<'MOTD'

  Welcome to the melon live system.

    melonfetch        show system info
    melon-svc list    list services
    wpa_passphrase "SSID" "password" >> /etc/wpa_supplicant/wpa_supplicant.conf
    melon-svc enable wpa_supplicant dhcp-wifi      connect to Wi-Fi

MOTD
cp $ROOT/boot/vmlinuz-melon $ISO/boot/vmlinuz
rm -rf $ROOT/var/cache/apk/*

step "squashfs"
mksquashfs $ROOT $ISO/melon/rootfs.sqfs -comp zstd -Xcompression-level 15 -noappend -quiet -e boot/vmlinuz-melon boot/System.map-melon
step "package repository"
mkdir -p $ISO/melon/repo/x86_64; cp $REPO/x86_64/*.apk $REPO/x86_64/Packages.adb $ISO/melon/repo/x86_64/

step "initramfs"
mkdir -p $INITRD/bin $INITRD/lib $INITRD/dev
cp $ROOT/usr/bin/busybox $INITRD/bin/busybox
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
ls -la $OUTISO
