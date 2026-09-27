#!/bin/bash
# fetch-sources.sh: download every source file listed in sources/MANIFEST.tsv and check its sha256.
# For a new build machine; the files are the same ones the melon packages were built from.
#   Ubuntu source archive (apt-get source, falling back to Launchpad's permanent file store),
#   Ubuntu archive pool (.debs), and git tags on GitHub.
set -uo pipefail
M=$(cd "$(dirname "$0")/.." && pwd)
S=$M/sources
mkdir -p $S/deb $S/firmware $S/fonts $S/git
fail=0
ok(){ [ -f "$S/$1" ] && [ "$2" = - -o "$(sha256sum "$S/$1" | cut -d' ' -f1)" = "$2" ]; }

while IFS=$'\t' read -r path sum method arg; do
  case $path in '#'*|'') continue ;; esac
  [ $method = link ] && continue
  ok "$path" "$sum" && continue
  file=${path##*/}
  case $method in
    apt)
      src=${arg%%=*} ver=${arg#*=}
      ( cd $S/deb && apt-get source --download-only -q "$arg" >/dev/null 2>&1 )
      if ! ok "$path" "$sum"; then   # an older version the archive no longer lists: Launchpad keeps every file
        curl -fsSL -o "$S/$path" "https://launchpad.net/ubuntu/+archive/primary/+sourcefiles/$src/${ver#*:}/$file" || true
      fi ;;
    pool)  curl -fsSL -o "$S/$path" "$arg" || true ;;
    inner) set -- $arg; tar -xOf "$S/$1" "$2" > "$S/$path" 2>/dev/null || true ;;
    git)
      set -- $arg; repo=$1 tag=$2; name=${file%.tar.*}
      d=$S/git/$(basename $repo).git
      [ -d $d ] || git clone -q --bare "$repo" $d
      git -C $d fetch -q --tags origin 2>/dev/null
      git -C $d archive --format=tar.gz --prefix=$name/ "$tag" > "$S/$path" || true
      sum=- ;;   # gzip bytes differ between git versions; the tag pins the content
  esac
  if ok "$path" "$sum"; then echo "ok    $path"; else echo "FAIL  $path ($method $arg)"; fail=$((fail + 1)); fi
done < $S/MANIFEST.tsv

# the names the recipes use (symlinks into deb/, firmware/, fonts/)
while IFS=$'\t' read -r path sum method arg; do
  [ "$method" = link ] || continue
  ln -sfn "$arg" "$S/$path"
done < $S/MANIFEST.tsv
[ $fail = 0 ] && echo "all sources present" || { echo "$fail files missing"; exit 1; }
