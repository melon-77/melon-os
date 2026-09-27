#!/bin/bash
# host-qt.sh: build the Qt 6 host tools (moc, rcc, uic, qsb, qmlcachegen, qtwaylandscanner, lrelease, ...)
# for the build machine. Cross-compiling Qt and KDE for melon requires host tools of the exact same
# Qt version (QT_HOST_PATH). Installed to hosttools/qt6; nothing from here ships in melon.
set -euo pipefail
M=/home/claude/melon
QV=6.10.2
H=$M/hosttools/qt6
W=$M/work/host-qt
unset CC CXX CFLAGS CXXFLAGS LDFLAGS PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH
rm -rf $W; mkdir -p $W $H; cd $W
common="-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$H -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DBUILD_TESTING=OFF"
mod(){ # mod <debian-name> <dir-prefix> [cmake args...]
  local deb=$1 dir=$2; shift 2
  tar xf $M/sources/deb/${deb}_${QV}*.orig.tar.xz
  local src=$(ls -d ${dir}*${QV}* | head -1)
  echo "=== $(date +%T) $src"
  cmake -S $src -B b-$deb $common -DCMAKE_PREFIX_PATH=$H "$@" >/dev/null
  ninja -C b-$deb -j$(nproc) >/dev/null
  ninja -C b-$deb install >/dev/null
  rm -rf $src b-$deb
}
mod qt6-base qtbase -DFEATURE_opengl=ON -DINPUT_opengl=desktop -DFEATURE_xcb=OFF -DFEATURE_glib=OFF -DFEATURE_icu=OFF \
    -DFEATURE_sql=OFF -DFEATURE_printsupport=OFF -DFEATURE_network=ON -DFEATURE_widgets=ON -DFEATURE_dbus=ON \
    -DFEATURE_system_pcre2=OFF -DFEATURE_system_zlib=ON -DFEATURE_system_harfbuzz=OFF -DFEATURE_system_freetype=OFF
mod qt6-shadertools qtshadertools
mod qt6-declarative qtdeclarative -DFEATURE_qml_debug=OFF
mod qt6-wayland qtwayland
mod qt6-tools qttools -DFEATURE_assistant=OFF -DFEATURE_designer=OFF -DFEATURE_distancefieldgenerator=OFF \
    -DFEATURE_pixeltool=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF -DFEATURE_clang=OFF -DFEATURE_qdoc=OFF
echo "=== $(date +%T) done"
ls $H/bin | head -40
