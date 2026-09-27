#!/bin/bash
# Qt 6 -> KF6 -> Plasma and apps (order from gen-kde-recipes.py), then the Qt-dependent installer and
# Flatpak pieces. Waits for the host Qt tools first.
. /home/claude/melon/scripts/env.sh
until grep -q '^EXIT' $M/logs/host-qt.log 2>/dev/null; do sleep 60; done
grep -q '^EXIT 0' $M/logs/host-qt.log || { echo "host Qt failed"; exit 1; }
python3 $M/scripts/gen-kde-recipes.py > $M/work/kde-order.txt
$M/scripts/build-all.sh $(cat $M/work/kde-order.txt) > $M/logs/build-kde.log 2>&1; echo "EXIT $?" >> $M/logs/build-kde.log
grep -q '^EXIT 0' $M/logs/build-kde.log || exit 1
# the side queue must have finished the plain libraries these need
until grep -q '^EXIT' $M/logs/build-q3.log 2>/dev/null; do sleep 60; done
MELON_KEEP_GOING=1 $M/scripts/build-all.sh appstream flatpak xdg-desktop-portal kpmcore calamares > $M/logs/build-q4b.log 2>&1
echo "EXIT $?" >> $M/logs/build-q4b.log
