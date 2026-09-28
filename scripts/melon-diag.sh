#!/bin/sh
# melon-diag: collect what's needed to find missing drivers, firmware and services on a machine running melon.
# Run it on the machine (as your normal user; it uses sudo for the kernel log):
#   curl -fsSL https://raw.githubusercontent.com/xbfj/melon-os/main/scripts/melon-diag.sh | sh > ~/melon-diag.txt 2>&1
# and send melon-diag.txt. It contains hardware IDs, driver and service state, no personal files.
sec(){ printf '\n===== %s =====\n' "$*"; }
run(){ printf '$ %s\n' "$*"; sh -c "$*" 2>&1 | head -${LINES_MAX:-80}; }
sec system
run 'cat /etc/os-release | head -3; uname -a'
run 'cat /sys/class/dmi/id/sys_vendor /sys/class/dmi/id/product_name /sys/class/dmi/id/bios_version 2>/dev/null'
run 'melon-detect-virt 2>/dev/null'
run 'cat /proc/cmdline'

sec 'PCI devices (vendor:device, class, driver in use)'
for d in /sys/bus/pci/devices/*; do
  drv=$(basename "$(readlink $d/driver 2>/dev/null)" 2>/dev/null)
  printf '%s %s:%s class=%s driver=%s\n' "${d##*/}" "$(cat $d/vendor)" "$(cat $d/device)" "$(cat $d/class)" "${drv:--NONE-}"
done
sec 'PCI modaliases without a loaded driver'
for d in /sys/bus/pci/devices/*; do [ -e $d/driver ] || cat $d/modalias; done

sec 'USB devices'
for d in /sys/bus/usb/devices/*; do
  [ -f $d/idVendor ] && printf '%s %s:%s %s %s\n' "${d##*/}" "$(cat $d/idVendor)" "$(cat $d/idProduct)" "$(cat $d/manufacturer 2>/dev/null)" "$(cat $d/product 2>/dev/null)"
done
sec 'USB/I2C/ACPI/HID devices without a driver'
for d in /sys/bus/usb/devices/*:* /sys/bus/i2c/devices/* /sys/bus/hid/devices/* /sys/bus/platform/devices/*; do
  [ -e "$d/modalias" ] || continue; [ -e "$d/driver" ] && continue
  printf '%s  %s\n' "${d##*/}" "$(cat $d/modalias)"
done 2>/dev/null | head -60

sec 'kernel modules loaded'
run 'cut -d" " -f1 /proc/modules | sort | tr "\n" " "'
sec 'kernel log: errors, firmware, failures'
LINES_MAX=120 run 'sudo -n dmesg 2>/dev/null || dmesg' | grep -iE 'firmware|error|fail|warn|timeout|not found|missing|unknown|denied|amdgpu|i915|iwl|rtw|ath|mt7|bluetooth|snd|sof|acp|i2c_hid|elan|synaptics|backlight|hp_wmi' | head -150

sec graphics
run 'ls -l /dev/dri; cat /sys/class/drm/card*/device/uevent 2>/dev/null | grep -E "DRIVER|PCI_ID"'
run 'ls /sys/class/backlight; for b in /sys/class/backlight/*; do echo $b $(cat $b/brightness)/$(cat $b/max_brightness); ls -l $b/brightness; done'
run 'echo "XDG_SESSION_TYPE=$XDG_SESSION_TYPE WAYLAND_DISPLAY=$WAYLAND_DISPLAY"'

sec 'network'
run 'ip -br link; ip -br addr'
run 'ls /sys/class/net/*/device/driver -d 2>/dev/null | xargs -r -n1 readlink'
run 'rfkill 2>/dev/null || for r in /sys/class/rfkill/*; do echo $r $(cat $r/type) soft=$(cat $r/soft) hard=$(cat $r/hard); done'
run 'nmcli general status; nmcli device status'
run 'cat /etc/resolv.conf'

sec 'sound'
run 'cat /proc/asound/cards; ls /dev/snd'
run 'wpctl status 2>/dev/null | head -60'
run 'ps -eo user,args | grep -E "pipewire|wireplumber|pulse" | grep -v grep'

sec 'bluetooth'
run 'ls /sys/class/bluetooth; bluetoothctl show 2>/dev/null | head -12'

sec 'input'
run 'cat /proc/bus/input/devices | grep -E "^N:|^H:"'
run 'ls -l /dev/input | head -30'

sec 'power'
run 'ls /sys/class/power_supply; cat /sys/class/power_supply/BAT*/capacity 2>/dev/null; cat /sys/power/mem_sleep'
run 'powerprofilesctl 2>/dev/null | head; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver'

sec 'session and permissions'
run 'id; loginctl list-sessions 2>/dev/null; loginctl show-session $XDG_SESSION_ID 2>/dev/null | grep -E "Type|Seat|Active|Remote"'
run 'ls /run/user/$(id -u) 2>/dev/null | head'

sec 'services'
run 'for s in /var/service/*; do sv status $s 2>&1; done'
run 'ls /var/log/*/current 2>/dev/null | while read f; do echo "--- $f"; tail -5 $f; done'
sec done
