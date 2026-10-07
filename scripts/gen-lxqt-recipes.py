#!/usr/bin/env python3
"""Generate MELONBUILD recipes for LXQt 2.4, the desktop of melon's 32-bit (i686) desktop edition.

All of them are CMake projects from LXQt's GitHub releases, cross-compiled like the KDE recipes
(gen-kde-recipes.py): Qt's host tools (moc, lrelease, ...) come from hosttools/qt6 (QT_HOST_PATH).
Every tarball was checked against Alpine's sha512sums (community/<name>/APKBUILD); LXQt also signs them (.asc).
Order matters: build-all builds them in ORDER and each package lands in the sysroot for the next.
Prints the recipe names in build order.
"""
import os
M = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

QT = '-DQT_HOST_PATH=$M/hosttools/qt6 -DQT_HOST_PATH_CMAKE_DIR=$M/hosttools/qt6/lib/cmake -DUPDATE_TRANSLATIONS=OFF'

# (name, version, extra cmake args, has a -dev subpackage, pkgdesc)
ORDER = [
 # it looks Qt6CoreTools up directly, which in a cross build lives in the build machine's Qt; /etc/xdg is what it would
 # ask that Qt's qtpaths for
 ('lxqt-build-tools', '2.4.0', ' -DQt6CoreTools_DIR=$M/hosttools/qt6/lib/cmake/Qt6CoreTools -DLXQT_ETC_XDG_DIR=/etc/xdg', False,
  'CMake modules and tools LXQt builds with'),
 ('libqtxdg', '4.4.0', ' -DBUILD_TESTS=OFF -DBUILD_DEV_UTILS=OFF', True, 'Qt implementation of the freedesktop.org XDG specifications'),
 ('qtxdg-tools', '4.4.0', '', True, 'qtxdg-mat, the command line tool for default applications'),
 ('lxqt-menu-data', '2.4.0', '', False, 'freedesktop.org application menu files for LXQt'),
 ('liblxqt', '2.4.0', '', True, 'Common library of the LXQt desktop'),
 ('libdbusmenu-lxqt', '0.4.0', ' -DWITH_DOC=OFF', True, 'Exports Qt menus over the DBusMenu protocol (tray menus)'),
 ('lxqt-globalkeys', '2.4.0', '', True, 'Global keyboard shortcuts for LXQt'),
 ('libsysstat', '1.1.0', '', True, 'System statistics for the LXQt panel'),
 ('libfm-qt', '2.4.0', '', True, 'File management library for LXQt (PCManFM-Qt, file dialogs)'),
 # the network monitor needs libstatgrab, which melon doesn't have
 ('lxqt-panel', '2.4.0', ' -DNETWORKMONITOR_PLUGIN=OFF', True, 'The LXQt panel'),
 ('pcmanfm-qt', '2.4.0', '', False, 'PCManFM-Qt, the LXQt file manager and desktop'),
 ('lxqt-qtplugin', '2.4.0', '', False, 'Qt platform theme that makes Qt programs follow LXQt settings'),
 ('lxqt-themes', '2.4.0', '', False, 'Themes, graphics and icons of LXQt'),
 ('lxqt-session', '2.4.0', ' -DWITH_LIBUDEV=ON', False, 'The LXQt session manager'),
 # the calculator needs muparser and the VirtualBox runner is for VirtualBox hosts: neither in melon
 ('lxqt-runner', '2.4.0', ' -DRUNNER_MATH=OFF -DRUNNER_VBOX=OFF', False, 'LXQt application launcher (Alt+F2)'),
 ('lxqt-notificationd', '2.4.0', '', False, 'LXQt notification daemon'),
 ('lxqt-policykit', '2.4.0', '', False, 'LXQt PolicyKit agent (asks for passwords)'),
 ('lxqt-powermanagement', '2.4.0', '', False, 'LXQt power management (battery, lid, idle)'),
 # touchpad settings are for Xorg's libinput driver; under labwc lxqt-wayland-session sets the touchpad
 ('lxqt-config', '2.4.0', ' -DWITH_TOUCHPAD=OFF -DCMAKE_CXX_STANDARD=20', False, 'LXQt settings (appearance, input, locale, ...)'),
 ('lxqt-wayland-session', '0.4.0', '', False, 'LXQt Wayland session files (labwc, kwin_wayland, ... as compositor)'),
 ('qtermwidget', '2.4.0', '', True, 'Terminal widget for Qt (QTerminal)'),
 ('qterminal', '2.4.0', '', False, 'QTerminal, the LXQt terminal'),
 ('pavucontrol-qt', '2.4.0', '', False, 'Volume control for PulseAudio (PipeWire)'),
 ('lximage-qt', '2.4.0', '', False, 'LXImage-Qt, the LXQt image viewer and screenshot tool'),
]

def write(name, ver, extra, dev, desc):
    lines = [f'pkgname={name}', f'pkgver={ver}', 'pkgrel=0', f'pkgdesc="{desc}"',
             'license="LGPL-2.1-or-later (see sources)"', 'url=https://lxqt-project.org',
             f'source=({name}-{ver}.tar.xz)']
    if dev: lines.append(f'subpackages=({name}-dev)')
    lines.append(f'build(){{ cmake_setup -B build {QT}{extra}; ninja -C build -j$JOBS; }}')
    lines.append('package(){ DESTDIR=$pkgdir ninja -C build install; rm -rf $pkgdir/usr/share/doc $pkgdir/usr/share/man; }')
    d = f'{M}/recipes/{name}'; os.makedirs(d, exist_ok=True)
    open(f'{d}/MELONBUILD', 'w').write('\n'.join(lines) + '\n')

for row in ORDER: write(*row)
print(' '.join(r[0] for r in ORDER))
