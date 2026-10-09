#!/bin/bash
# second half of the toolchain: musl with the stage-1 compiler, then the full GCC
set -euo pipefail
. "$(dirname "$(readlink -f "$0")")/env.sh"
cd $WORK/toolchain
echo "=== $(date +%T) musl"
cd musl-1.2.5
./configure --target=$TARGET --prefix=/usr --syslibdir=/usr/lib CROSS_COMPILE=$TARGET- CC=$TARGET-gcc >/dev/null
make -j$JOBS >/dev/null; make DESTDIR=$SYSROOT install >/dev/null; cd ..
ln -sf libc.so $SYSROOT/usr/lib/$MUSL_LDSO
# i686: position-independent code calls the hidden __stack_chk_fail_local, which every program and library links in
# from libssp_nonshared.a (gcc's -lssp_nonshared, patches/gcc-i686); musl doesn't make one, Alpine's musl package does
if [ "$MUSL_ARCH" = i386 ]; then
  printf 'extern void __stack_chk_fail(void);\nvoid __attribute__((visibility("hidden"))) __stack_chk_fail_local(void) { __stack_chk_fail(); }\n' > ssp-local.c
  $TARGET-gcc -O2 -fPIC -fno-stack-protector -c ssp-local.c -o ssp-local.o && $TARGET-ar rcs $SYSROOT/usr/lib/libssp_nonshared.a ssp-local.o
fi
echo "=== $(date +%T) gcc final"
cd b-gcc; rm -rf $TARGET/libgcc; make -j$JOBS >/dev/null 2>../gcc-final.err || { tail -30 ../gcc-final.err; exit 1; }
make install >/dev/null; cd ..
echo "=== $(date +%T) done"
$TARGET-gcc --version | head -1
