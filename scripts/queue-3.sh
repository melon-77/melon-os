#!/bin/bash
# low-priority side queue (x86_64): host Python, then the libraries and services that don't need Qt,
# and the installer's non-Qt dependencies. Runs niced next to the Qt build.
. /home/claude/melon/scripts/env.sh
$M/scripts/host-python.sh > $M/logs/host-python.log 2>&1; echo "EXIT $?" >> $M/logs/host-python.log
MELON_KEEP_GOING=1 $M/scripts/build-all.sh \
  sqlite libyaml libfyaml json-c popt yaml-cpp dosfstools squashfs-tools device-mapper cryptsetup python3 \
  libseccomp libusb sbc libndp lua5.4 hwdata libdisplay-info lcms2 shared-mime-info hicolor-icon-theme \
  xcb-util xcb-util-renderutil xcb-util-image xcb-util-keysyms xcb-util-wm xcb-util-cursor \
  libpsl nghttp2 curl libxmlb libxslt vulkan-headers vulkan-loader gdk-pixbuf json-glib \
  pipewire wireplumber networkmanager bluez upower power-profiles-daemon \
  npth libgpg-error libgcrypt libassuan libksba gnupg gpgme libarchive fuse3 bubblewrap xdg-dbus-proxy ostree \
  gamemode melon-fonts > $M/logs/build-q3.log 2>&1
echo "EXIT $?" >> $M/logs/build-q3.log
