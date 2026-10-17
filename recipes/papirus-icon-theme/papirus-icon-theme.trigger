#!/bin/sh
# the icon caches GTK reads (a theme without one is read file by file, which is slow for 46 000 icons)
for d in /usr/share/icons/Papirus /usr/share/icons/Papirus-Dark; do
  [ -d "$d" ] && command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -f "$d" 2>/dev/null
done
exit 0
