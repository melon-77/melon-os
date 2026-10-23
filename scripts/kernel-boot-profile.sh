#!/bin/sh
# kernel-boot-profile.sh: where the kernel's own boot time goes, from the running system's log. Run it as root on a melon
# system that was started with `initcall_debug` added to the kernel command line (GRUB's editor: press e, add it to the
# linux line, boot with Ctrl-X). Prints the 25 slowest initcalls and the time at which the kernel started /init.
# Use it to decide what to change in recipes/linux-melon/config-melon: a driver that costs 300 ms and is rarely needed can
# become a module; one the boot depends on stays built in. Compare before and after a kernel change with melon-boottime.
dmesg | awk '
  /initcall .* returned .* after [0-9]+ usecs/ {
    for (i = 1; i <= NF; i++) if ($i == "after") us = $(i + 1)
    n = $0; sub(/.*initcall /, "", n); sub(/ returned.*/, "", n)
    printf "%9d us  %s\n", us, n
  }' | sort -rn | head -25
echo
dmesg | grep -m1 -E 'Run /[a-z/]+ as init process' | sed 's/^/kernel handed over to init: /'
