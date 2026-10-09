#!/usr/bin/env bash
# Upload release ISOs to a file host and write their SHA256SUMS. See docs/iso-hosting.md for what each host needs.
#
#   scripts/upload-isos.sh [--dry-run] HOST RELEASE FILE...
#
#   HOST     sourceforge | archive | r2
#   RELEASE  the release's name for the host, e.g. 0.2-bailan (a folder on SourceForge, the item name on the Internet Archive,
#            a prefix in the R2 bucket)
#   FILE     the ISOs (their SHA256SUMS is written next to the first one and uploaded too)
#
# Nothing secret lives in this repo; every host takes its login from the environment or from its own tool's config:
#   sourceforge  SF_USER, SF_PROJECT          (an ssh key added to your SourceForge account; rsync over ssh to frs.sourceforge.net)
#   archive      `ia configure` once          (the internet archive's command line tool, `ia`)
#   r2           R2_ACCOUNT_ID, R2_BUCKET, AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY   (an R2 API token; needs the `aws` CLI)
# --dry-run prints what would run and uploads nothing.
set -euo pipefail

dry=0
[ "${1:-}" = --dry-run ] && { dry=1; shift; }
[ $# -ge 3 ] || { sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
host=$1 release=$2; shift 2
for f in "$@"; do [ -f "$f" ] || { echo "no such file: $f" >&2; exit 1; }; done
case $release in *[!A-Za-z0-9._-]*|"") echo "RELEASE may only use letters, digits, . _ -" >&2; exit 1;; esac

run(){ if [ $dry = 1 ]; then printf '+ %s\n' "$*"; else "$@"; fi; }
need(){ [ $dry = 1 ] || command -v "$1" >/dev/null || { echo "$1 is not installed" >&2; exit 1; }; }
var(){ [ $dry = 1 ] || [ -n "${!1:-}" ] || { echo "set $1 (see docs/iso-hosting.md)" >&2; exit 1; }; }

# SHA256SUMS: one line per file, names only, so `sha256sum -c` works next to the downloads
dir=$(dirname "$1"); sums=$dir/SHA256SUMS
( for f in "$@"; do ( cd "$(dirname "$f")" && sha256sum "$(basename "$f")" ); done ) > "$sums"
cat "$sums"

case $host in
  sourceforge)
    var SF_USER; var SF_PROJECT; need rsync
    dest="${SF_USER:-USER}@frs.sourceforge.net:/home/frs/project/${SF_PROJECT:-PROJECT}/$release/"
    run rsync -avP --partial -e ssh "$@" "$sums" "$dest"
    echo "download page: https://sourceforge.net/projects/${SF_PROJECT:-PROJECT}/files/$release/"
    ;;
  archive)
    need ia
    item=melon-linux-$release
    run ia upload "$item" "$@" "$sums" --metadata="mediatype:software" --metadata="title:melon Linux $release" \
      --metadata="subject:linux;distribution;iso" --metadata="description:melon Linux $release: live and installer ISOs with SHA256SUMS. https://github.com/melon-77/melon-os"
    echo "download page: https://archive.org/details/$item (torrent: https://archive.org/download/$item/${item}_archive.torrent)"
    ;;
  r2)
    var R2_ACCOUNT_ID; var R2_BUCKET; need aws
    ep="https://${R2_ACCOUNT_ID:-ACCOUNT}.r2.cloudflarestorage.com"
    for f in "$@" "$sums"; do
      run aws s3 cp --endpoint-url "$ep" "$f" "s3://${R2_BUCKET:-BUCKET}/$release/$(basename "$f")"
    done
    echo "files are at <your public bucket or custom domain>/$release/"
    ;;
  *) echo "HOST must be sourceforge, archive or r2" >&2; exit 2;;
esac
