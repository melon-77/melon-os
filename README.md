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
| 32-bit edition | i686 (Pentium M, Pentium 4, Atom netbooks such as the MSI Wind U100) with LXQt on Wayland (labwc) |

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

## 32-bit edition (old netbooks)

melon also builds for 32-bit PCs with SSE2 (Pentium M, Pentium 4, Intel Atom: the MSI Wind U100 and similar netbooks).
Its desktop is **LXQt** on Wayland with the labwc compositor, lighter than Plasma and comfortable in 1–2 GB of RAM,
with zram swap and drivers for every graphics and network chip an old 32-bit netbook or laptop is likely to have: Intel GMA, old Radeon (r300, r600) and GeForce (nouveau) graphics; Atheros, Ralink, Realtek, Intel 3945/4965 and Broadcom Wi-Fi, USB Wi-Fi sticks and the usual wired chips; the MSI Wind's Fn keys and radio switch (msi-laptop). It
uses the same installers (Calamares with the gauntlet, the console installer). Flatpak and the NVIDIA drivers are
64-bit only. Build it with `MELON_ARCH=x86` (`BUILDING.md`); the ISOs are `melon-*-i686.iso` and
`melon-desktop-*-i686.iso`. Status (7 October 2026): both ISOs are built, signed with melon's key and pass their install tests on an emulated
Atom N270 with 2 GB of RAM (LXQt logs in through SDDM, printing works); the 32-bit packages are in the online
repository (`packages/x86`); the ISOs are on the pre-release `i686-20261007`; a test on a real U100 comes next. NetHack and
the QEMU guest agent aren't in the 32-bit edition yet.

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

**NVIDIA graphics:** on a computer with an NVIDIA card (GTX 16/RTX 20 and newer), `melon-first-boot` offers the open
driver (`sudo apk add linux-firmware-nvidia mesa-nvk`: nouveau with NVK, the desktop and games use the card) or NVIDIA's
own kernel driver (`sudo apk add nvidia-open`: Flatpak games get NVIDIA's libraries from Flathub; the desktop can't use
them, as they need glibc). Restart after installing either.
