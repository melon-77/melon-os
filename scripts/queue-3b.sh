#!/bin/bash
# retry the side-queue packages that needed recipe fixes, after queue-3 is done
. /home/claude/melon/scripts/env.sh
until grep -q '^EXIT' $M/logs/build-q3.log 2>/dev/null; do sleep 30; done
MELON_KEEP_GOING=1 $M/scripts/build-all.sh sqlite libyaml libfyaml yaml-cpp cryptsetup python3 libndp libdisplay-info \
  $(grep '^##### FAILED: ' $M/logs/build-q3.log | sed 's/^##### FAILED: //' | tr ' ' '\n' | grep -vxE 'sqlite|libyaml|libfyaml|yaml-cpp|cryptsetup|python3|libndp|libdisplay-info') \
  > $M/logs/build-q3b.log 2>&1
echo "EXIT $?" >> $M/logs/build-q3b.log
