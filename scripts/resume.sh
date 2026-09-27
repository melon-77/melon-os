#!/bin/bash
# resume.sh: restart the desktop build after the container was reclaimed (background jobs die with it).
# Safe to run any time: finished host Qt modules and already-built packages are skipped.
cd /home/claude/melon
running(){ pgrep -f "$1" >/dev/null; }
if ! grep -q '^EXIT 0' logs/host-qt.log 2>/dev/null && ! running 'scripts/host-qt.sh'; then
  nohup sh -c 'scripts/host-qt.sh > logs/host-qt.log 2>&1; echo "EXIT $?" >> logs/host-qt.log' >/dev/null 2>&1 &
  echo "host Qt: restarted"
fi
if ! running 'scripts/queue-4.sh'; then
  nohup scripts/queue-4.sh > logs/queue-4.log 2>&1 &
  echo "Qt/KDE queue: restarted"
fi
