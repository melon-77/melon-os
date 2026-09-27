# melon build environment
#   MELON_ARCH=x86_64 (default) or MELON_ARCH=x86 (32-bit, i686)
export M=${M:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
export MELON_ARCH=${MELON_ARCH:-x86_64}
case $MELON_ARCH in
  x86_64) export TARGET=x86_64-melon-linux-musl APK_ARCH=x86_64 ARCH_SUFFIX= MUSL_ARCH=x86_64 KARCH=x86_64 \
                 MUSL_LDSO=ld-musl-x86_64.so.1 GCC_ARCH=x86-64 MESON_CPU_FAMILY=x86_64 MESON_CPU=x86_64 EFI_TARGET=x86_64-efi EFI_BOOT=BOOTX64.EFI ;;
  x86)    export TARGET=i686-melon-linux-musl APK_ARCH=x86 ARCH_SUFFIX=-x86 MUSL_ARCH=i386 KARCH=i386 \
                 MUSL_LDSO=ld-musl-i386.so.1 GCC_ARCH=i686 MESON_CPU_FAMILY=x86 MESON_CPU=i686 EFI_TARGET=i386-efi EFI_BOOT=BOOTIA32.EFI ;;
  *) echo "unknown MELON_ARCH $MELON_ARCH" >&2; return 1 2>/dev/null || exit 1 ;;
esac
export TOOLS=$M/tools$ARCH_SUFFIX
export SYSROOT=$M/sysroot$ARCH_SUFFIX
export SRC=$M/sources
export WORK=$M/work$ARCH_SUFFIX
export REPO=$M/repo
export PATH=$TOOLS/bin:$PATH
export JOBS=${JOBS:-$(nproc)}   # JOBS=10 scripts/... to use fewer (RAM: about 1 GB per job for Qt/KDE)
