#!/usr/bin/env python3
"""Generate MELONBUILD recipes for Qt 6 modules, KDE Frameworks 6, Plasma 6 and the KDE apps.

All of them are CMake projects cross-compiled the same way:
  - Qt's host tools (moc, rcc, qmlcachegen, ...) come from hosttools/qt6 (QT_HOST_PATH);
  - KDE's own build-time tools (kconfig_compiler, ...) are melon binaries, which run on the build host.
Order matters: build-all builds them in ORDER and each package lands in the sysroot for the next.
"""
import json, os
M = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRCIDX = json.load(open(f'{M}/scripts/kde-sources.json'))  # Debian source tarball for each recipe

QT = '-DQT_HOST_PATH=$M/hosttools/qt6 -DQT_HOST_PATH_CMAKE_DIR=$M/hosttools/qt6/lib/cmake'  # $M expands when the recipe runs
QTDIRS = ('-DINSTALL_BINDIR=lib/qt6/bin -DINSTALL_PUBLICBINDIR=usr/bin -DINSTALL_LIBEXECDIR=lib/qt6/libexec '
          '-DINSTALL_ARCHDATADIR=lib/qt6 -DINSTALL_DATADIR=share/qt6 -DINSTALL_INCLUDEDIR=include/qt6 '
          '-DINSTALL_MKSPECSDIR=lib/qt6/mkspecs -DINSTALL_DOCDIR=share/doc/qt6 -DINSTALL_EXAMPLESDIR=share/doc/qt6/examples')
KDE = (QT + ' -DBUILD_QCH=OFF -DBUILD_PYTHON_BINDINGS=OFF -DKDE_INSTALL_QTPLUGINDIR=lib/qt6/plugins '
       '-DKDE_INSTALL_QMLDIR=lib/qt6/qml -DKDE_INSTALL_LIBEXECDIR=libexec -DKF_IGNORE_PLATFORM_CHECK=ON '
       '-DBUILD_WITH_QT6=ON -DQT_MAJOR_VERSION=6 -DBUILD_DOC=OFF')   # handbooks need KDocTools, not in melon yet

QTBASE = (QT + ' ' + QTDIRS + ' -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF '
  '-DFEATURE_opengl=ON -DINPUT_opengl=desktop -DFEATURE_egl=ON -DFEATURE_eglfs=OFF -DFEATURE_xcb=ON '
  '-DFEATURE_wayland=ON -DFEATURE_glib=ON -DFEATURE_dbus_linked=ON -DFEATURE_system_pcre2=ON -DFEATURE_system_zlib=ON '
  '-DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON '
  '-DFEATURE_fontconfig=ON -DFEATURE_openssl_linked=ON -DFEATURE_sql_sqlite=ON -DFEATURE_system_sqlite=ON '
  '-DFEATURE_icu=OFF -DFEATURE_journald=OFF -DFEATURE_zstd=ON -DFEATURE_xkbcommon=ON -DFEATURE_cups=ON '
  '-DFEATURE_gtk3=OFF -DFEATURE_vulkan=ON -DFEATURE_libinput=OFF -DFEATURE_tslib=OFF -DFEATURE_mtdev=OFF '
  '-DFEATURE_linuxfb=OFF -DFEATURE_vnc=OFF -DFEATURE_sql_psql=OFF -DFEATURE_sql_mysql=OFF -DFEATURE_sql_odbc=OFF '
  '-DFEATURE_reduce_relocations=OFF')

# (recipe name, debian source name, extra cmake args, kind)   kind: qt | kde | noarch-kde
ORDER = [
 ('qt6-qtbase','qt6-base', QTBASE, 'qt'),
 ('qt6-qtshadertools','qt6-shadertools', QT+' '+QTDIRS, 'qt'),
 ('qt6-qtsvg','qt6-svg', QT+' '+QTDIRS, 'qt'),
 ('qt6-qtimageformats','qt6-imageformats', QT+' '+QTDIRS, 'qt'),
 ('qt6-qtdeclarative','qt6-declarative', QT+' '+QTDIRS+' -DFEATURE_qml_debug=OFF', 'qt'),
 ('qt6-qtwayland','qt6-wayland', QT+' '+QTDIRS, 'qt'),
 ('qt6-qtpositioning','qt6-positioning', QT+' '+QTDIRS, 'qt'),
 ('qt6-qtlocation','qt6-location', QT+' '+QTDIRS, 'qt'),   # plasma-workspace needs QtLocation
 ('qt6-qtwebview','qt6-webview', QT+' '+QTDIRS, 'qt'),   # Discover; no QtWebEngine backend (a whole Chromium)
 ('qt6-qtspeech','qt6-speech', QT+' '+QTDIRS, 'qt'),   # KTextEditor needs the module; no speech engine yet
 ('qt6-qt5compat','qt6-5compat', QT+' '+QTDIRS, 'qt'),
 ('qt6-qttools','qt6-tools', QT+' '+QTDIRS+' -DFEATURE_assistant=OFF -DFEATURE_designer=OFF -DFEATURE_distancefieldgenerator=OFF '
   '-DFEATURE_pixeltool=OFF -DFEATURE_qtdiag=OFF -DFEATURE_clang=OFF -DFEATURE_qdoc=OFF -DFEATURE_linguist=ON', 'qt'),
 ('qt6-qtmultimedia','qt6-multimedia', QT+' '+QTDIRS+' -DFEATURE_ffmpeg=OFF -DFEATURE_gstreamer=OFF -DFEATURE_pulseaudio=OFF', 'qt'),
 ('extra-cmake-modules','kf6-extra-cmake-modules', '-DBUILD_DOC=OFF', 'noarch-kde'),
 ('plasma-wayland-protocols','plasma-wayland-protocols', '', 'noarch-kde'),
 ('polkit-qt-1','polkit-qt-1', KDE, 'kde'),
 ('qcoro','qcoro', KDE+' -DBUILD_SHARED_LIBS=ON -DQCORO_BUILD_EXAMPLES=OFF -DQCORO_WITH_QTWEBSOCKETS=OFF -DQCORO_WITH_QML=ON', 'kde'),   # shared: plasma-nm links it into a shared library
 ('pulseaudio-qt','pulseaudio-qt', KDE, 'kde'),   # plasma-pa talks to PipeWire through the PulseAudio API
 # QCA (crypto for KWallet's secret service); only the OpenSSL and GnuPG plugins
 ('qca','qca2', KDE+' -DQT6=ON -DBUILD_TESTS=OFF -DBUILD_TOOLS=OFF -DWITH_botan_PLUGIN=no -DWITH_pkcs11_PLUGIN=no -DWITH_cyrus-sasl_PLUGIN=no', 'kde'),
]
KF6 = ('kcoreaddons kconfig ki18n kwidgetsaddons kwindowsystem kguiaddons kcodecs kitemmodels kitemviews karchive '
       'kdbusaddons kcrash kauth kcolorscheme kcompletion kconfigwidgets kglobalaccel kiconthemes breeze-icons '
       'kservice knotifications kjobwidgets solid sonnet ktextwidgets kxmlgui kbookmarks kpackage kidletime '
       'kstatusnotifieritem kwallet attica kirigami ksvg kdeclarative kded kio kcmutils knewstuff knotifyconfig '
       'kparts kpty kunitconversion krunner kquickcharts qqc2-desktop-style frameworkintegration kdesu '
       'kfilemetadata threadweaver networkmanager-qt modemmanager-qt bluez-qt kimageformats kholidays prison '
       'syntax-highlighting ktexteditor purpose').split()
for k in KF6:
    extra = KDE
    if k == 'breeze-icons': extra += ' -DWITH_ICON_GENERATION=OFF -DBINARY_ICONS_RESOURCE=OFF'
    if k == 'kwallet': extra += ' -DBUILD_KSECRETD=ON -DBUILD_KWALLETD=OFF -DBUILD_KWALLET_QUERY=OFF'   # ksecretd is the store in 6.x; the legacy bridge needs libsecret
    if k == 'kfilemetadata': extra += ' -DKFILEMETADATA_USE_TAGLIB=OFF'
    if k == 'kpty': extra += ' -DCMAKE_DISABLE_FIND_PACKAGE_UTEMPTER=ON'   # no utmp logging on musl
    if k == 'ktextwidgets': extra += ' -DWITH_TEXT_TO_SPEECH=OFF'         # no qtspeech in melon yet
    if k == 'syntax-highlighting': extra += ' -DKATEHIGHLIGHTINGINDEXER_EXECUTABLE=$PWD/host-indexer/bin/katehighlightingindexer'
    if k == 'prison': extra += ' -DWITH_DMTX=OFF'   # no Data Matrix yet; ZXing reads codes (Spectacle's QR scanning)
    ORDER.append((f'kf6-{k}', f'kf6-{k}', extra, 'kde'))
PLASMA = [
 ('kdecoration',''), ('kwayland',''), ('layer-shell-qt',''), ('plasma-activities',''), ('plasma-activities-stats',''),
 ('kactivitymanagerd',''), ('kglobalacceld',''), ('libplasma',''), ('plasma5support',''), ('libkscreen',''),
 ('kirigami-addons',''), ('kpipewire',''), ('libksysguard',' -DBUILD_WITH_QTWEBENGINE=OFF'), ('ksystemstats',''),
 ('kscreenlocker',''), ('breeze',' -DBUILD_QT5=OFF'), ('knighttime',''),   # KWin's night light
 ('kquickimageeditor',''),   # Spectacle's annotation editor
 ('kwin',' -DKWIN_BUILD_ACTIVITIES=ON -DKWIN_BUILD_X11=OFF -DQTWAYLANDSCANNER_KDE_EXECUTABLE=$PWD/host-scanner/qtwaylandscanner_kde'),
 ('plasma-workspace',' -DPLASMA_WAYLAND_DEFAULT_SESSION=ON'), ('plasma-integration',' -DBUILD_QT5=OFF'),
 ('plasma-desktop',''), ('systemsettings',''), ('kscreen',''), ('powerdevil',''), ('plasma-nm',' -DDISABLE_MODEMMANAGER_SUPPORT=ON'),
 ('plasma-pa',''), ('bluedevil',''), ('polkit-kde-agent-1',''), ('xdg-desktop-portal-kde',''), ('milou',''),
 ('kde-cli-tools',''), ('plasma-systemmonitor',''), ('kinfocenter',''), ('sddm-kcm',''),
 ('sddm',' -DENABLE_PAM=ON -DNO_SYSTEMD=ON -DUSE_ELOGIND=ON -DBUILD_MAN_PAGES=OFF -DRUNTIME_DIR=/run/sddm -DUID_MIN=1000 -DDBUS_CONFIG_DIR=/usr/share/dbus-1/system.d'),
 ('dolphin',''), ('konsole',''), ('kde-spectacle',''), ('plasma-discover',' -DBUILD_PackageKitBackend=OFF -DBUILD_SnapBackend=OFF -DBUILD_FwupdBackend=OFF'),
]
for n, extra in PLASMA:
    ORDER.append((n, n, KDE + extra, 'kde'))

# pkgrel for recipes whose build changed without a version change (rule 6/31 in AGENTS.md).
# 1: Qt gained its xcb platform (Plasma 6 needs KDE's X11 integration headers even in a Wayland session)
PKGREL = {n: 1 for n in ('qt6-qtbase kf6-kwindowsystem kf6-kguiaddons kf6-kdbusaddons kf6-kcrash kf6-kglobalaccel '
                         'kf6-kjobwidgets kf6-kidletime kf6-kstatusnotifieritem kf6-kio kf6-qqc2-desktop-style kf6-kdesu '
                         'kglobalacceld').split()}
PKGREL['kf6-prison'] = 1   # with ZXing: barcode reading and PDF417
PKGREL['kf6-kitemmodels'] = PKGREL['kf6-bluez-qt'] = 1   # rebuilt with their QML modules (first built before Qt QML existed)
PKGREL['qcoro'] = 1   # shared libraries instead of static ones
PKGREL['qca'] = 2   # relocatable CMake export (POST below)
PKGREL['qt6-qtbase'] = 3   # 2: CUPS print support (xdg-desktop-portal-kde); 3: Vulkan (kinfocenter; Mesa has RADV/ANV)

# extra build() steps, run before configuring
HOSTENV = ('unset CC CXX AR AS LD NM RANLIB STRIP OBJCOPY CFLAGS CXXFLAGS CPPFLAGS LDFLAGS PKG_CONFIG_LIBDIR '
           'PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH CONFIG_SITE; ')
PRE = {
    # KSyntaxHighlighting builds its syntax index with katehighlightingindexer: build that for the build host first
    'kf6-syntax-highlighting': '( ' + HOSTENV + 'cmake -G Ninja -S . -B host-indexer -DKSYNTAXHIGHLIGHTING_USE_GUI=OFF '
            '-DBUILD_TESTING=OFF -DQT_MAJOR_VERSION=6 -DCMAKE_PREFIX_PATH=$M/hosttools/qt6 -DECM_DIR=$SYSROOT/usr/share/ECM/cmake '
            '>/dev/null && ninja -C host-indexer katehighlightingindexer >/dev/null ); ',
    # KWin generates Wayland code with its own qtwaylandscanner_kde: build that for the build host first,
    # against the host Qt, with the cross toolchain and sysroot out of sight (rule 2)
    'kwin': '( unset CC CXX AR AS LD NM RANLIB STRIP OBJCOPY CFLAGS CXXFLAGS CPPFLAGS LDFLAGS PKG_CONFIG_LIBDIR '
            'PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH CONFIG_SITE; cmake -G Ninja -S src/wayland/tools -B host-scanner '
            '-DCMAKE_PREFIX_PATH=$M/hosttools/qt6 -DECM_DIR=$SYSROOT/usr/share/ECM/cmake >/dev/null && '
            'ninja -C host-scanner >/dev/null ); ',
}

# extra package() steps
POST = {
    # QCA exports absolute /usr paths in its CMake targets; make them relative so they resolve inside the sysroot too
    'qca': " sed -i 's|\"/usr/|\"${_IMPORT_PREFIX}/|g' $pkgdir/usr/lib/cmake/Qca-qt6/Qca-qt6Targets*.cmake;"
           " sed -i 's|^set(_IMPORT_PREFIX \"/usr\")$|get_filename_component(_IMPORT_PREFIX \"${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)|'"
           " $pkgdir/usr/lib/cmake/Qca-qt6/Qca-qt6Targets.cmake;",
}

def write(name, deb, args, kind):
    s = SRCIDX[deb]
    ext = s['file'].rsplit('.orig.', 1)[1]
    link = f'{name}-{s["ver"]}.{ext}'
    lp = f'{M}/sources/{link}'
    if not os.path.lexists(lp): os.symlink(f'deb/{s["file"]}', lp)
    top = s['top']
    bdir = '$srcdir' if top == '.' else f'$srcdir/{top}'
    lines = [f'pkgname={name}', f'pkgver={s["ver"].split("+")[0]}', f'pkgrel={PKGREL.get(name, 0)}', f'pkgdesc="{name}"',
             'license="LGPL-2.1-or-later OR GPL-2.0-or-later (see sources)"', f'source=({link})', f'builddir={bdir}']
    if kind != 'noarch-kde': lines.append(f'subpackages=({name}-dev)')
    lines.append(f'build(){{ {PRE.get(name, "")}cmake_setup -B build {args}; ninja -C build -j$JOBS; }}')
    lines.append('package(){ DESTDIR=$pkgdir ninja -C build install; rm -rf $pkgdir/usr/share/doc $pkgdir/usr/share/man;' + POST.get(name, '') + ' }')
    if kind == 'qt':
        lines.append('pkg_dev(){ default_dev; amove usr/lib/qt6/mkspecs usr/lib/qt6/metatypes usr/lib/qt6/modules usr/lib/qt6/sbom; }')
    d = f'{M}/recipes/{name}'; os.makedirs(d, exist_ok=True)
    open(f'{d}/MELONBUILD', 'w').write('\n'.join(lines) + '\n')

for row in ORDER: write(*row)
print(' '.join(r[0] for r in ORDER))
