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
ISOARCH=$([ $MELON_ARCH = x86 ] && echo i686 || echo x86_64)
OUTISO=$M/out/melon-$DATE-$ISOARCH.iso
PKGS=${PKGS:-"melon-base linux-melon linux-firmware wpa_supplicant xfsprogs grub zstd openssl ca-certificates cryptsetup ncurses-terminfo musl-utils alsa-utils mpg123 kmod"}
# leave out optional packages that haven't been built for this architecture yet (with a warning)
_p=; for x in $PKGS; do
  if ls $M/repo/$APK_ARCH/$x-[0-9]*.apk >/dev/null 2>&1; then _p="$_p $x"; else echo "warning: $x is not built for $APK_ARCH, left out"; fi
done; PKGS=${_p# }
# MELON_EDITION=desktop: the Plasma live ISO. Its system image already contains the desktop (the installers
# copy it as is) plus the live-only installer packages, which both installers remove from the new system.
EDITION=${MELON_EDITION:-console}
DESKTOP_PKGS=${DESKTOP_PKGS:-}
LIVE_ONLY=
if [ "$EDITION" = desktop ]; then
  DESKTOP_PKGS=${DESKTOP_PKGS:-$(grep -v '^#' $M/scripts/desktop-packages.txt | xargs)}
  LIVE_ONLY=${LIVE_ONLY:-calamares-melon}
  OUTISO=$M/out/melon-desktop-$DATE-$ISOARCH.iso
fi
step(){ printf '\033[1;35m== %s\033[0m\n' "$*"; }
apkx(){ $APK --arch $APK_ARCH --keys-dir $M/keys/trusted --repositories-file /dev/null --repository $REPO/$APK_ARCH/Packages.adb --no-cache "$@"; }

step "system layer (what gets installed)"
rm -rf $ROOT $LIVE $ISO $INITRD; mkdir -p $ROOT $LIVE $ISO/boot/grub $ISO/melon $INITRD
if [ "$EDITION" = desktop ]; then
  _p=; for x in $DESKTOP_PKGS $LIVE_ONLY; do
    if ls $M/repo/$APK_ARCH/$x-[0-9]*.apk >/dev/null 2>&1; then _p="$_p $x"; else echo "warning: $x is not built for $APK_ARCH, left out"; fi
  done; DESKTOP_PKGS=${_p# }
  apkx --root $ROOT --initdb add $PKGS $DESKTOP_PKGS
else
  apkx --root $ROOT --initdb add $PKGS
fi
rm -rf $ROOT/var/cache/apk/*
mkdir -p $ROOT/usr/share/melon/profiles
printf '%s\n' $PKGS > $ROOT/usr/share/melon/profiles/base
[ -n "$LIVE_ONLY" ] && printf '%s\n' $LIVE_ONLY > $ROOT/usr/share/melon/profiles/live-only
if [ -n "$DESKTOP_PKGS" ]; then
  printf '%s\n' $PKGS $(printf '%s\n' $DESKTOP_PKGS | grep -vxF "${LIVE_ONLY:-@none@}") > $ROOT/usr/share/melon/profiles/desktop
  # services for the desktop profile: udev replaces mdev, NetworkManager replaces the dhcp/wpa services
  printf '%s\n' -mdevd -dhcp udevd dbus elogind polkitd NetworkManager bluetoothd power-profiles-daemon zram sddm \
    > $ROOT/usr/share/melon/profiles/desktop.services
fi
cp $ROOT/boot/vmlinuz-melon $ISO/boot/vmlinuz

step "live layer"
mkdir -p $LIVE/etc/sv/getty-tty1 $LIVE/etc/sv/getty-ttyS0 $LIVE/var/service $LIVE/etc/apk $LIVE/usr/share/melon
echo melon-live > $LIVE/etc/hostname
sed 's/^root:[^:]*:/root::/' $ROOT/etc/shadow > $LIVE/etc/shadow; chmod 640 $LIVE/etc/shadow
echo 'GETTY_ARGS="-n -l /usr/bin/melon-autologin"' > $LIVE/etc/sv/getty-tty1/conf
echo 'GETTY_ARGS="-n -l /usr/bin/melon-autologin"' > $LIVE/etc/sv/getty-ttyS0/conf
if [ "$EDITION" = desktop ]; then
  for s in getty-tty2 getty-tty3 getty-ttyS0 syslogd klogd udevd dbus elogind polkitd NetworkManager bluetoothd \
           power-profiles-daemon zram sddm qemu-ga vmtoolsd hv_kvp_daemon hv_vss_daemon hv_fcopy_uio_daemon; do ln -sfn /etc/sv/$s $LIVE/var/service/$s; done
  # the live user: logs in automatically to Plasma, may use doas without a password, has the installer on the desktop
  awk -F: '$1!="live"' $ROOT/etc/passwd > $LIVE/etc/passwd; echo 'live:x:1000:1000:melon live:/home/live:/bin/bash' >> $LIVE/etc/passwd
  awk -F: '$1!="live"' $LIVE/etc/shadow > $LIVE/etc/shadow.t; echo 'live::20000:0:99999:7:::' >> $LIVE/etc/shadow.t
  mv $LIVE/etc/shadow.t $LIVE/etc/shadow; chmod 640 $LIVE/etc/shadow
  awk -F: -v OFS=: '$1=="live"{next} $1 ~ /^(wheel|audio|video|input|render|plugdev|netdev|users)$/{$4=($4==""?"live":$4",live")} {print}' \
    $ROOT/etc/group > $LIVE/etc/group; echo 'live:x:1000:' >> $LIVE/etc/group
  mkdir -p $LIVE/home/live/Desktop $LIVE/etc/sddm.conf.d
  cp -a $ROOT/etc/skel/. $LIVE/home/live/
  install -m755 $ROOT/usr/share/applications/melon-install.desktop $LIVE/home/live/Desktop/melon-install.desktop 2>/dev/null || true
  chown -R 1000:1000 $LIVE/home/live
  printf '[Autologin]\nUser=live\nSession=plasma\nRelogin=false\n' > $LIVE/etc/sddm.conf.d/20-live.conf
  { cat $ROOT/etc/doas.conf 2>/dev/null; echo 'permit nopass live'; } > $LIVE/etc/doas.conf; chmod 600 $LIVE/etc/doas.conf
else
  for s in getty-tty1 getty-tty2 getty-tty3 getty-ttyS0 mdevd syslogd klogd dhcp; do ln -sfn /etc/sv/$s $LIVE/var/service/$s; done
fi
echo "/media/melon/melon/repo/$APK_ARCH/Packages.adb" > $LIVE/etc/apk/repositories
install -m644 $M/recipes/melon-sounds/ice.mp3 $LIVE/usr/share/melon/.ice
cat > $LIVE/etc/motd <<'MOTD'

  Welcome to the melon live system.

    melonfetch        show system info
    melon-svc list    list services
    wpa_passphrase "SSID" "password" >> /etc/wpa_supplicant/wpa_supplicant.conf
    melon-svc enable wpa_supplicant dhcp-wifi      connect to Wi-Fi
    melon-repo enable alpine    opt-in Alpine packages (apk add <name>@alpine)

MOTD

step "squashfs"
mksquashfs $ROOT $ISO/melon/rootfs.sqfs -comp zstd -Xcompression-level 15 -noappend -quiet
mksquashfs $LIVE $ISO/melon/live.sqfs -comp zstd -Xcompression-level 15 -noappend -quiet   # built as root; /home/live keeps its owner

step "package repository (extras only; the base system comes from rootfs.sqfs)"
mkdir -p $ISO/melon/repo/$APK_ARCH
if [ -n "$DESKTOP_PKGS" ]; then
  apkx fetch --recursive --output $ISO/melon/repo/$APK_ARCH $DESKTOP_PKGS >/dev/null
fi
# always carry the small, commonly wanted extras so an offline install can still add them
extras="melon-base bash busybox musl apk-tools"
# guest tools, which the installers add when they run inside a VM
ls $M/repo/$APK_ARCH/melon-vm-guest-[0-9]*.apk >/dev/null 2>&1 && extras="$extras melon-vm-guest"
apkx fetch --recursive --output $ISO/melon/repo/$APK_ARCH $extras >/dev/null
( cd $ISO/melon/repo/$APK_ARCH && $APK --keys-dir $M/keys/trusted --sign-key $M/keys/melon-signing.rsa mkndx -d "melon $DATE" -o Packages.adb *.apk )

step "initramfs"
mkdir -p $INITRD/bin $INITRD/lib $INITRD/dev
cp $ROOT/usr/bin/busybox $INITRD/bin/busybox; ln -s busybox $INITRD/bin/sh
cp $ROOT/usr/lib/libc.so $INITRD/lib/$MUSL_LDSO
cp $M/iso-files/init $INITRD/init
(cd $INITRD && find . | cpio -o -H newc --quiet | zstd -q -19 > $ISO/boot/initramfs.img)

step "grub"
cat > $ISO/boot/grub/grub.cfg <<'CFG'
set timeout=5
set default=0
# serial console only where there is a serial port (most VMs and PCs have none)
if serial --unit=0 --speed=115200; then
  terminal_input console serial
  terminal_output console serial
fi
# melon is 64-bit: say so instead of booting into a black screen on a 32-bit CPU or VM
# (VirtualBox VMs created as "Other Linux" 32-bit have no 64-bit mode)
insmod cpuid
if cpuid -l; then set melon_cpu64=1; fi
if [ "$melon_cpu64" != 1 ]; then
  echo "melon needs a 64-bit (x86_64) processor, and this computer or virtual machine doesn't offer one."
  echo ""
  echo "VirtualBox: Settings > General > Basic, set Version to 'Other Linux (64-bit)'."
  echo "  If only 32-bit versions are listed: turn on VT-x / AMD-V (SVM) in the PC's BIOS, and in"
  echo "  Windows turn off Hyper-V / Memory integrity so VirtualBox can use them."
  echo "Other VM apps (VMware, Hyper-V, QEMU): choose a 64-bit Linux guest."
  echo ""
  echo "Press any key to power off."
  sleep --interruptible 3600
  halt
fi
insmod all_video
# the kernel starts in the firmware's own mode (VGA text on BIOS, so its first messages and errors
# stay visible; the GOP framebuffer on UEFI)
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
# desktop edition: the melon GRUB background (black menu backgrounds are transparent over it)
if [ -f $ROOT/usr/share/melon/grub/background.png ]; then
  cp $ROOT/usr/share/melon/grub/background.png $ISO/boot/grub/melon-bg.png
  sed -i 's|^insmod all_video$|insmod all_video\nif loadfont unicode; then set gfxmode=auto; insmod gfxterm; insmod png; terminal_output gfxterm serial; background_image -m stretch /boot/grub/melon-bg.png; fi|' $ISO/boot/grub/grub.cfg
fi
mkdir -p $M/out
if [ "$EDITION" = desktop ]; then rm -f $M/out/melon-desktop-*-$ISOARCH.iso; else rm -f $M/out/melon-2*-$ISOARCH.iso; fi
$M/hosttools/grub$ARCH_SUFFIX/bin/grub-mkrescue -o $OUTISO $ISO -- -volid MELON 2>&1 | grep -v -E '^xorriso|^Drive|^Media|^libisofs|^Added|^ISO image|^Writing|^Written|^$' || true
( cd $M/out && sha256sum melon-*.iso > SHA256SUMS )
ls -la $OUTISO
