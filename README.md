# melon Linux

A from-scratch, rolling x86_64 distribution:

| | |
|---|---|
| libc | musl 1.2.5 |
| userland | BusyBox 1.37 |
| init | runit 2.3 |
| packages | apk-tools 3 (signed apk v3 packages, binary + source recipes) |
| kernel | Linux 7.0, generic flavour |
| boot | GRUB 2.14, one ISO and one install that boot on both BIOS and UEFI |
| root fs | XFS (FAT32 `/boot`) |
| shell | bash 5.3 |
| desktop | KDE Plasma on Wayland (stage 2), Flatpak for Steam and games (stage 3) |

## Layout

```
scripts/toolchain.sh   builds the x86_64-melon-linux-musl cross toolchain (binutils 2.46, GCC 15.2, musl)
scripts/melon-build    builds one recipe into signed .apk packages and reindexes the repo
scripts/mkiso.sh       assembles the hybrid BIOS/UEFI live ISO from the repo
recipes/<pkg>/MELONBUILD   package recipes (APKBUILD-like)
iso-files/init         live initramfs init
branding/              logo
recipes/calamares-melon/   graphical installer branding and the 200-question gauntlet
```

## Building

Sources come from upstream release tarballs (fetched via the Ubuntu source archive in the original build
environment) into `sources/`. Then:

```sh
scripts/toolchain.sh
for p in melon-layout linux-headers musl gcc-runtime zlib zstd openssl apk-tools busybox ncurses bash runit \
         util-linux userspace-rcu inih xfsprogs grub libnl3 wpa_supplicant alsa-lib alsa-utils mpg123 \
         linux-firmware linux-melon melon-base melon-sounds; do scripts/melon-build $p; done
scripts/mkiso.sh
```

Packages are signed with `keys/melon-signing.rsa` (not committed; generate your own with
`openssl genrsa -out keys/melon-signing.rsa 4096` and put the public half in `keys/`).

## Installing

Boot the ISO. The graphical installer (desktop ISO) is Calamares. There is also a quick console
installer for people who know its name.
