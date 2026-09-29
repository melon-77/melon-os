#!/bin/sh
# a package added or removed commands: restore any missing BusyBox applet links
/usr/bin/busybox --install -s 2>/dev/null
exit 0
