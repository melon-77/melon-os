#!/bin/bash
# build-everything.sh: build melon from nothing on a prepared host (scripts/host-setup.sh first).
#   toolchain -> base system -> plumbing -> libraries and services -> Qt/KDE/Plasma -> installers -> ISOs
# Resumable: run it again after an interruption; finished packages are skipped and a package that was
# interrupted mid-build continues where it stopped. Needs keys/melon-signing.rsa (see BUILDING.md).
#   JOBS=16 scripts/build-everything.sh
set -uo pipefail
. "$(dirname "$(readlink -f "$0")")/env.sh"
LOG=$M/logs; mkdir -p $LOG $REPO/$APK_ARCH
step(){ printf '\033[1;35m== %s  %s\033[0m\n' "$(date +%H:%M)" "$*"; }
[ -f $M/keys/melon-signing.rsa ] || { echo "keys/melon-signing.rsa is missing (see BUILDING.md)" >&2; exit 1; }

if [ ! -x $TOOLS/bin/$TARGET-gcc ] || ! grep -q '^EXIT 0' $LOG/toolchain.log 2>/dev/null; then
  step "cross toolchain ($TARGET)"
  $M/scripts/toolchain.sh > $LOG/toolchain.log 2>&1; echo "EXIT $?" >> $LOG/toolchain.log
  grep -q '^EXIT 0' $LOG/toolchain.log || { echo "toolchain failed, see logs/toolchain.log"; exit 1; }
fi
[ -d $SYSROOT/lib/apk/db ] || $M/hosttools/bin/apk --root $SYSROOT --arch $APK_ARCH --initdb --keys-dir $M/keys/trusted \
  --repositories-file /dev/null add >/dev/null 2>&1 || true

BASE="melon-layout linux-headers musl gcc-runtime zlib zstd xz openssl apk-tools busybox ncurses bash runit util-linux
  userspace-rcu inih bsd-compat-headers xfsprogs grub libnl3 expat dbus wpa_supplicant alsa-lib alsa-utils mpg123 kmod
  opendoas ca-certificates linux-firmware melon-base melon-sounds linux-melon"
PLUMBING="libffi pcre2 glib libcap duktape linux-pam eudev elogind polkit argp-standalone musl-fts musl-obstack elfutils
  sqlite json-c popt device-mapper cryptsetup dosfstools squashfs-tools e2fsprogs bzip2 hunspell attr acl lm-sensors libogg libvorbis
  libtool sound-theme-freedesktop libcanberra icu boost-headers python3 readline keyutils gmp mpfr
  opus flac liblc3 libfreeaptx libsndfile flite lua5.4"
# programs shipped on both ISOs (mkiso.sh PKGS); nethack needs ncurses and the same Lua tarball as lua5.4
APPS="nethack"
# in the package repository only: Cataclysm: DDA's game data is too big for the ISOs
GAMES="cataclysm-dda"
SIMPLE=$(python3 $M/scripts/gen-simple-recipes.py)
# GNU gettext (msgfmt, xgettext, ...); it uses the system libxml2 (SIMPLE), so it comes after $SIMPLE
GETTEXT="gettext"
DESKTOP_LIBS="libbytesize libnvme libatasmart libblockdev udisks2 pulseaudio libdaemon avahi cups qpdf poppler libcupsfilters libppd
  cups-filters modemmanager qrencode zxing-cpp opencv llvm mesa libepoxy xkbcomp xwayland vulkan-loader libvpx x264 libwebp libdmtx ffmpeg gamemode melon-fonts
  sdl3 sdl3-image sdl3-ttf melon-pinball
  qemu-guest-agent open-vm-tools hvtools melon-vm-guest"
# AppStream has Qt bindings (Discover), and Flatpak and the portal build against AppStream: right after Qt
KDE=$(python3 $M/scripts/gen-kde-recipes.py | sed "s/\bqt6-qtbase\b/qt6-qtbase appstream flatpak xdg-desktop-portal/")
INSTALLERS="kpmcore calamares calamares-melon melon-desktop"
# a compiler and binutils that run on melon (the cross toolchain in tools/ only runs on the build machine)
DEVTOOLS="mpc binutils gcc make pkgconf patch"
ALL=$(printf '%s\n' $BASE $PLUMBING $DEVTOOLS $APPS $GAMES $SIMPLE $GETTEXT $DESKTOP_LIBS $KDE $INSTALLERS | awk '!seen[$0]++')
# recipes nobody listed yet go at the end
EXTRA=$(ls $M/recipes | grep -vxF -f <(printf '%s\n' $ALL))
ALL="$ALL $EXTRA"

step "build order"
$M/scripts/check-order.sh || { echo "   reorder the lists above first"; exit 1; }

# a few passes: a package that failed because something it needs came later in the list gets another go
for pass in 1 2 3; do
  step "packages, pass $pass"
  MELON_AUTO_RESUME=1 MELON_SKIP_BUILT=1 MELON_KEEP_GOING=1 $M/scripts/build-all.sh $ALL > $LOG/everything-$pass.log 2>&1
  failed=$(grep '^##### FAILED: ' $LOG/everything-$pass.log | sed 's/^##### FAILED: //')
  echo "   failed: ${failed:-none}"
  [ -z "$failed" ] && break
  [ $pass -gt 1 ] && [ "$failed" = "$prev" ] && break     # no progress: needs a fix, not another pass
  prev=$failed
done

step "ISOs"
$M/scripts/mkiso.sh > $LOG/mkiso.log 2>&1 && echo "   console ISO: $(ls $M/out/melon-2*.iso)"
MELON_EDITION=desktop $M/scripts/mkiso.sh > $LOG/mkiso-desktop.log 2>&1 && echo "   desktop ISO: $(ls $M/out/melon-desktop-*.iso)"
[ -z "${failed:-}" ] || { echo "packages that still fail: $failed (logs/pkg-<name>.log)"; exit 1; }
