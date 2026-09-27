#!/bin/bash
# retry-failed.sh <log> <newlog> [extra recipes...]: wait for a build-all run (its log) to finish,
# then build the recipes that failed in it again (in the same order), plus any extras first.
. /home/claude/melon/scripts/env.sh
log=$1 new=$2; shift 2
until grep -q '^EXIT' "$log" 2>/dev/null; do sleep 30; done
failed=$(grep '^##### FAILED: ' "$log" | sed 's/^##### FAILED: //')
[ -z "$failed$*" ] && { echo "EXIT 0" > "$new"; exit 0; }
MELON_KEEP_GOING=1 $M/scripts/build-all.sh "$@" $failed > "$new" 2>&1
echo "EXIT $?" >> "$new"
