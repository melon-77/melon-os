#!/usr/bin/env bash
# Publish site/ as the website: adds one commit with the contents of site/ to the gh-pages branch of
# github.com/melon-77/melon-os, which GitHub Pages serves at https://melon-77.github.io/melon-os/.
# Usage: scripts/publish-site.sh [remote-url]     (default: this checkout's "origin")
set -euo pipefail
M=$(cd "$(dirname "$0")/.." && pwd)
remote=${1:-$(git -C "$M" remote get-url origin)}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
git clone --quiet --branch gh-pages --single-branch "$remote" "$tmp/pages"
cd "$tmp/pages"
git rm -rq --ignore-unmatch . >/dev/null
cp -a "$M/site/." .
git add -A
if git diff --cached --quiet; then echo "gh-pages is already up to date"; exit 0; fi
git commit -q -m "Site: $(git -C "$M" log -1 --format=%s -- site)"
git push origin gh-pages
