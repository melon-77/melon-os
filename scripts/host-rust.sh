#!/bin/bash
# host-rust.sh: the Rust toolchain recipes use to cross-compile Rust code for melon, installed to hosttools/rust.
# Upstream's release binaries for the build machine (rustc, cargo, std) plus the standard library for melon's target
# (x86_64-unknown-linux-musl). Same version as recipes/rust; nothing from here ships in melon.
# melon-build links Rust programs with melon's gcc and dynamically against melon's musl (see "Rust" in melon-build).
set -euo pipefail
M=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd); V=1.98.1; H=$M/hosttools/rust; W=$M/work/host-rust
if [ -x $H/bin/rustc ] && $H/bin/rustc --version | grep -q "^rustc $V "; then echo "host rust $V already installed"; exit 0; fi
rm -rf $W $H; mkdir -p $W; cd $W
for c in rustc-$V-x86_64-unknown-linux-gnu cargo-$V-x86_64-unknown-linux-gnu rust-std-$V-x86_64-unknown-linux-gnu \
         rust-std-$V-x86_64-unknown-linux-musl; do
  tar xJf $M/sources/$c.tar.xz
  $c/install.sh --prefix=$H --disable-ldconfig --without=rust-docs >/dev/null
done
cd /; rm -rf $W
$H/bin/rustc --version
$H/bin/rustc --print target-list >/dev/null
[ -d $H/lib/rustlib/x86_64-unknown-linux-musl/lib ] || { echo "host-rust: the musl standard library is missing" >&2; exit 1; }
echo "host rust: $H"
