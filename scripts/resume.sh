#!/bin/bash
# resume.sh: restart the desktop build after the container was reclaimed (background jobs die with it).
# Safe to run any time: finished host Qt modules and already-built packages are skipped.
cd "$(dirname "$(readlink -f "$0")")/.."
running(){ pgrep -f "$1" >/dev/null; }
if ! grep -q '^EXIT 0' logs/host-qt.log 2>/dev/null && ! running 'scripts/host-qt.sh'; then
  nohup sh -c 'scripts/host-qt.sh > logs/host-qt.log 2>&1; echo "EXIT $?" >> logs/host-qt.log' >/dev/null 2>&1 &
  echo "host Qt: restarted"
fi
if ! running 'build-all.sh mesa' && ! grep -q '^EXIT 0' logs/build-mesa2.log 2>/dev/null; then
  nohup sh -c 'nice -n 19 env MELON_AUTO_RESUME=1 scripts/build-all.sh mesa > logs/build-mesa2.log 2>&1; echo "EXIT $?" >> logs/build-mesa2.log' >/dev/null 2>&1 &
  echo "Mesa: restarted"
fi
if ! running 'scripts/queue-4.sh'; then
  nohup scripts/queue-4.sh > logs/queue-4.log 2>&1 &
  echo "Qt/KDE queue: restarted"
fi
