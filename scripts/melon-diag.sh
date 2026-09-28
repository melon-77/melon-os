#!/bin/sh
# melon-diag: the hardware and service report for problem reports now lives in melon-hwreport (installed on every
# melon system: run "melon-hwreport" or "sudo melon-hwreport"). This wrapper stays because older instructions
# point at its URL:
#   curl -fsSL https://raw.githubusercontent.com/melon-77/melon-os/main/scripts/melon-diag.sh | sh > ~/melon-diag.txt 2>&1
# It prints the report from the newest melon-hwreport, or the installed one when it can't download it.
url=https://raw.githubusercontent.com/melon-77/melon-os/main/recipes/melon-base/files/usr/bin/melon-hwreport
echo "(melon-diag is now melon-hwreport; next time run: sudo melon-hwreport)"
if code=$(curl -fsSL "$url" 2>/dev/null) && [ -n "$code" ]; then
  sh -c "$code" melon-hwreport - </dev/null
elif command -v melon-hwreport >/dev/null 2>&1; then
  melon-hwreport - </dev/null
else
  echo "melon-diag: couldn't download $url and melon-hwreport isn't installed" >&2; exit 1
fi
