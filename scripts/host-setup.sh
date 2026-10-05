#!/bin/bash
# host-setup.sh: prepare a fresh Ubuntu 24.04 machine (or WSL2 Ubuntu 24.04), or a melon system, to build melon.
# Run as root (sudo scripts/host-setup.sh). Safe to run again.
#
#  1. build dependencies (from Ubuntu, or melon's own packages on melon)
#  2. Ubuntu only: the Ubuntu 26.04 ("resolute") source archive, where older melon sources come from
#  3. let the build host run melon's musl binaries (build-time generators from earlier packages)
#  4. host tools the cross builds need: apk, wayland-scanner 1.24, Mesa's mesa_clc, GRUB, Python, Rust, Qt
set -euo pipefail
M=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
[ "$(id -u)" = 0 ] || { echo "run as root: sudo $0" >&2; exit 1; }
. /etc/os-release
[ "$ID" = melon ] || [ "$VERSION_ID" = 24.04 ] || echo "warning: tested on Ubuntu 24.04 and melon, this is $PRETTY_NAME"
# build output goes to logs/host-setup.log; a failing step prints its end instead of stopping without a word
mkdir -p "$M/logs"; LOG=$M/logs/host-setup.log; : > "$LOG"
set -E; trap 'rc=$?; echo "host-setup.sh failed (line $LINENO, exit $rc). End of $LOG:" >&2; tail -n 40 "$LOG" >&2; exit $rc' ERR
step(){ printf '\033[1;35m== %s\033[0m\n' "$*"; echo "== $*" >> "$LOG"; }

ubuntu_deps(){
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
    python3-mako python3-yaml python3-pexpect python3-pil libxml2-utils appstream libappstream-dev itstool nasm bubblewrap autoconf-archive ntfs-3g
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
}

# melon as the build machine (BUILDING.md): the same tools from melon's own package repository. Everything here was
# built for melon's self-hosting work (AGENTS.md, Roadmap); names that are missing are listed at the end.
melon_deps(){
  step "build dependencies (melon packages)"
  local pkgs="gcc g++ binutils make pkgconf patch perl bison flex texinfo bc gawk gperf m4 python3 python3-mako
    python3-yaml python3-packaging python3-pexpect ninja cmake meson autoconf automake autoconf-archive libtool gettext
    file xz zstd lz4 bzip2 rsync curl git ca-certificates xorriso mtools dosfstools xfsprogs squashfs-tools qemu ovmf
    kmod dwarves scdoc tcl hwdata publicsuffix dtc glslang spirv-tools spirv-tools-dev libxslt libxml2 appstream itstool nasm bubblewrap
    ntfs-3g linux-headers bsd-compat-headers llvm llvm-dev clang clang-dev libclc spirv-llvm-translator spirv-llvm-translator-dev
    spirv-headers openssl-dev zlib-dev zstd-dev elfutils-dev expat-dev libffi-dev sqlite-dev ncurses-dev readline-dev
    bzip2-dev xz-dev util-linux-dev libxml2-dev appstream-dev mesa-dev libxkbcommon-dev wayland wayland-dev
    wayland-protocols fontconfig-dev freetype-dev dbus-dev glib-dev libpng-dev libdrm-dev libx11-dev libxext-dev
    libxcb-dev libxrender-dev"
  # shellcheck disable=SC2086
  if ! apk add -q $pkgs; then   # one at a time, so one missing name doesn't stop the rest
    local p missing=
    for p in $pkgs; do apk add -q "$p" >/dev/null 2>&1 || missing="$missing $p"; done
    [ -z "$missing" ] || echo "warning: not in melon's repository (builds that need them will fail):$missing"
  fi
  # Python modules melon doesn't package (ppd's shell completion, Mesa's test runner). melon's recipes turn both off
  # (ppd -Dbashcomp=disabled -Dzshcomp=, Mesa -Dbuild-tests=false), and melon's python3 has no pip: optional.
  if python3 -m pip --version >/dev/null 2>&1; then
    python3 -m pip install -q --break-system-packages shtab pycotap==1.3.1 || echo "note: pip could not install shtab and pycotap (optional)"
  fi

  # Rule 18 on melon: the system's own musl is the loader, so never point /lib/ld-musl-x86_64.so.1 at the sysroot
  # (that would swap libc under every running program). Programs from the sysroot run as they are; only libraries that
  # exist nowhere but in the sysroot need a path, and melon's own directories come first so the system's programs
  # never load a library from the sysroot.
  step "running melon binaries from the sysroot (rule 18 in AGENTS.md, melon variant)"
  printf '%s\n' /usr/local/lib /usr/lib "$M/sysroot/usr/lib" > /etc/ld-musl-x86_64.path

  # recipes unpack Ubuntu .debs (firmware, fonts, certificates, OVMF) with `dpkg-deb -x`; melon has no dpkg
  if ! command -v dpkg-deb >/dev/null; then
    cat > /usr/local/bin/dpkg-deb <<'SHIM'
#!/bin/sh
# dpkg-deb -x <deb> <dir>, the only form melon's recipes use (melon as the build machine; scripts/host-setup.sh)
[ "$1" = -x ] && [ $# = 3 ] || { echo "dpkg-deb (melon shim): only 'dpkg-deb -x <deb> <dir>' is supported" >&2; exit 2; }
m=$(ar t "$2" | grep '^data\.tar') || { echo "dpkg-deb: no data.tar in $2" >&2; exit 1; }
case $m in *.xz) d="xz -dc" ;; *.zst) d="zstd -dc" ;; *.gz) d="gzip -dc" ;; *.bz2) d="bzip2 -dc" ;; *) d=cat ;; esac
mkdir -p "$3" && ar p "$2" "$m" | $d | tar -x -C "$3" -f -
SHIM
    chmod 755 /usr/local/bin/dpkg-deb
  fi
}

if [ "$ID" = melon ]; then melon_deps; else ubuntu_deps
  step "running melon's own binaries on the build host (rule 18 in AGENTS.md)"
  ln -sfn $M/sysroot/usr/lib/libc.so /lib/ld-musl-x86_64.so.1
  echo $M/sysroot/usr/lib > /etc/ld-musl-x86_64.path
fi

W=$M/work/host-setup; rm -rf $W; mkdir -p $W $M/hosttools/bin
unset CC CXX CFLAGS CXXFLAGS LDFLAGS PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH

step "sources"
$M/scripts/fetch-sources.sh

step "host apk (signs and indexes packages)"
if [ "$ID" = melon ] && apk --version 2>/dev/null | grep -q 'apk-tools 3\.'; then
  ln -sfn "$(command -v apk)" $M/hosttools/bin/apk   # melon's own apk is the same apk-tools 3
elif [ ! -x $M/hosttools/bin/apk ]; then
  cd $W; tar xzf $M/sources/apk-tools-3.0.8.tar.gz; cd apk-tools-3.0.8
  meson setup build --prefix=$M/hosttools -Dlua=disabled -Ddocs=disabled -Dhelp=disabled -Dpython=disabled \
    -Dtests=disabled -Ddefault_library=static >>$LOG 2>&1
  ninja -C build >>$LOG 2>&1; ninja -C build install >>$LOG 2>&1
fi

step "wayland-scanner 1.24 (Ubuntu 24.04 has 1.22)"
if ! /usr/local/bin/wayland-scanner --version 2>&1 | grep -q '1\.24' &&
   ! { [ "$ID" = melon ] && wayland-scanner --version 2>&1 | grep -q '1\.2[4-9]' &&   # melon ships 1.24: the cross file's
       ln -sfn "$(command -v wayland-scanner)" /usr/local/bin/wayland-scanner; }; then  # path (rule 20) points at it
  cd $W; tar xzf $M/sources/wayland-1.24.0.tar.gz; cd wayland-1.24.0
  meson setup build --prefix=/usr/local -Dlibraries=false -Ddocumentation=false -Dtests=false -Ddtd_validation=false >>$LOG 2>&1
  ninja -C build >>$LOG 2>&1; ninja -C build install >>$LOG 2>&1
fi

step "mesa_clc and vtn_bindgen2 for the Intel drivers (built against the host's LLVM: Ubuntu's 19, melon's 21)"
if [ ! -x $M/hosttools/bin/mesa_clc ]; then
  cd $W; tar xJf $M/sources/mesa-26.0.8.tar.xz; cd mesa-26.0.8
  meson setup build -Dplatforms= -Dgallium-drivers= -Dvulkan-drivers= -Dglx=disabled -Degl=disabled -Dgbm=disabled \
    -Dllvm=enabled -Dshared-llvm=enabled -Dmesa-clc=enabled -Dinstall-mesa-clc=true -Dprecomp-compiler=enabled \
    -Dinstall-precomp-compiler=true --prefix=$W/mesa-host >>$LOG 2>&1
  ninja -C build src/compiler/clc/mesa_clc src/compiler/spirv/vtn_bindgen2 >>$LOG 2>&1
  cp build/src/compiler/clc/mesa_clc build/src/compiler/spirv/vtn_bindgen2 $M/hosttools/bin/
fi

step "GRUB for building ISOs"
[ -x $M/hosttools/grub/bin/grub-mkrescue ] || $M/scripts/hostgrub.sh

step "Python 3.14 (cross-building melon's Python needs the same version on the host)"
[ -x $M/hosttools/python/bin/python3.14 ] || $M/scripts/host-python.sh

step "Rust 1.98.1 (recipes cross-compile Rust code with it; recipes/rust builds melon's own from source)"
$M/scripts/host-rust.sh

step "Qt 6 host tools (resumable; the long one)"
$M/scripts/host-qt.sh

rm -rf $W
step "done. Next: scripts/build-everything.sh"
