#!/bin/bash
# build-everything.sh: build melon from nothing on a prepared host (scripts/host-setup.sh first).
#   toolchain -> base system -> plumbing -> libraries and services -> Qt/KDE/Plasma -> installers -> ISOs
# Resumable: run it again after an interruption; finished packages are skipped and a package that was
# interrupted mid-build continues where it stopped. Needs keys/melon-signing.rsa (see BUILDING.md).
#   JOBS=16 scripts/build-everything.sh
#   MELON_ARCH=x86 scripts/build-everything.sh    the 32-bit edition: LXQt on labwc instead of Plasma (X86 below)
set -uo pipefail
. "$(dirname "$(readlink -f "$0")")/env.sh"
LOG=$M/logs; mkdir -p $LOG $REPO/$APK_ARCH
step(){ printf '\033[1;35m== %s  %s\033[0m\n' "$(date +%H:%M)" "$*"; }
[ -f $M/keys/melon-signing.rsa ] || { echo "keys/melon-signing.rsa is missing (see BUILDING.md)" >&2; exit 1; }

if [ ! -x $TOOLS/bin/$TARGET-gcc ] || ! grep -q '^EXIT 0' $LOG/toolchain$ARCH_SUFFIX.log 2>/dev/null; then
  step "cross toolchain ($TARGET)"
  $M/scripts/toolchain.sh > $LOG/toolchain$ARCH_SUFFIX.log 2>&1; echo "EXIT $?" >> $LOG/toolchain$ARCH_SUFFIX.log
  grep -q '^EXIT 0' $LOG/toolchain$ARCH_SUFFIX.log || { echo "toolchain failed, see logs/toolchain$ARCH_SUFFIX.log"; exit 1; }
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
# written in Rust (options=(rust)), cross-compiled with the build machine's Rust from scripts/host-rust.sh
RUST="ripgrep"
# rustc and cargo that run on melon (recipes/rust): needs melon's llvm (DESKTOP_LIBS), gcc and binutils (DEVTOOLS)
RUSTC="rust"
DESKTOP_LIBS="libbytesize libnvme libatasmart libblockdev udisks2 pulseaudio libdaemon avahi cups qpdf poppler libcupsfilters libppd
  cups-filters modemmanager qrencode zxing-cpp opencv llvm mesa libepoxy xkbcomp xwayland vulkan-loader libvpx x264 libwebp libdmtx ffmpeg gamemode melon-fonts
  sdl3 sdl3-image sdl3-ttf melon-pinball
  qemu-guest-agent open-vm-tools hvtools melon-vm-guest"
# AppStream has Qt bindings (Discover), and Flatpak and the portal build against AppStream: right after Qt
KDE=$(python3 $M/scripts/gen-kde-recipes.py | sed "s/\bqt6-qtbase\b/qt6-qtbase appstream flatpak xdg-desktop-portal/")
INSTALLERS="kpmcore calamares calamares-melon melon-desktop"
# a compiler and binutils that run on melon (the cross toolchain in tools/ only runs on the build machine)
DEVTOOLS="mpc binutils gcc make pkgconf patch"
# the build tools melon's own recipes need, so melon can build melon (self-hosting, step 1)
BUILDTOOLS="perl m4 bison flex gawk gperf bc texinfo autoconf automake autoconf-archive file"
# the ones that link curl or libarchive (SIMPLE) come after them
# self-hosting step 2: clang, libclc and the SPIR-V translator (Mesa's OpenCL C shaders), QEMU + OVMF (the tests), Go + gh
STEP2="clang spirv-llvm-translator libclc libslirp qemu ovmf go github-cli"
# NVIDIA: nouveau's firmware and NVK (Rust) for the open driver, and NVIDIA's own kernel modules (online installs)
NVIDIA="linux-firmware-nvidia mesa-nvk nvidia-open"
BUILDTOOLS2="lz4 ninja cmake meson git nasm tcl rsync
  python3-markupsafe python3-jinja2 python3-pyparsing python3-mako python3-yaml python3-packaging python3-ptyprocess python3-pexpect python3-libxml2 itstool
  xorriso mtools scdoc dtc dwarves publicsuffix rpcsvc-proto"
# LXQt on labwc (wlroots): the 32-bit edition's desktop (X86 below); on 64-bit an extra desktop from the online repository
# (scripts/gen-lxqt-recipes.py), with melon's LXQt defaults
LXQT="fribidi cairo pango libsfdo seatd wlroots labwc libexif libfm-extra menu-cache xdg-user-dirs
  $(python3 $M/scripts/gen-lxqt-recipes.py) melon-lxqt"
ALL=$(printf '%s\n' $BASE $PLUMBING $DEVTOOLS $BUILDTOOLS $APPS $GAMES $SIMPLE $BUILDTOOLS2 $GETTEXT $RUST $DESKTOP_LIBS $STEP2 $NVIDIA $RUSTC $KDE $INSTALLERS $LXQT | awk '!seen[$0]++')
# The 32-bit (i686) edition, for old netbooks and laptops (the owner, 7 October 2026: LXQt as its desktop): the base
# system, the libraries and services LXQt, SDDM and Calamares need, Mesa without LLVM, the Qt and KDE Frameworks parts
# LXQt and the installer use, labwc (wlroots) as the Wayland compositor, LXQt. No Plasma, Flatpak, NVIDIA, Rust or
# developer tools: those stay 64-bit.
if [ "$APK_ARCH" = x86 ]; then
  SKIP_X86="spirv-headers spirv-tools glslang libva libvdpau gstreamer gst-plugins-base npth libgpg-error libgcrypt libassuan
    libksba gnupg gpgme gpgmepp bubblewrap xdg-dbus-proxy json-glib ostree libxmlb power-profiles-daemon libseccomp"
  SIMPLE_X86=$(printf '%s\n' $SIMPLE | grep -vxF -f <(printf '%s\n' $SKIP_X86))
  DESKTOP_LIBS_X86="libbytesize libnvme libatasmart libblockdev udisks2 pulseaudio mesa libepoxy xkbcomp xwayland libwebp
    melon-fonts qemu-guest-agent open-vm-tools hvtools melon-vm-guest"
  QT_X86="qt6-qtbase qt6-qtshadertools qt6-qtsvg qt6-qtimageformats qt6-qtdeclarative qt6-qtwayland qt6-qt5compat qt6-qttools
    qt6-qttranslations"
  KDE_X86="extra-cmake-modules plasma-wayland-protocols polkit-qt-1 kf6-kcoreaddons kf6-kconfig kf6-ki18n kf6-kwidgetsaddons
    kf6-kwindowsystem kf6-kguiaddons kf6-kdbusaddons kf6-kcrash kf6-kidletime kf6-solid kf6-breeze-icons layer-shell-qt
    libkscreen sddm"
  ALL=$(printf '%s\n' $BASE $PLUMBING nethack $SIMPLE_X86 $DESKTOP_LIBS_X86 $QT_X86 $KDE_X86 $LXQT kpmcore calamares \
    calamares-melon melon-desktop | awk '!seen[$0]++')
fi
# recipes nobody listed yet go at the end (64-bit only)
if [ "$APK_ARCH" != x86 ]; then
  EXTRA=$(ls $M/recipes | grep -vxF -f <(printf '%s\n' $ALL))
  ALL="$ALL $EXTRA"
fi

step "build order"
$M/scripts/check-order.sh || { echo "   reorder the lists above first"; exit 1; }

# a few passes: a package that failed because something it needs came later in the list gets another go
for pass in 1 2 3; do
  step "packages, pass $pass"
  MELON_AUTO_RESUME=1 MELON_SKIP_BUILT=1 MELON_KEEP_GOING=1 $M/scripts/build-all.sh $ALL > $LOG/everything$ARCH_SUFFIX-$pass.log 2>&1
  failed=$(grep '^##### FAILED: ' $LOG/everything$ARCH_SUFFIX-$pass.log | sed 's/^##### FAILED: //')
  echo "   failed: ${failed:-none}"
  [ -z "$failed" ] && break
  [ $pass -gt 1 ] && [ "$failed" = "$prev" ] && break     # no progress: needs a fix, not another pass
  prev=$failed
done

step "ISOs"
ISOARCH=$([ $APK_ARCH = x86 ] && echo i686 || echo x86_64)
# grub-mkrescue for 32-bit ISOs (i386-pc + i386-efi); host-setup.sh made the 64-bit one
[ -x $M/hosttools/grub$ARCH_SUFFIX/bin/grub-mkrescue ] || $M/scripts/hostgrub.sh > $LOG/hostgrub$ARCH_SUFFIX.log 2>&1
$M/scripts/mkiso.sh > $LOG/mkiso$ARCH_SUFFIX.log 2>&1 && echo "   console ISO: $(ls $M/out/melon-2*-$ISOARCH.iso)"
MELON_EDITION=desktop $M/scripts/mkiso.sh > $LOG/mkiso-desktop$ARCH_SUFFIX.log 2>&1 && echo "   desktop ISO: $(ls $M/out/melon-desktop-*-$ISOARCH.iso)"
[ -z "${failed:-}" ] || { echo "packages that still fail: $failed (logs/pkg-<name>.log)"; exit 1; }
