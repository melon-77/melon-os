#!/bin/bash
# fetch-sources.sh: download every source file listed in sources/MANIFEST.tsv and check its sha256.
# For a new build machine; the files are the same ones the melon packages were built from.
#   Ubuntu source archive (apt-get source, falling back to Launchpad's permanent file store),
#   Ubuntu archive pool (.debs), git tags on GitHub, and a few tarballs from upstream sites.
set -uo pipefail
M=$(cd "$(dirname "$0")/.." && pwd)
S=$M/sources
mkdir -p $S/deb $S/firmware $S/fonts $S/git
[ -f $S/MANIFEST.tsv ] || { echo "sources/MANIFEST.tsv is missing" >&2; exit 1; }
fail=0
# -s: an empty file never counts (a failed download or git archive can leave one)
ok(){ [ -s "$S/$1" ] && [ "$2" = - -o "$(sha256sum "$S/$1" | cut -d' ' -f1)" = "$2" ]; }

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
        # (the file store's path keeps the version's epoch, e.g. print-manager/6:6.6.4-0ubuntu1; older uploads took it without)
        curl -fsSL -o "$S/$path" "https://launchpad.net/ubuntu/+archive/primary/+sourcefiles/$src/$ver/$file" ||
          curl -fsSL -o "$S/$path" "https://launchpad.net/ubuntu/+archive/primary/+sourcefiles/$src/${ver#*:}/$file" || true
      fi ;;
    pool|url)  curl -fsSL -o "$S/$path" "$arg" || true ;;
    inner) set -- $arg; tar -xOf "$S/$1" "$2" > "$S/$path" 2>/dev/null || true ;;
    git)
      set -- $arg; repo=$1 tag=$2; name=${file%.tar.*}
      # a good file stays (a backup's copy, or GitHub's own tarball when the manifest has its checksum: its top
      # directory, e.g. SPIRV-LLVM-Translator-21.1.7/, is what the recipe expects); otherwise GitHub's tarball, then git archive
      if ok "$path" "$sum"; then echo "ok    $path"; continue; fi
      [ "$sum" = - ] || { curl -fsSL -o "$S/$path" "$repo/archive/refs/tags/$tag.tar.gz" || true; }
      if ok "$path" "$sum"; then echo "ok    $path"; continue; fi
      d=$S/git/$(basename $repo).git
      # safe.directory: doas runs this as root over repos a user unpacked or cloned (git refuses those otherwise)
      [ -d $d ] || git clone -q --bare "$repo" $d
      git -c safe.directory=$d -C $d fetch -q --tags origin 2>/dev/null
      git -c safe.directory=$d -C $d archive --format=tar.gz --prefix=$name/ "$tag" > "$S/$path.part" \
        && mv "$S/$path.part" "$S/$path" || rm -f "$S/$path.part"
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
