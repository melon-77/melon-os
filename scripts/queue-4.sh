#!/bin/bash
# Qt 6 -> KF6 -> Plasma and apps (order from gen-kde-recipes.py), then the Qt-dependent installer and
# Flatpak pieces. Waits for the host Qt tools first.
. "$(dirname "$(readlink -f "$0")")/env.sh"
until grep -q '^EXIT' $M/logs/host-qt.log 2>/dev/null; do sleep 60; done
grep -q '^EXIT 0' $M/logs/host-qt.log || { echo "host Qt failed"; exit 1; }
python3 $M/scripts/gen-kde-recipes.py > $M/work/kde-order.txt
MELON_AUTO_RESUME=1 MELON_SKIP_BUILT=1 $M/scripts/build-all.sh $(cat $M/work/kde-order.txt) > $M/logs/build-kde.log 2>&1; echo "EXIT $?" >> $M/logs/build-kde.log
grep -q '^EXIT 0' $M/logs/build-kde.log || exit 1
# the side queue must have finished the plain libraries these need
MELON_AUTO_RESUME=1 MELON_SKIP_BUILT=1 MELON_KEEP_GOING=1 $M/scripts/build-all.sh appstream flatpak xdg-desktop-portal kpmcore calamares calamares-melon melon-desktop > $M/logs/build-q4b.log 2>&1
echo "EXIT $?" >> $M/logs/build-q4b.log
