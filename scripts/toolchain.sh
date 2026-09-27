#!/bin/bash
# Build the melon cross toolchain for $MELON_ARCH (x86_64-melon-linux-musl or i686-melon-linux-musl).
set -euo pipefail
. "$(dirname "$(readlink -f "$0")")/env.sh"
B=$WORK/toolchain; mkdir -p $B $SYSROOT/usr; cd $B
# melon uses a merged /usr: /lib -> usr/lib, /bin -> usr/bin
ln -sfn usr/lib $SYSROOT/lib; ln -sfn usr/bin $SYSROOT/bin; ln -sfn usr/bin $SYSROOT/sbin; ln -sfn bin $SYSROOT/usr/sbin
mkdir -p $SYSROOT/usr/lib $SYSROOT/usr/bin

step(){ echo "=== $(date +%T) $*"; }

step binutils
if [ ! -x $TOOLS/bin/$TARGET-as ]; then rm -rf b-binutils binutils-with-gold-2.46
tar xf $SRC/binutils-2.46.tar.xz; mkdir b-binutils; cd b-binutils
../binutils-with-gold-2.46/configure --target=$TARGET --prefix=$TOOLS --with-sysroot=$SYSROOT \
  --disable-nls --disable-werror --disable-multilib --disable-gprofng --enable-deterministic-archives --disable-gold >/dev/null
make -j$JOBS >/dev/null; make install >/dev/null; cd ..
fi

step linux headers
rm -rf linux-7.0 gcc-15.2.0 musl-1.2.5 b-gcc
tar xzf $SRC/linux-7.0.tar.gz linux-7.0
make -C linux-7.0 ARCH=$KARCH INSTALL_HDR_PATH=$SYSROOT/usr headers_install >/dev/null

step gcc sources
tar xf $SRC/gcc-15.2.0.tar.xz; cd gcc-15.2.0
tar xf $SRC/gmp-6.3.0.tar.xz && mv gmp-6.3.0+dfsg gmp
# the Debian dfsg tarball drops the docs; stop GMP from expecting them
sed -i "s| doc/Makefile||" gmp/configure; sed -i "s/^SUBDIRS = \(.*\) doc$/SUBDIRS = \1/" gmp/Makefile.in
tar xf $SRC/mpfr-4.2.2.tar.xz && mv mpfr-4.2.2 mpfr
tar xf $SRC/mpc-1.3.1.tar.gz && mv mpc-1.3.1 mpc
cd ..

step musl headers
tar xzf $SRC/musl-1.2.5.tar.gz; cd musl-1.2.5
for p in $M/patches/musl/*.patch; do patch -p1 -s < $p; done
make ARCH=$MUSL_ARCH prefix=/usr DESTDIR=$SYSROOT install-headers >/dev/null; cd ..

GCC_CONF="--target=$TARGET --prefix=$TOOLS --with-sysroot=$SYSROOT --with-build-sysroot=$SYSROOT
  --enable-languages=c,c++ --disable-multilib --disable-nls --disable-werror
  --disable-libsanitizer --disable-libssp --disable-libquadmath --disable-libgomp-offload
  --enable-default-pie --enable-default-ssp --enable-tls --enable-initfini-array
  --enable-libstdcxx-time --enable-__cxa_atexit --enable-threads=posix --enable-shared
  --with-pkgversion=melon --disable-symvers --disable-fixed-point --with-arch=$GCC_ARCH --with-tune=generic"

step gcc stage1
mkdir b-gcc; cd b-gcc
../gcc-15.2.0/configure $GCC_CONF >/dev/null
make -j$JOBS all-gcc >/dev/null; make install-gcc >/dev/null
# libgcc's static parts build fine now; the shared libgcc_s needs the C library, which comes next.
# Install the static pieces by hand so musl can be built with this compiler.
make -j$JOBS all-target-libgcc >/dev/null 2>&1 || true
D=$TOOLS/lib/gcc/$TARGET/15.2.0; L=$TARGET/libgcc
cp $L/libgcc.a $L/libgcc_eh.a $L/crtbegin.o $L/crtbeginS.o $L/crtbeginT.o $L/crtend.o $L/crtendS.o $D/
cd ..

step "musl + final gcc"
exec "$(dirname "$0")/toolchain-finish.sh"
