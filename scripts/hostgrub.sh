#!/bin/bash
# host GRUB used only to assemble ISOs (grub-mkrescue). x86_64 ISOs: i386-pc + x86_64-efi; 32-bit ISOs: i386-pc + i386-efi
set -e
. "$(dirname "$(readlink -f "$0")")/env.sh"
unset CC CXX CFLAGS CXXFLAGS LDFLAGS PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
W=$M/work/hostgrub$ARCH_SUFFIX; rm -rf $W; mkdir -p $W; cd $W; tar xf $SRC/grub-2.14.tar.xz; cd grub-2.14
for p in pc efi; do t=${EFI_TARGET%-efi}; [ $p = pc ] && t=i386
  mkdir -p b-$p; (cd b-$p; ../configure --prefix=$M/hosttools/grub$ARCH_SUFFIX --target=$t --with-platform=$p --disable-werror \
     --disable-nls --disable-grub-mkfont --disable-device-mapper --disable-libzfs --disable-grub-emu-sdl --disable-grub-mount >/dev/null
   make -j$JOBS >/dev/null; make install >/dev/null); done
cp $M/recipes/grub/unicode.pf2 $M/hosttools/grub$ARCH_SUFFIX/share/grub/
rm -rf $W
