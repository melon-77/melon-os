#!/usr/bin/env python3
"""Generate MELONBUILD recipes for simple libraries from one table.

Each row: recipe name, version, tarball in sources/deb, top directory, build system, makedepends,
extra configure/meson/cmake options, and flags. Recipes that need special handling are written by hand.
"""
import os, sys
M = '/home/claude/melon'
ROWS = [
 # name, ver, orig tarball, topdir, system, makedepends, options, extra
 ('libpciaccess','0.18.1','libpciaccess_0.18.1.orig.tar.gz','libpciaccess-0.18.1','meson','zlib-dev',"-Dzlib=enabled",''),
 ('libdrm','2.4.131','libdrm_2.4.131.orig.tar.xz','libdrm-2.4.131','meson','libpciaccess-dev',"-Dintel=enabled -Dradeon=enabled -Damdgpu=enabled -Dnouveau=enabled -Dvmwgfx=enabled -Dtests=false -Dman-pages=disabled -Dvalgrind=disabled -Dcairo-tests=disabled -Dudev=false",''),
 ('wayland','1.24.0','wayland_1.24.0.orig.tar.gz','wayland-1.24.0','meson','libffi-dev expat-dev',"-Ddocumentation=false -Dtests=false -Ddtd_validation=false",''),
 ('wayland-protocols','1.47','wayland-protocols_1.47.orig.tar.xz','wayland-protocols-1.47','meson','wayland-dev',"-Dtests=false",'noarch'),
 ('xkeyboard-config','2.46','xkeyboard-config_2.46.orig.tar.xz','xkeyboard-config-2.46','meson','',"-Dxkb-base=/usr/share/X11/xkb -Dcompat-rules=true -Dxorg-rules-symlinks=true",'noarch'),
 ('libxkbcommon','1.13.1','libxkbcommon_1.13.1.orig.tar.gz','xkbcommon-libxkbcommon-920ea79','meson','libxcb-dev wayland-dev wayland-protocols libxml2-dev xkeyboard-config',"-Denable-docs=false -Denable-x11=true -Denable-wayland=true -Denable-tools=true -Dxkb-config-root=/usr/share/X11/xkb -Denable-bash-completion=false",''),
 ('libevdev','1.13.6','libevdev_1.13.6+dfsg.orig.tar.xz','libevdev-1.13.6','meson','',"-Dtests=disabled -Ddocumentation=disabled",''),
 ('mtdev','1.1.7','mtdev_1.1.7.orig.tar.gz','mtdev-1.1.7','auto','',"--disable-static",''),
 ('libgudev','238','libgudev_238.orig.tar.xz','libgudev-238','meson','glib-dev eudev-dev',"-Dintrospection=disabled -Dvapi=disabled -Dtests=disabled",''),
 ('libwacom','2.18.0','libwacom_2.18.0.orig.tar.gz','libwacom-libwacom-2.18.0','meson','glib-dev eudev-dev libevdev-dev libgudev-dev',"-Dtests=disabled -Ddocumentation=disabled -Dudev-dir=/usr/lib/udev",''),
 ('libinput','1.31.1','libinput_1.31.1.orig.tar.gz','libinput-1.31.1-1920686963fe224f7d2f4fe195cbed651e11d455','meson','eudev-dev libevdev-dev mtdev-dev libwacom-dev',"-Ddocumentation=false -Dtests=false -Ddebug-gui=false -Dlibwacom=true -Dudev-dir=/usr/lib/udev -Dlua-plugins=disabled",''),
 ('pixman','0.46.4','pixman_0.46.4.orig.tar.gz','pixman-0.46.4','meson','',"-Dtests=disabled -Ddemos=disabled -Dgtk=disabled -Dlibpng=disabled",''),
 ('libpng','1.6.57','libpng1.6_1.6.57.orig.tar.gz','libpng-1.6.57','auto','zlib-dev',"--disable-static",''),
 ('libjpeg-turbo','2.1.5','libjpeg-turbo_2.1.5.orig.tar.gz','libjpeg-turbo-2.1.5','cmake','',"-DENABLE_STATIC=OFF -DWITH_JPEG8=ON -DCMAKE_INSTALL_DEFAULT_LIBDIR=lib",''),
 ('freetype','2.14.2','freetype_2.14.2+dfsg.orig.tar.xz','freetype-2.14.2','meson','zlib-dev libpng-dev',"-Dharfbuzz=disabled -Dbrotli=disabled -Dbzip2=disabled -Dpng=enabled -Dzlib=system",''),
 ('harfbuzz','12.3.2','harfbuzz_12.3.2.orig.tar.xz','harfbuzz-12.3.2','meson','freetype-dev glib-dev',"-Dtests=disabled -Ddocs=disabled -Dintrospection=disabled -Dcairo=disabled -Dicu=disabled -Dfreetype=enabled -Dglib=enabled -Dgobject=disabled -Dutilities=disabled",''),
 ('fontconfig','2.17.1','fontconfig_2.17.1.orig.tar.gz','fontconfig-2.17.1-6d0a98982ec351c165c9224c8b7dbdfca3010e47','meson','freetype-dev expat-dev',"-Ddoc=disabled -Dtests=disabled -Dtools=enabled -Dcache-build=disabled -Dnls=disabled",''),
 ('libepoxy','1.5.10','libepoxy_1.5.10.orig.tar.gz','libepoxy-1.5.10','meson','mesa-dev',"-Ddocs=false -Dtests=false -Dx11=true -Degl=yes -Dglx=yes",''),
 ('xorgproto','2025.1','xorgproto_2025.1.orig.tar.gz','xorgproto-2025.1','meson','',"-Dlegacy=false",'noarch'),
 ('libxau','1.0.11','libxau_1.0.11.orig.tar.gz','libXau-1.0.11','auto','xorgproto',"--disable-static",''),
 ('libxdmcp','1.1.5','libxdmcp_1.1.5.orig.tar.gz','libXdmcp-1.1.5','auto','xorgproto',"--disable-static --disable-docs",''),
 ('xcb-proto','1.17.0','xcb-proto_1.17.0.orig.tar.gz','xcb-proto-1.17.0','auto','',"",'noarch'),
 ('libxcb','1.17.0','libxcb_1.17.0.orig.tar.gz','libxcb-1.17.0','auto','xcb-proto libxau-dev libxdmcp-dev',"--disable-static --disable-devel-docs --without-doxygen --enable-xinput --enable-xkb",'xcb'),
 ('xtrans','1.6.0','xtrans_1.6.0.orig.tar.gz','xtrans-1.6.0','auto','',"--disable-docs",'noarch'),
 ('libx11','1.8.13','libx11_1.8.13.orig.tar.gz','libX11-1.8.13','auto','libxcb-dev xtrans xorgproto',"--disable-static --disable-specs --without-xmlto --without-fop --enable-malloc0returnsnull=no",''),
 ('libxext','1.3.4','libxext_1.3.4.orig.tar.gz','libXext-1.3.4','auto','libx11-dev',"--disable-static --disable-specs --without-xmlto --enable-malloc0returnsnull=no",''),
 ('libxfixes','6.0.0','libxfixes_6.0.0.orig.tar.gz','libXfixes-6.0.0','auto','libx11-dev',"--disable-static",''),
 ('libxrender','0.9.12','libxrender_0.9.12.orig.tar.gz','libXrender-0.9.12','auto','libx11-dev',"--disable-static --enable-malloc0returnsnull=no",''),
 ('libxrandr','1.5.4','libxrandr_1.5.4.orig.tar.gz','libXrandr-1.5.4','auto','libxext-dev libxrender-dev',"--disable-static --enable-malloc0returnsnull=no",''),
 ('libxi','1.8.2','libxi_1.8.2.orig.tar.gz','libXi-1.8.2','auto','libxext-dev libxfixes-dev',"--disable-static --disable-docs --disable-specs --enable-malloc0returnsnull=no",''),
 ('libxtst','1.2.5','libxtst_1.2.5.orig.tar.gz','libXtst-1.2.5','auto','libxi-dev',"--disable-static --disable-specs",''),
 ('libxkbfile','1.1.0','libxkbfile_1.1.0.orig.tar.gz','libxkbfile-1.1.0','auto','libx11-dev',"--disable-static",''),
 ('libfontenc','1.1.8','libfontenc_1.1.8.orig.tar.gz','libfontenc-1.1.8','auto','zlib-dev xorgproto',"--disable-static",''),
 ('libxfont2','2.0.6','libxfont_2.0.6.orig.tar.gz','libXfont2-2.0.6','auto','freetype-dev libfontenc-dev xtrans',"--disable-static --disable-devel-docs",''),
 ('libxcvt','0.1.3','libxcvt_0.1.3.orig.tar.xz','libxcvt-0.1.3','meson','',"",''),
 ('libxshmfence','1.3.3','libxshmfence_1.3.3.orig.tar.gz','libxshmfence-1.3.3','auto','xorgproto',"--disable-static",''),
 ('libxxf86vm','1.1.4','libxxf86vm_1.1.4.orig.tar.gz','libXxf86vm-1.1.4','auto','libxext-dev',"--disable-static --enable-malloc0returnsnull=no",''),
 ('libxdamage','1.1.7','libxdamage_1.1.7.orig.tar.xz','libXdamage-1.1.7','auto','libxfixes-dev',"--disable-static",''),
 ('libxcomposite','0.4.6','libxcomposite_0.4.6.orig.tar.gz','libXcomposite-0.4.6','auto','libxfixes-dev',"--disable-static",''),
 ('libxcursor','1.2.3','libxcursor_1.2.3.orig.tar.gz','libXcursor-1.2.3','auto','libxrender-dev libxfixes-dev',"--disable-static",''),
 ('libxinerama','1.1.4','libxinerama_1.1.4.orig.tar.gz','libXinerama-1.1.4','auto','libxext-dev',"--disable-static --enable-malloc0returnsnull=no",''),
 ('libxml2','2.15.2','libxml2_2.15.2+dfsg.orig.tar.xz','libxml2-2.15.2','meson','zlib-dev',"-Dpython=disabled -Dicu=disabled -Dlzma=disabled -Dzlib=enabled -Dhistory=disabled -Dreadline=disabled -Ddocs=disabled",''),
 ('spirv-headers','1.4.341.0','spirv-headers_1.6.1+1.4.341.0.orig.tar.gz','spirv-headers-1.6.1+1.4.341.0','cmake','',"",'noarch'),
 ('spirv-tools','2026.1','spirv-tools_2026.1.orig.tar.gz','KhronosGroup-SPIRV-Tools-fbe4f3a','cmake','spirv-headers',"-DSPIRV-Headers_SOURCE_DIR=$SYSROOT/usr -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=OFF -DSPIRV_WERROR=OFF -DSPIRV_TOOLS_BUILD_STATIC=OFF -DBUILD_SHARED_LIBS=ON",''),
 ('glslang','16.2.0','glslang_16.2.0.orig.tar.gz','glslang-16.2.0','cmake','spirv-tools-dev',"-DALLOW_EXTERNAL_SPIRV_TOOLS=ON -DGLSLANG_TESTS=OFF -DENABLE_OPT=ON -DBUILD_SHARED_LIBS=ON",''),
 ('libva','2.23.0','libva_2.23.0.orig.tar.gz','libva-2.23.0','meson','libdrm-dev wayland-dev libx11-dev libxext-dev libxfixes-dev',"-Dwith_glx=no -Dwith_wayland=yes -Dwith_x11=yes -Denable_docs=false",''),
 ('libvdpau','1.5','libvdpau_1.5.orig.tar.bz2','libvdpau-1.5','meson','libx11-dev libxext-dev',"-Ddocumentation=false",''),
]

def recipe(r):
    name, ver, tb, top, system, mdeps, opts, extra = r
    base = os.path.basename(tb)
    link = f'{name}-{ver}.' + ('tar.xz' if tb.endswith('.xz') else 'tar.bz2' if tb.endswith('.bz2') else 'tar.gz')
    src = f'{M}/sources/{link}'
    if not os.path.lexists(src): os.symlink(f'deb/{tb}', src)
    lines = [f'pkgname={name}', f'pkgver={ver}', f'pkgdesc="{name} (graphics stack)"', 'license=custom', f'source=({link})',
             f'builddir=$srcdir/{top}']
    if mdeps: lines.append(f'makedepends=({mdeps})')
    if 'noarch' not in extra: lines.append(f'subpackages=({name}-dev)')
    if system == 'meson':
        lines.append(f'build(){{ meson_setup {opts} build; ninja -C build; }}')
        lines.append('package(){ DESTDIR=$pkgdir ninja -C build install; rm -rf $pkgdir/usr/share/doc $pkgdir/usr/share/man; }')
    elif system == 'cmake':
        lines.append(f'build(){{ cmake_setup -B build {opts}; ninja -C build; }}')
        lines.append('package(){ DESTDIR=$pkgdir ninja -C build install; rm -rf $pkgdir/usr/share/doc $pkgdir/usr/share/man; }')
    else:
        pre = ''
        if extra == 'xcb':
            pre = 'export XCBPROTO_XCBINCLUDEDIR=$SYSROOT/usr/share/xcb XCBPROTO_XCBPYTHONDIR=$(ls -d $SYSROOT/usr/lib/python3*/site-packages | head -1); '
        lines.append(f'build(){{ {pre}[ -x configure ] || autoreconf -fi; ./configure $conf_flags {opts}; make -j$JOBS; }}')
        lines.append('package(){ make DESTDIR=$pkgdir install; rm -rf $pkgdir/usr/share/doc $pkgdir/usr/share/man $pkgdir/usr/share/info; }')
    if 'noarch' in extra: lines.append("options=('!strip')")
    d = f'{M}/recipes/{name}'; os.makedirs(d, exist_ok=True)
    open(f'{d}/MELONBUILD', 'w').write('\n'.join(lines) + '\n')
    return name

names = [recipe(r) for r in ROWS]
print(' '.join(names))
