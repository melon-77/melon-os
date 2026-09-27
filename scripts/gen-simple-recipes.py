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
 ('sqlite','3.46.1','sqlite3_3.46.1.orig.tar.xz','sqlite3-3.46.1','auto','zlib-dev',"--disable-static --enable-threadsafe --disable-readline --disable-editline",''),
 ('libxslt','1.1.45','libxslt_1.1.45.orig.tar.xz','libxslt-1.1.45','auto','libxml2-dev',"--disable-static --without-python --without-crypto --without-debugger",''),
 ('libseccomp','2.6.0','libseccomp_2.6.0.orig.tar.gz','libseccomp-2.6.0','auto','',"--disable-static --disable-python",''),
 ('libusb','1.0.29','libusb-1.0_1.0.29.orig.tar.bz2','libusb-1.0.29','auto','eudev-dev',"--disable-static",''),
 ('sbc','2.1','sbc_2.1.orig.tar.gz','sbc-2.1','auto','',"--disable-static --disable-tester --disable-tools",''),
 ('libndp','1.9','libndp_1.9.orig.tar.gz','libndp-1.9','auto','',"--disable-static CFLAGS=\"$CFLAGS -Wno-error=incompatible-pointer-types\"",''),
 ('libyaml','0.2.5','libyaml_0.2.5.orig.tar.gz','libyaml-0.2.5','auto','',"--disable-static",''),
 ('libfyaml','0.9.4','libfyaml_0.9.4.orig.tar.gz','pantoniou-libfyaml-deb1ec7','cmake','',"-DBUILD_SHARED_LIBS=ON -DENABLE_NETWORK=OFF -DBUILD_TESTING=OFF",''),
 ('libpsl','0.21.2','libpsl_0.21.2.orig.tar.xz','libpsl-0.21.2','meson','',"-Dtests=false -Ddocs=false -Druntime=no -Dbuiltin=true -Dpsl_file=/usr/share/publicsuffix/public_suffix_list.dat",''),
 ('nghttp2','1.68.0','nghttp2_1.68.0.orig.tar.gz','nghttp2-1.68.0','cmake','openssl-dev zlib-dev',"-DENABLE_LIB_ONLY=ON -DENABLE_STATIC_LIB=OFF -DBUILD_STATIC_LIBS=OFF",''),
 ('curl','8.18.0','curl_8.18.0.orig.tar.gz','curl-8.18.0','auto','openssl-dev zlib-dev zstd-dev nghttp2-dev libpsl-dev',"--disable-static --with-openssl --with-nghttp2 --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt --without-libidn2 --without-brotli --disable-manual --disable-ldap --enable-ipv6",''),
 ('libxmlb','0.3.24','libxmlb_0.3.24.orig.tar.gz','libxmlb-0.3.24','meson','glib-dev zstd-dev',"-Dintrospection=false -Dgtkdoc=false -Dtests=false -Dcli=false -Dlzma=disabled -Dzstd=enabled -Dstemmer=false",''),
 ('xcb-util','0.4.1','xcb-util_0.4.1.orig.tar.gz','xcb-util-0.4.1','auto','libxcb-dev',"--disable-static",''),
 ('xcb-util-renderutil','0.3.10','xcb-util-renderutil_0.3.10.orig.tar.xz','xcb-util-renderutil-0.3.10','auto','libxcb-dev',"--disable-static",''),
 ('xcb-util-image','0.4.0','xcb-util-image_0.4.0.orig.tar.bz2','xcb-util-image-0.4.0','auto','xcb-util-dev',"--disable-static",''),
 ('xcb-util-keysyms','0.4.1','xcb-util-keysyms_0.4.1.orig.tar.xz','xcb-util-keysyms-0.4.1','auto','libxcb-dev',"--disable-static",''),
 ('xcb-util-wm','0.4.2','xcb-util-wm_0.4.2.orig.tar.xz','xcb-util-wm-0.4.2','auto','libxcb-dev',"--disable-static",''),
 ('xcb-util-cursor','0.1.6','xcb-util-cursor_0.1.6.orig.tar.gz','xcb-util-cursor-0.1.6','auto','xcb-util-renderutil-dev xcb-util-image-dev',"--disable-static",''),
 ('hwdata','0.394','hwdata_0.394.orig.tar.gz','hwdata-0.394','auto','',"--datadir=/usr/share --disable-blacklist",'noarch'),
 ('libdisplay-info','0.3.0','libdisplay-info_0.3.0.orig.tar.bz2','libdisplay-info-0.3.0','meson','hwdata',"",''),
 ('lcms2','2.17','lcms2_2.17.orig.tar.gz','lcms2-2.17','meson','libjpeg-turbo-dev',"-Dtests=disabled -Dutils=false -Dsamples=false",''),
 ('shared-mime-info','2.4','shared-mime-info_2.4.orig.tar.bz2','shared-mime-info-2.4','meson','libxml2-dev glib-dev',"-Dupdate-mimedb=false -Dbuild-translations=false -Dbuild-tests=false -Dbuild-tools=true",''),
 ('hicolor-icon-theme','0.18','hicolor-icon-theme_0.18.orig.tar.xz','hicolor-icon-theme-0.18','meson','',"",'noarch'),
 ('pipewire','1.6.2','pipewire_1.6.2.orig.tar.gz','pipewire-1.6.2-95da54a482b68475958bbc3fa572a9c20df0df74','meson','dbus-dev alsa-lib-dev eudev-dev glib-dev sbc-dev linux-headers',"-Dsystemd=disabled -Dsystemd-system-service=disabled -Dsystemd-user-service=disabled -Dsession-managers=[] -Dalsa=enabled -Dpipewire-alsa=enabled -Djack=disabled -Dpipewire-jack=disabled -Dbluez5=enabled -Dbluez5-codec-aptx=disabled -Dbluez5-codec-ldac=disabled -Dbluez5-codec-lc3=disabled -Dbluez5-codec-lc3plus=disabled -Dbluez5-codec-opus=disabled -Dbluez5-codec-aac=disabled -Dlibcamera=disabled -Dvulkan=disabled -Dgstreamer=disabled -Dexamples=disabled -Dtests=disabled -Dman=disabled -Ddocs=disabled -Davahi=disabled -Decho-cancel-webrtc=disabled -Droc=disabled -Dlibpulse=disabled -Dudev=enabled -Dselinux=disabled -Dsnap=disabled -Dlibffado=disabled -Draop=disabled -Dopus=disabled -Dlv2=disabled -Dsdl2=disabled -Dsndfile=disabled -Dlibusb=disabled -Dreadline=disabled -Dflatpak=enabled -Dlibmysofa=disabled -Dlibcanberra=disabled -Dlegacy-rtkit=false -Dx11=disabled -Dx11-xfixes=disabled -Dpipewire-v4l2=disabled -Dv4l2=disabled -Dbluez5-backend-native=enabled -Dbluez5-backend-ofono=disabled -Dbluez5-backend-hsphfpd=disabled -Dcompress-offload=disabled -Debur128=disabled -Dfftw=disabled -Donnxruntime=disabled",''),
 ('wireplumber','0.5.13','wireplumber_0.5.13.orig.tar.gz','wireplumber-0.5.13-84429b47943d789389fbde17c06b82efb197d04e','meson','pipewire-dev lua5.4-dev glib-dev elogind-dev',"-Dsystem-lua=true -Dsystem-lua-version=5.4 -Dintrospection=disabled -Ddoc=disabled -Dsystemd=disabled -Dsystemd-user-service=false -Dsystemd-system-service=false -Delogind=enabled -Dtests=false -Ddbus-tests=false",''),
 ('networkmanager','1.54.3','network-manager_1.54.3.orig.tar.bz2','NetworkManager-1.54.3','meson','glib-dev dbus-dev eudev-dev util-linux-dev libndp-dev polkit-dev elogind-dev linux-headers',"-Dsystemdsystemunitdir=no -Dsystemd_journal=false -Dsession_tracking=elogind -Dsession_tracking_consolekit=false -Dsuspend_resume=elogind -Dpolkit=true -Dconfig_auth_polkit_default=true -Dselinux=false -Dlibaudit=no -Dmodem_manager=false -Dppp=false -Dnm_cloud_setup=false -Dbluez5_dun=false -Debpf=false -Dwifi=true -Diwd=false -Dteamdctl=false -Dovs=false -Dnmcli=false -Dnmtui=false -Dintrospection=false -Dvapi=false -Ddocs=false -Dtests=no -Dfirewalld_zone=false -Dcrypto=null -Dqt=false -Dlibpsl=false -Dconcheck=false -Djson_validation=false -Dreadline=none -Dmore_asserts=no -Ddhcpcd=no -Ddhclient=no -Dresolvconf=no -Dnetconfig=no -Dconfig_dns_rc_manager_default=file -Dudev_dir=/usr/lib/udev -Dconfig_plugins_default=keyfile -Difupdown=false -Dmodify_system=true",''),
 ('bluez','5.85','bluez_5.85.orig.tar.gz','bluez-5.85','auto','glib-dev dbus-dev eudev-dev linux-headers',"--disable-systemd --enable-library --disable-cups --disable-obex --disable-mesh --disable-midi --disable-manpages --disable-client --disable-monitor --with-udevdir=/usr/lib/udev --with-dbusconfdir=/usr/share --disable-asha --disable-bap --disable-bass --disable-mcp --disable-vcp --disable-micp --disable-csip",''),
 ('upower','1.91.1','upower_1.91.1.orig.tar.bz2','upower-v1.91.1','meson','glib-dev libgudev-dev polkit-dev',"-Dsystemdsystemunitdir=no -Didevice=disabled -Dintrospection=disabled -Dgtk-doc=false -Dman=false -Dos_backend=linux -Dudevrulesdir=/usr/lib/udev/rules.d -Dudevhwdbdir=/usr/lib/udev/hwdb.d -Dpolkit=enabled",''),
 ('power-profiles-daemon','0.30','power-profiles-daemon_0.30.orig.tar.bz2','power-profiles-daemon-0.30','meson','glib-dev libgudev-dev polkit-dev upower-dev',"-Dsystemdsystemunitdir=/usr/lib/systemd/system -Dpylint=disabled -Dtests=false -Dmanpage=disabled -Dbashcomp=disabled -Dzshcomp=/nonexistent",''),
 ('vulkan-headers','1.4.341','../vulkan-headers-1.4.341.tar.gz','vulkan-headers-1.4.341','cmake','',"",'noarch'),
 ('gdk-pixbuf','2.44.5','gdk-pixbuf_2.44.5+dfsg.orig.tar.xz','gdk-pixbuf-2.44.5','meson','glib-dev libpng-dev libjpeg-turbo-dev shared-mime-info',"-Dintrospection=disabled -Dgtk_doc=false -Dman=false -Dtests=false -Dinstalled_tests=false -Dothers=disabled -Dtiff=disabled -Dgif=disabled -Dglycin=disabled -Dbuiltin_loaders=png,jpeg",''),
 ('npth','1.8','npth_1.8.orig.tar.bz2','npth-1.8','auto','',"--disable-static",''),
 ('libgpg-error','1.58','libgpg-error_1.58.orig.tar.bz2','libgpg-error-1.58','auto','',"--disable-static --disable-doc --disable-tests --disable-nls --enable-install-gpg-error-config",''),
 ('libgcrypt','1.12.0','libgcrypt20_1.12.0.orig.tar.bz2','libgcrypt-1.12.0','auto','libgpg-error-dev',"--disable-static --disable-doc --with-libgpg-error-prefix=$SYSROOT/usr --disable-asm",''),
 ('libassuan','3.0.2','libassuan_3.0.2.orig.tar.bz2','libassuan-3.0.2','auto','libgpg-error-dev',"--disable-static --disable-doc --with-libgpg-error-prefix=$SYSROOT/usr",''),
 ('libksba','1.6.7','libksba_1.6.7.orig.tar.bz2','libksba-1.6.7','auto','libgpg-error-dev',"--disable-static --disable-doc --with-libgpg-error-prefix=$SYSROOT/usr",''),
 ('gnupg','2.4.8','gnupg2_2.4.8.orig.tar.bz2','gnupg-2.4.8','auto','libgcrypt-dev libassuan-dev libksba-dev npth-dev zlib-dev sqlite-dev',"--disable-doc --disable-ldap --disable-gnutls --disable-card-support --disable-ccid-driver --disable-tofu --disable-sqlite --disable-bzip2 --without-readline --disable-nls --disable-wks-tools --with-libgpg-error-prefix=$SYSROOT/usr --with-libgcrypt-prefix=$SYSROOT/usr --with-libassuan-prefix=$SYSROOT/usr --with-ksba-prefix=$SYSROOT/usr",''),
 ('gpgme','2.0.1','gpgme1.0_2.0.1.orig.tar.bz2','gpgme-2.0.1','auto','libassuan-dev libgpg-error-dev gnupg',"--disable-static --disable-gpg-test --disable-gpgsm-test --disable-gpgconf-test --disable-g13-test --enable-languages= --with-libgpg-error-prefix=$SYSROOT/usr --with-libassuan-prefix=$SYSROOT/usr",''),
 ('libarchive','3.8.5','libarchive_3.8.5.orig.tar.xz','libarchive-3.8.5','auto','zlib-dev zstd-dev openssl-dev',"--disable-static --without-xml2 --without-expat --without-lzma --without-bz2lib --without-lz4 --without-libb2 --without-iconv --disable-acl --disable-xattr",''),
 ('fuse3','3.18.2','fuse3_3.18.2.orig.tar.gz','fuse-3.18.2','meson','',"-Dexamples=false -Dutils=true -Dtests=false -Duseroot=false -Dinitscriptdir= -Dudevrulesdir=/usr/lib/udev/rules.d",''),
 ('bubblewrap','0.11.1','bubblewrap_0.11.1.orig.tar.xz','bubblewrap-0.11.1','meson','libcap-dev',"-Dman=disabled -Dbash_completion=disabled -Dzsh_completion=disabled -Dselinux=disabled -Dtests=false",''),
 ('xdg-dbus-proxy','0.1.7','xdg-dbus-proxy_0.1.7.orig.tar.xz','xdg-dbus-proxy-0.1.7','meson','glib-dev',"-Dman=disabled -Dtests=false",''),
 ('json-glib','1.10.8','json-glib_1.10.8+ds.orig.tar.xz','json-glib-1.10.8','meson','glib-dev',"-Dintrospection=disabled -Dgtk_doc=disabled -Dman=false -Dtests=false -Dnls=disabled",''),
 ('ostree','2025.7','ostree_2025.7.orig.tar.xz','libostree-2025.7','auto','glib-dev curl-dev gpgme-dev libarchive-dev fuse3-dev openssl-dev zlib-dev',"--with-curl --without-soup --without-soup3 --with-openssl --disable-gtk-doc --disable-man --without-selinux --without-libsystemd --without-dracut --without-mkinitcpio --without-grub2 --without-avahi --without-ed25519-libsodium --disable-glibtest --without-composefs --disable-introspection --without-libmount",''),
 # installer support (Calamares, LUKS)
 ('json-c','0.18','json-c_0.18+ds.orig.tar.xz','json-c-json-c-0.18-20240915','cmake','',"-DBUILD_STATIC_LIBS=OFF -DBUILD_TESTING=OFF -DDISABLE_WERROR=ON -DBUILD_APPS=OFF",''),
 ('popt','1.19','popt_1.19+dfsg.orig.tar.xz','popt-1.19','auto','',"--disable-static --disable-nls",''),
 ('yaml-cpp','0.8.0','yaml-cpp_0.8.0+dfsg.orig.tar.xz','jbeder-yaml-cpp-f732014','cmake','',"-DYAML_BUILD_SHARED_LIBS=ON -DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF -DYAML_CPP_BUILD_CONTRIB=OFF",''),
 ('dosfstools','4.2','dosfstools_4.2.orig.tar.gz','dosfstools-4.2','auto','',"--enable-compat-symlinks --without-udev",''),
 ('nano','8.7.1','nano_8.7.1.orig.tar.xz','nano-8.7.1','auto','ncurses-dev',"--disable-nls --enable-utf8 --disable-libmagic --sysconfdir=/etc",''),
]

def recipe(r):
    name, ver, tb, top, system, mdeps, opts, extra = r
    base = os.path.basename(tb)
    link = f'{name}-{ver}.' + ('tar.xz' if tb.endswith('.xz') else 'tar.bz2' if tb.endswith('.bz2') else 'tar.gz')
    src = f'{M}/sources/{link}'
    if not os.path.lexists(src): os.symlink(tb[3:] if tb.startswith('../') else f'deb/{tb}', src)
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
