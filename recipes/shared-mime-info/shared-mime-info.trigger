#!/bin/sh
# a package added or removed MIME definitions: rebuild the cache Qt, KDE and GLib read
exec update-mime-database /usr/share/mime
