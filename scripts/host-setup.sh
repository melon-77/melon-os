#!/bin/bash
# host-setup.sh: prepare a fresh Ubuntu 24.04 machine (or WSL2 Ubuntu 24.04) to build melon.
# Run as root (sudo scripts/host-setup.sh). Safe to run again.
#
#  1. build dependencies from Ubuntu
#  2. Ubuntu 26.04 ("resolute") source archive, where most melon sources come from
#  3. let the build host run melon's musl binaries (build-time generators from earlier packages)
#  4. host tools the cross builds need: apk, wayland-scanner 1.24, Mesa's mesa_clc, GRUB, Python, Qt
set -euo pipefail
M=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
[ "$(id -u)" = 0 ] || { echo "run as root: sudo $0" >&2; exit 1; }
. /etc/os-release
[ "$VERSION_ID" = 24.04 ] || echo "warning: tested on Ubuntu 24.04, this is $PRETTY_NAME"
step(){ printf '\033[1;35m== %s\033[0m\n' "$*"; }

step "build dependencies"
export DEBIAN_FRONTEND=noninteractive
apt-get update -q
apt-get install -y -q build-essential bison flex texinfo bc gawk gperf m4 python3 python3-pip python3-venv \
  ninja-build cmake pkg-config autoconf automake autopoint libtool libltdl-dev gettext patch file \
  xz-utils zstd lz4 bzip2 cpio rsync unzip wget curl git ca-certificates dpkg-dev \
  xorriso mtools dosfstools xfsprogs squashfs-tools qemu-system-x86 ovmf \
  libssl-dev zlib1g-dev libzstd-dev libelf-dev dwarves kmod scdoc \
  tcl hwdata publicsuffix rpcsvc-proto device-tree-compiler glslang-tools spirv-tools xsltproc \
  llvm-19-dev libclang-19-dev libclang-cpp19-dev libclc-19-dev libllvmspirvlib-19-dev \
  libexpat1-dev libffi-dev libsqlite3-dev libncurses-dev libreadline-dev libbz2-dev liblzma-dev uuid-dev \
  libgl-dev libegl-dev libxkbcommon-dev libwayland-dev wayland-protocols libfontconfig-dev libfreetype-dev \
  libdbus-1-dev libglib2.0-dev libpng-dev libdrm-dev libx11-dev libxext-dev libxcb1-dev libxrender-dev \
  python3-mako python3-yaml python3-pexpect python3-pil
# newer meson than Ubuntu's (Mesa 26 needs it), plus Python modules some builds import
pip install -q --break-system-packages meson==1.12.1 mako pyyaml shtab pycotap==1.3.1 packaging pexpect pillow

step "Ubuntu 26.04 source archive (deb-src)"
cat > /etc/apt/sources.list.d/melon-src.sources <<'EOF'
Types: deb-src
URIs: http://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates resolute resolute-updates
Components: main universe
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
apt-get update -q

step "running melon's own binaries on the build host (rule 18 in AGENTS.md)"
ln -sfn $M/sysroot/usr/lib/libc.so /lib/ld-musl-x86_64.so.1
echo $M/sysroot/usr/lib > /etc/ld-musl-x86_64.path

W=$M/work/host-setup; rm -rf $W; mkdir -p $W $M/hosttools/bin
unset CC CXX CFLAGS CXXFLAGS LDFLAGS PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH

step "sources"
$M/scripts/fetch-sources.sh

step "host apk (signs and indexes packages)"
if [ ! -x $M/hosttools/bin/apk ]; then
  cd $W; tar xzf $M/sources/apk-tools-3.0.8.tar.gz; cd apk-tools-3.0.8
  meson setup build --prefix=$M/hosttools -Dlua=disabled -Ddocs=disabled -Dhelp=disabled -Dpython=disabled \
    -Dtests=disabled -Ddefault_library=static >/dev/null
  ninja -C build >/dev/null; ninja -C build install >/dev/null
fi

step "wayland-scanner 1.24 (Ubuntu 24.04 has 1.22)"
if ! /usr/local/bin/wayland-scanner --version 2>&1 | grep -q '1\.24'; then
  cd $W; tar xzf $M/sources/wayland-1.24.0.tar.gz; cd wayland-1.24.0
  meson setup build --prefix=/usr/local -Dlibraries=false -Ddocumentation=false -Dtests=false -Ddtd_validation=false >/dev/null
  ninja -C build >/dev/null; ninja -C build install >/dev/null
fi

step "mesa_clc and vtn_bindgen2 for the Intel drivers (built against the host's LLVM 19)"
if [ ! -x $M/hosttools/bin/mesa_clc ]; then
  cd $W; tar xJf $M/sources/mesa-26.0.8.tar.xz; cd mesa-26.0.8
  meson setup build -Dplatforms= -Dgallium-drivers= -Dvulkan-drivers= -Dglx=disabled -Degl=disabled -Dgbm=disabled \
    -Dllvm=enabled -Dshared-llvm=enabled -Dmesa-clc=enabled -Dinstall-mesa-clc=true -Dprecomp-compiler=enabled \
    -Dinstall-precomp-compiler=true --prefix=$W/mesa-host >/dev/null
  ninja -C build src/compiler/clc/mesa_clc src/compiler/spirv/vtn_bindgen2 >/dev/null
  cp build/src/compiler/clc/mesa_clc build/src/compiler/spirv/vtn_bindgen2 $M/hosttools/bin/
fi

step "GRUB for building ISOs"
[ -x $M/hosttools/grub/bin/grub-mkrescue ] || $M/scripts/hostgrub.sh

step "Python 3.14 (cross-building melon's Python needs the same version on the host)"
[ -x $M/hosttools/python/bin/python3.14 ] || $M/scripts/host-python.sh

step "Qt 6 host tools (resumable; the long one)"
$M/scripts/host-qt.sh

rm -rf $W
step "done. Next: scripts/build-everything.sh"
