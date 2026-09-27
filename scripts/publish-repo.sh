#!/bin/bash
# publish-repo.sh: publish the package repository (repo/<arch>/) on GitHub.
#
# The packages live on the "packages" branch of github.com/xbfj/melon-os as ONE commit that is replaced
# on every publish (force push), so the git history never fills up with old binaries. GitHub serves the
# files at
#     https://raw.githubusercontent.com/xbfj/melon-os/packages/<arch>/Packages.adb
# which is what installed systems have in /etc/apk/repositories. Every package and the index are signed
# with keys/melon-signing.rsa, so the hosting doesn't need to be trusted.
#
# GitHub refuses files over 100 MB; split big packages (see linux-firmware) before publishing.
set -euo pipefail
. /home/claude/melon/scripts/env.sh
W=$M/work/publish
URL=$(git -C $M remote get-url origin)
rm -rf $W; mkdir -p $W; cd $W
git init -q -b packages
for d in $REPO/*/; do
  a=$(basename $d); [ -f $d/Packages.adb ] || continue
  big=$(find $d -name '*.apk' -size +99M)
  [ -z "$big" ] || { echo "publish-repo: over GitHub's 100 MB limit: $big" >&2; exit 1; }
  mkdir -p $a; cp -l $d/*.apk $d/Packages.adb $a/ 2>/dev/null || cp $d/*.apk $d/Packages.adb $a/
  echo "$a: $(ls $a/*.apk | wc -l) packages, $(du -sh $a | cut -f1)"
done
cat > README.md <<'EOF'
# melon package repository

This branch is the apk repository for melon Linux, not source code. It is rewritten on every publish.

    https://raw.githubusercontent.com/xbfj/melon-os/packages/x86_64/Packages.adb

Packages and index are signed with the melon key (`keys/melon-signing.rsa.pub` on the main branch).
EOF
git add -A
git -c user.name="melon repo" -c user.email="noreply@github.com" commit -q -m "melon packages $(date -u +%Y-%m-%d)"
git push -q -f "$URL" packages
echo "published: https://raw.githubusercontent.com/xbfj/melon-os/packages/"
