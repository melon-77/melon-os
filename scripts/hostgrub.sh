#!/bin/bash
# host GRUB used only to assemble ISOs (grub-mkrescue). x86_64 ISOs: i386-pc + x86_64-efi; 32-bit ISOs: i386-pc + i386-efi
set -e
. "$(dirname "$(readlink -f "$0")")/env.sh"
unset CC CXX CFLAGS CXXFLAGS LDFLAGS PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
q(){ if [ -n "${LOG:-}" ]; then "$@" >>"$LOG" 2>&1; else "$@" >/dev/null; fi; }   # under host-setup.sh: output into its log
W=$M/work/hostgrub$ARCH_SUFFIX; rm -rf $W; mkdir -p $W; cd $W; tar xf $SRC/grub-2.14.tar.xz; cd grub-2.14
for p in pc efi; do t=${EFI_TARGET%-efi}; [ $p = pc ] && t=i386
  mkdir -p b-$p; (cd b-$p; q ../configure --prefix=$M/hosttools/grub$ARCH_SUFFIX --target=$t --with-platform=$p --disable-werror \
     ax_cv_check_ldflags___Wl___image_base_0x400000=no \
     --disable-nls --disable-grub-mkfont --disable-device-mapper --disable-libzfs --disable-grub-emu-sdl --disable-grub-mount
   q make -j$JOBS; q make install); done
# (the --image-base answer: see recipes/grub; with ld 2.44+ grub-mkrescue refused the i386-pc kernel.img)
cp $M/recipes/grub/unicode.pf2 $M/hosttools/grub$ARCH_SUFFIX/share/grub/
rm -rf $W
