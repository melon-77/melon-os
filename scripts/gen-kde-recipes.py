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
       '-DBUILD_WITH_QT6=ON -DQT_MAJOR_VERSION=6 -DWITH_X11=OFF')   # melon's Qt is Wayland-only (no xcb)

QTBASE = (QT + ' ' + QTDIRS + ' -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF '
  '-DFEATURE_opengl=ON -DINPUT_opengl=desktop -DFEATURE_egl=ON -DFEATURE_eglfs=OFF -DFEATURE_xcb=OFF '
  '-DFEATURE_wayland=ON -DFEATURE_glib=ON -DFEATURE_dbus_linked=ON -DFEATURE_system_pcre2=ON -DFEATURE_system_zlib=ON '
  '-DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON '
  '-DFEATURE_fontconfig=ON -DFEATURE_openssl_linked=ON -DFEATURE_sql_sqlite=ON -DFEATURE_system_sqlite=ON '
  '-DFEATURE_icu=OFF -DFEATURE_journald=OFF -DFEATURE_zstd=ON -DFEATURE_xkbcommon=ON -DFEATURE_cups=OFF '
  '-DFEATURE_gtk3=OFF -DFEATURE_vulkan=OFF -DFEATURE_libinput=OFF -DFEATURE_tslib=OFF -DFEATURE_mtdev=OFF '
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
 ('qt6-qt5compat','qt6-5compat', QT+' '+QTDIRS, 'qt'),
 ('qt6-qttools','qt6-tools', QT+' '+QTDIRS+' -DFEATURE_assistant=OFF -DFEATURE_designer=OFF -DFEATURE_distancefieldgenerator=OFF '
   '-DFEATURE_pixeltool=OFF -DFEATURE_qtdiag=OFF -DFEATURE_clang=OFF -DFEATURE_qdoc=OFF -DFEATURE_linguist=ON', 'qt'),
 ('qt6-qtmultimedia','qt6-multimedia', QT+' '+QTDIRS+' -DFEATURE_ffmpeg=OFF -DFEATURE_gstreamer=OFF -DFEATURE_pulseaudio=OFF', 'qt'),
 ('extra-cmake-modules','kf6-extra-cmake-modules', '-DBUILD_DOC=OFF', 'noarch-kde'),
 ('plasma-wayland-protocols','plasma-wayland-protocols', '', 'noarch-kde'),
 ('polkit-qt-1','polkit-qt-1', KDE, 'kde'),
 ('qcoro','qcoro', KDE+' -DQCORO_BUILD_EXAMPLES=OFF -DQCORO_WITH_QTWEBSOCKETS=OFF -DQCORO_WITH_QML=ON', 'kde'),
]
KF6 = ('kcoreaddons kconfig ki18n kwidgetsaddons kwindowsystem kguiaddons kcodecs kitemmodels kitemviews karchive '
       'kdbusaddons kcrash kauth kcolorscheme kcompletion kconfigwidgets kglobalaccel kiconthemes breeze-icons '
       'kservice knotifications kjobwidgets solid sonnet ktextwidgets kxmlgui kbookmarks kpackage kidletime '
       'kstatusnotifieritem kwallet attica kirigami ksvg kdeclarative kded kio kcmutils knewstuff knotifyconfig '
       'kparts kpty kunitconversion krunner kquickcharts qqc2-desktop-style frameworkintegration kdesu '
       'kfilemetadata threadweaver networkmanager-qt bluez-qt kimageformats').split()
for k in KF6:
    extra = KDE
    if k == 'breeze-icons': extra += ' -DWITH_ICON_GENERATION=OFF -DBINARY_ICONS_RESOURCE=OFF'
    if k == 'kwallet': extra += ' -DBUILD_KWALLETD=ON -DBUILD_KWALLET_QUERY=OFF'
    if k == 'kfilemetadata': extra += ' -DKFILEMETADATA_USE_TAGLIB=OFF'
    if k == 'kpty': extra += ' -DCMAKE_DISABLE_FIND_PACKAGE_UTEMPTER=ON'   # no utmp logging on musl
    if k == 'kwindowsystem': extra += ' -DKWINDOWSYSTEM_X11=OFF'           # its own switch, not WITH_X11
    ORDER.append((f'kf6-{k}', f'kf6-{k}', extra, 'kde'))
PLASMA = [
 ('kdecoration',''), ('kwayland',''), ('layer-shell-qt',''), ('plasma-activities',''), ('plasma-activities-stats',''),
 ('kactivitymanagerd',''), ('kglobalacceld',''), ('libplasma',''), ('plasma5support',''), ('libkscreen',''),
 ('kirigami-addons',''), ('kpipewire',''), ('libksysguard',' -DBUILD_WITH_QTWEBENGINE=OFF'), ('ksystemstats',''),
 ('kscreenlocker',''), ('breeze',' -DBUILD_QT5=OFF'), ('kwin',' -DKWIN_BUILD_ACTIVITIES=ON -DKWIN_BUILD_X11=OFF'),
 ('plasma-workspace',' -DPLASMA_WAYLAND_DEFAULT_SESSION=ON'), ('plasma-integration',' -DBUILD_QT5=OFF'),
 ('plasma-desktop',''), ('systemsettings',''), ('kscreen',''), ('powerdevil',''), ('plasma-nm',' -DDISABLE_MODEMMANAGER_SUPPORT=ON'),
 ('plasma-pa',''), ('bluedevil',''), ('polkit-kde-agent-1',''), ('xdg-desktop-portal-kde',''), ('milou',''),
 ('kde-cli-tools',''), ('plasma-systemmonitor',''), ('kinfocenter',''), ('sddm-kcm',''),
 ('sddm',' -DENABLE_PAM=ON -DNO_SYSTEMD=ON -DUSE_ELOGIND=ON -DBUILD_MAN_PAGES=OFF -DRUNTIME_DIR=/run/sddm -DUID_MIN=1000 -DDBUS_CONFIG_DIR=/usr/share/dbus-1/system.d'),
 ('dolphin',''), ('konsole',''), ('kde-spectacle',''), ('plasma-discover',' -DBUILD_PackageKitBackend=OFF -DBUILD_SnapBackend=OFF -DBUILD_FwupdBackend=OFF'),
]
for n, extra in PLASMA:
    ORDER.append((n, n, KDE + extra, 'kde'))

def write(name, deb, args, kind):
    s = SRCIDX[deb]
    ext = s['file'].rsplit('.orig.', 1)[1]
    link = f'{name}-{s["ver"]}.{ext}'
    lp = f'{M}/sources/{link}'
    if not os.path.lexists(lp): os.symlink(f'deb/{s["file"]}', lp)
    top = s['top']
    bdir = '$srcdir' if top == '.' else f'$srcdir/{top}'
    lines = [f'pkgname={name}', f'pkgver={s["ver"].split("+")[0]}', f'pkgdesc="{name}"',
             'license="LGPL-2.1-or-later OR GPL-2.0-or-later (see sources)"', f'source=({link})', f'builddir={bdir}']
    if kind != 'noarch-kde': lines.append(f'subpackages=({name}-dev)')
    lines.append(f'build(){{ cmake_setup -B build {args}; ninja -C build -j$JOBS; }}')
    lines.append('package(){ DESTDIR=$pkgdir ninja -C build install; rm -rf $pkgdir/usr/share/doc $pkgdir/usr/share/man; }')
    if kind == 'qt':
        lines.append('pkg_dev(){ default_dev; amove usr/lib/qt6/mkspecs usr/lib/qt6/metatypes usr/lib/qt6/modules usr/lib/qt6/sbom; }')
    d = f'{M}/recipes/{name}'; os.makedirs(d, exist_ok=True)
    open(f'{d}/MELONBUILD', 'w').write('\n'.join(lines) + '\n')

for row in ORDER: write(*row)
print(' '.join(r[0] for r in ORDER))
