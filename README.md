# melon Linux

> **Contributing: open pull requests against the `testing` branch only, never `main`.**
> `main` is the stable branch that ISOs and the package repository are built from. Changes reach it only
> when the owner has personally reviewed and tested them on `testing` and decides to bring them over.
> Pull requests against `main` will be closed.

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
| desktop | KDE Plasma 6 on Wayland; Flatpak for Steam and other glibc apps; more desktops as online installs |

## Layout

```
scripts/toolchain.sh   builds the x86_64-melon-linux-musl cross toolchain (binutils 2.46, GCC 15.2, musl)
scripts/melon-build    builds one recipe into signed .apk packages and reindexes the repo
scripts/mkiso.sh       assembles the hybrid BIOS/UEFI live ISO from the repo
recipes/<pkg>/MELONBUILD   package recipes (APKBUILD-like)
iso-files/init         live initramfs init
branding/              logo
site/                  the website (published to the gh-pages branch by scripts/publish-site.sh)
art/site.py            draws the website's pixel melon and netting
recipes/calamares-melon/   graphical installer branding and the 200-question gauntlet
```

## Building

Sources come from upstream releases (`scripts/fetch-sources.sh`, into `sources/`; older recipes still use the Ubuntu
source archive's copies of the same tarballs, and new ones come straight from upstream). Then:

```sh
scripts/toolchain.sh
for p in melon-layout linux-headers musl gcc-runtime zlib zstd openssl apk-tools busybox ncurses bash runit \
         util-linux userspace-rcu inih xfsprogs grub libnl3 wpa_supplicant alsa-lib alsa-utils mpg123 \
         linux-firmware linux-melon melon-base melon-sounds; do scripts/melon-build $p; done
scripts/mkiso.sh
```

The build machine can be Ubuntu 24.04 (or WSL2), Debian or Devuan, or **melon itself**: `scripts/host-setup.sh`
notices which one it runs on and installs the build tools with `apt` or with melon's own `apk`. `BUILDING.md` has
them all, including what to keep and what to rebuild when moving a build machine.

Packages are signed with `keys/melon-signing.rsa` (not committed; generate your own with
`openssl genrsa -out keys/melon-signing.rsa 4096` and put the public half in `keys/`).

## Installing

Boot the ISO. The graphical installer (desktop ISO) is Calamares. There is also a quick console
installer for people who know its name.

## Website

[melon-77.github.io/melon-os](https://melon-77.github.io/melon-os/) is the `site/` folder, served from the `gh-pages` branch.
It is plain HTML and CSS (fonts hosted in the folder, no trackers) plus a small demo of the installer's gauntlet. The big
melon in the first screen is `melonfetch`'s own, redrawn by `art/site.py`. The site also hosts the
[distro finder](https://melon-77.github.io/melon-os/distro-finder/), a quiz that scores more than a hundred Linux
distros, melon included, against your answers. Change the site through a pull request to `testing` like everything else;
after it is merged, `scripts/publish-site.sh` puts `site/` on `gh-pages`.

## Developer tools

Compilers and build tools come from the package repository (they're not on the ISOs), for example:

```sh
sudo apk add gcc g++ make pkgconf      # also offered on first login
sudo apk add clang go cmake meson git github-cli
sudo apk add qemu ovmf                  # virtual machines with KVM, BIOS and UEFI
```

**Claude Code** runs on melon. Anthropic's installer recognises musl and installs its musl build for your user:

```sh
sudo apk add bash curl ca-certificates libgcc libstdc++ ripgrep
curl -fsSL https://claude.ai/install.sh | bash
echo 'export USE_BUILTIN_RIPGREP=0' >> ~/.bashrc    # Claude Code then searches with melon's ripgrep
```

## Boot time

An installed system that has no other operating system next to it skips its boot menu after 1 second (hold Shift or press Esc
to open it; "safe graphics" is in there; `GRUB_TIMEOUT` and `GRUB_TIMEOUT_STYLE` in `/etc/default/grub` change this, and
`sudo melon-update-grub` applies it). Next to Windows the menu still shows for 5 seconds. `melon-boottime` shows where a boot's
time went: the milestones of stage 1 and when the main services started.

**NVIDIA graphics:** on a computer with an NVIDIA card (GTX 16/RTX 20 and newer), `melon-first-boot` offers the open
driver (`sudo apk add linux-firmware-nvidia mesa-nvk`: nouveau with NVK, the desktop and games use the card) or NVIDIA's
own kernel driver (`sudo apk add nvidia-open`: Flatpak games get NVIDIA's libraries from Flathub; the desktop can't use
them, as they need glibc). Restart after installing either.
