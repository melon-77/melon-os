# melon build environment
#   MELON_ARCH=x86_64 (default) or MELON_ARCH=x86 (32-bit, i686)
export M=${M:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
export MELON_ARCH=${MELON_ARCH:-x86_64}
case $MELON_ARCH in
  x86_64) export TARGET=x86_64-melon-linux-musl APK_ARCH=x86_64 ARCH_SUFFIX= MUSL_ARCH=x86_64 KARCH=x86_64 \
                 MUSL_LDSO=ld-musl-x86_64.so.1 GCC_ARCH=x86-64 MESON_CPU_FAMILY=x86_64 MESON_CPU=x86_64 EFI_TARGET=x86_64-efi EFI_BOOT=BOOTX64.EFI \
                 RUST_TARGET=x86_64-unknown-linux-musl ;;
  x86)    export TARGET=i686-melon-linux-musl APK_ARCH=x86 ARCH_SUFFIX=-x86 MUSL_ARCH=i386 KARCH=i386 \
                 MUSL_LDSO=ld-musl-i386.so.1 GCC_ARCH=i686 MESON_CPU_FAMILY=x86 MESON_CPU=i686 EFI_TARGET=i386-efi EFI_BOOT=BOOTIA32.EFI \
                 RUST_TARGET=i686-unknown-linux-musl ;;
  *) echo "unknown MELON_ARCH $MELON_ARCH" >&2; return 1 2>/dev/null || exit 1 ;;
esac
export TOOLS=$M/tools$ARCH_SUFFIX
export SYSROOT=$M/sysroot$ARCH_SUFFIX
export SRC=$M/sources
export WORK=$M/work$ARCH_SUFFIX
export REPO=$M/repo
export PATH=$TOOLS/bin:$PATH
# The build machine: Ubuntu (or WSL2 Ubuntu), or melon itself (BUILDING.md, "melon as the build machine").
# On melon, `gcc -dumpmachine` is melon's own target triple, x86_64-melon-linux-musl; configure scripts would take the
# cross toolchain for a native one, so the build machine calls itself <arch>-pc-linux-musl (AGENTS.md rule 53).
export MELON_HOST=$( (. /etc/os-release 2>/dev/null && echo "${ID:-unknown}") || echo unknown)
BUILD_TRIPLE=$(gcc -dumpmachine 2>/dev/null || echo "$(uname -m)-pc-linux-gnu")
case $BUILD_TRIPLE in *-melon-linux-*) BUILD_TRIPLE=$(uname -m)-pc-linux-musl ;; esac
export BUILD_TRIPLE
# the cross toolchain's binutils and gcc: say which machine they run on when that's melon
TOOLCHAIN_HOST_FLAGS=; [ "$MELON_HOST" = melon ] && TOOLCHAIN_HOST_FLAGS="--build=$BUILD_TRIPLE --host=$BUILD_TRIPLE"
export TOOLCHAIN_HOST_FLAGS
export JOBS=${JOBS:-$(nproc)}   # JOBS=10 scripts/... to use fewer (RAM: about 1 GB per job for Qt/KDE)
