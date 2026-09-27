#!/bin/bash
# second half of the toolchain: musl with the stage-1 compiler, then the full GCC
set -euo pipefail
. /home/claude/melon/scripts/env.sh
cd $WORK/toolchain
echo "=== $(date +%T) musl"
cd musl-1.2.5
./configure --target=$TARGET --prefix=/usr --syslibdir=/usr/lib CROSS_COMPILE=$TARGET- CC=$TARGET-gcc >/dev/null
make -j$JOBS >/dev/null; make DESTDIR=$SYSROOT install >/dev/null; cd ..
ln -sf libc.so $SYSROOT/usr/lib/ld-musl-x86_64.so.1
echo "=== $(date +%T) gcc final"
cd b-gcc; rm -rf $TARGET/libgcc; make -j$JOBS >/dev/null 2>../gcc-final.err || { tail -30 ../gcc-final.err; exit 1; }
make install >/dev/null; cd ..
echo "=== $(date +%T) done"
$TARGET-gcc --version | head -1
