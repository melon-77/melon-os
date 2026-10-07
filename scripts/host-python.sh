#!/bin/bash
# host-python.sh: build a host Python of the same version as melon's python3 recipe. Cross-compiling
# Python needs one (--with-build-python). Installed to hosttools/python; nothing from here ships in melon.
set -euo pipefail
M=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd); V=3.14.4; H=$M/hosttools/python; W=$M/work/host-python
unset CC CXX CFLAGS CXXFLAGS LDFLAGS PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH
q(){ if [ -n "${LOG:-}" ]; then "$@" >>"$LOG" 2>&1; else "$@" >/dev/null; fi; }   # under host-setup.sh: output into its log
rm -rf $W; mkdir -p $W; cd $W
tar xf $M/sources/Python-$V.tar.xz; cd Python-$V
q ./configure --prefix=$H --with-ensurepip=no --disable-test-modules
q make -j$(nproc)
q make install
cd /; rm -rf $W
$H/bin/python3.14 -c 'import sys, ssl, zlib, ctypes; print("host python", sys.version.split()[0])'
