#!/bin/bash
# build a list of recipes in order; stop at the first failure
. /home/claude/melon/scripts/env.sh
for p in "$@"; do
  echo "##### $(date +%T) $p"
  if ! $M/scripts/melon-build $p > $M/logs/pkg-$p.log 2>&1; then echo "##### FAILED $p"; tail -25 $M/logs/pkg-$p.log; exit 1; fi
  grep 'created' $M/logs/pkg-$p.log | sed 's/\x1b\[[0-9;]*m//g'
done
echo "##### ALL DONE $(date +%T)"
