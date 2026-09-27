# Contributing to melon (instructions for agents)

This file is for AI agents (and humans) who work on melon. Read it fully before changing anything.
`CLAUDE.md` points here.

## What melon is

A from-scratch, rolling, x86_64 Linux distribution. Every package is cross-compiled from upstream
source by our own scripts and shipped as a signed apk v3 package.

| Piece | Choice | Notes |
|---|---|---|
| libc | musl 1.2.5 | + CVE-2025-26519 iconv patches |
| toolchain | GCC 15.2, binutils 2.46 | target triple `x86_64-melon-linux-musl` |
| userland | BusyBox 1.37 | applet symlinks created by its post-install script |
| shell | bash 5.3 (login shell), BusyBox ash is `/bin/sh` | |
| init | runit 2.3 | stages in `/etc/runit/{1,2,3}`, services in `/etc/sv`, enabled = symlink in `/var/service` |
| devices | BusyBox mdev (`mdev -d` service) | no udev yet, stage 2 will need one |
| packages | apk-tools 3.0.8 | repo index `Packages.adb`, signed with `keys/melon-signing.rsa` |
| kernel | Linux 7.0, `linux-melon` (generic) | config = `x86_64_defconfig` + `recipes/linux-melon/config-melon` |
| boot | GRUB 2.14, both `i386-pc` and `x86_64-efi` | ISO and installs boot on BIOS **and** UEFI |
| disks | GPT: 1 MiB BIOS boot, 1 GiB FAT32 `/boot` (also the ESP), XFS `/` | kernel boots with `root=PARTUUID=...`, no initramfs on installed systems |
| filesystem | merged `/usr`: `/bin`, `/sbin`, `/usr/sbin` -> `usr/bin`, `/lib` -> `usr/lib` | packages must only ship files under `/usr`, `/etc`, `/var`, `/boot` |
| desktop | KDE Plasma on Wayland (stage 2, not built yet) | |
| gaming | Flatpak + Flathub Steam (stage 3, not built yet) | Steam is glibc-only, so it can't run natively on musl |

Owner's config (`CONFIG_*` answers) lives in `docs/config.txt`. Don't change those choices without the owner's approval.

## Repository layout

```
scripts/env.sh          paths: M, TARGET, TOOLS, SYSROOT, SRC, WORK, REPO
scripts/toolchain.sh    cross toolchain, part 1 (binutils, headers, gcc stage 1)
scripts/toolchain-finish.sh  part 2 (musl, full gcc)
scripts/melon-build     build ONE recipe -> signed .apk(s) in repo/x86_64, reindex, install into sysroot
scripts/build-all.sh    build a list of recipes in order, logs to logs/pkg-<name>.log
scripts/mkiso.sh        live/installer ISO from the repo
scripts/qemu-test.py    headless boot + install + reboot test over the serial console
recipes/<name>/MELONBUILD   one directory per recipe (see below)
recipes/<name>/*.patch      applied automatically with patch -p1, in name order
recipes/<name>/<pkg>.post-install etc.   apk scripts for (sub)package <pkg>
iso-files/init          live initramfs /init
branding/               logo (transparent PNGs)
recipes/calamares-melon/    graphical installer branding + gauntlet (stage 2)
```

Not in git (see `.gitignore`): `sources/`, `work/`, `tools/`, `sysroot/`, `hosttools/`, `repo/`, `out/`,
`logs/`, and **the private signing key**. Never commit `keys/*.rsa`. Only `keys/*.rsa.pub` is committed.

## Recipe format (MELONBUILD)

A bash fragment sourced by `scripts/melon-build`. Minimal example:

```bash
pkgname=zlib
pkgver=1.3.1
pkgrel=0                     # bump when you change the recipe but not the version
pkgdesc="A compression/decompression library"
license=Zlib
url=https://zlib.net
source=(zlib-1.3.1.tar.gz)   # file names inside $SRC (sources/), or files in the recipe dir
subpackages=(zlib-dev)       # -dev and -doc get default split functions
makedepends=()               # packages installed into the sysroot before building
depends=()                   # runtime deps; so: deps are added automatically from ELF NEEDED
build(){ ./configure --prefix=/usr --libdir=/usr/lib --shared >/dev/null; make -j$JOBS >/dev/null; }
package(){ make DESTDIR=$pkgdir install >/dev/null; }
```

Things the builder gives you:
- `$CC`, `$CXX`, etc. already point at the cross toolchain. `$conf_flags` holds the standard autoconf flags
  (`--host`, `--prefix=/usr`, `--sbindir=/usr/bin`, ...). `meson_setup` wraps meson with our cross file.
- `$srcdir`, `$builddir` (default `$srcdir/$pkgname-$pkgver`), `$pkgdir`, `$startdir` (recipe dir), `$subpkgdir`.
- Split functions: `pkg_<suffix>()` for subpackage `<pkgname>-<suffix>`, or `pkg_<name>()` for a
  differently named subpackage. Non-alphanumerics become `_` (e.g. `libstdc++-dev` -> `pkg_libstdc___dev`).
  Use `amove path...` (globs allowed, relative to `$pkgdir`) to move files into the subpackage.
- Per-subpackage variables use the same mangling: `depends_<x>`, `pkgdesc_<x>`, `provides_<x>` (array).
- `options=('!strip')` skips stripping. `options=(keepdirs)` keeps empty directories.
- After packaging, files in `/bin`, `/sbin`, `/lib`, `/usr/sbin` are folded into `/usr/bin` and `/usr/lib`,
  `.la` files are deleted, and ELF files are stripped.
- `so:` provides/depends are generated from SONAME/NEEDED. musl's `libc.so` has no SONAME, so the musl
  recipe declares `so:libc.so` by hand. Keep it that way.

Build one package: `scripts/melon-build <recipe>`. Build several: `scripts/build-all.sh a b c`.
Rebuilding the kernel takes about an hour on 2 cores.

## Hard-won rules (each one broke the build once)

1. **Never run build steps as `yes | cmd` under `set -o pipefail`.** `yes` dies with SIGPIPE and the recipe
   aborts silently. Use `yes "" | cmd || [ -f expected-output ]`.
2. **Host tools must not see the sysroot.** Anything built with the host compiler (kernel `scripts/`,
   `certs/extract-cert`, GRUB's firmware code) must run with `PKG_CONFIG_SYSROOT_DIR`, `PKG_CONFIG_LIBDIR`,
   `CFLAGS` and `LDFLAGS` unset (see `_kmake` in the kernel recipe and `_tgt` in the GRUB recipe).
   Otherwise it links against musl's libcrypto and fails with "invalid ELF header".
3. **GRUB firmware code gets `TARGET_CFLAGS=-Os` and empty `CFLAGS/LDFLAGS`.** Our `-fno-plt` produces GOT
   references that make `moddep.lst` fail.
4. **ncurses is built without a separate libtinfo.** `libtinfo.so` is a symlink to `libncursesw.so`.
   A split tinfo breaks readline and bash with `--as-needed`.
5. **musl has no `sys/cdefs.h`.** Add `bsd-compat-headers` to `makedepends` for software that includes it.
6. **The sysroot upgrades with `apk add --upgrade`.** Bump `pkgrel` whenever you change a recipe, or
   the change won't reach the sysroot or users.
7. **Install scripts must not create config files that another package ships.** apk would then keep the
   script's version and write the package's as `*.apk-new`. See `recipes/bash/bash.post-install`.
8. **GCC's target libraries install to `tools/x86_64-melon-linux-musl/lib64`**, not `lib`.
9. Anything that runs cross-built binaries on the build host needs `/lib/ld-musl-x86_64.so.1`
   (a symlink to `sysroot/usr/lib/libc.so`).

## Testing (required before you commit a change that affects boot or install)

```sh
scripts/mkiso.sh
qemu-img create -f raw /tmp/disk.img 12G
scripts/qemu-test.py live out/melon-*-x86_64.iso /tmp/disk.img            # BIOS: boot ISO + unattended install
scripts/qemu-test.py disk /tmp/disk.img                                   # BIOS: boot the installed system
scripts/qemu-test.py disk /tmp/disk.img --uefi                            # UEFI: same disk
```

Logs go to `logs/qemu-*.log`. There's no KVM in the usual build container, so QEMU runs in software
emulation and everything is slow. Use generous timeouts. The ISO's GRUB and the installed system both use
the serial port, so tests don't need a screen. The test also records the sound card output to
`logs/audio-capture.wav`, which lets you check that the installer music really plays.

## Installers: do not break these product decisions

- **Graphical installer (desktop ISO): Calamares with the gauntlet.** About 200 very easy questions, one
  per screen. The Next button waits a few seconds. A wrong answer sends you back 10 questions, never out
  of the installer. Questions are in `recipes/calamares-melon/gauntlet/questions.js`. It's deliberately
  slow, to put off people who are only there for status. Keep it that way.
- **Console installer: quick and deliberately hidden.** It lives at `/usr/libexec/melon/.cold`. It is started
  by typing its name in bash, which a `command_not_found_handle` in `/etc/profile.d/zz-melon.sh`
  recognises **by hash**. The owner decided the name must not be discoverable. Therefore:
  - do not write the command's name in any file in this repo (docs, comments, commit messages, package
    names, help text, motd, man pages, shell completion, test scripts);
  - do not add a file or symlink with that name to `PATH`;
  - if you need to run it in a test, call `/usr/libexec/melon/.cold` directly.
  It asks base or desktop and installs exactly what the desktop ISO installs. It plays
  `/usr/share/melon/.ice` (from the `melon-sounds` package, live ISO only) at 30% volume while it runs.
- Unattended install variables: `MELON_DISK MELON_HOSTNAME MELON_ROOTPW MELON_USER MELON_USERPW
  MELON_PROFILE MELON_YES=1 MELON_SERIAL=1 MELON_WIFI_SSID MELON_WIFI_PSK`.

## Sources

The original build container can't reach kernel.org, gnu.org or most upstream sites. It can reach the
Ubuntu archive and public GitHub. Upstream tarballs were therefore taken from the Ubuntu 26.04 source
archive (`apt-get source --download-only <pkg>`, then the `*.orig.tar.*`, symlinked into `sources/` under
its upstream name) or from GitHub (apk-tools). Firmware blobs come from Ubuntu's `linux-firmware-*`
.debs. If you have normal internet access, fetching the same versions from upstream is fine, as long as
the tarballs are identical.

## Roadmap

- **Stage 1 (base):** toolchain, base packages, live ISO, installers. In progress. See the task list in the PR/issue.
- **Tuned ProBook kernel:** boot the live ISO on the HP ProBook 445 G8 (Ryzen 7 5800U), run
  `melon-hwprofile`, then build a `linux-melon-probook` flavour from that module list.
- **Stage 2 (desktop):** udev replacement (eudev or libudev-zero), dbus, elogind or seatd, Mesa with LLVM
  (radeonsi for the Ryzen iGPU), Qt 6, KDE Frameworks 6, Plasma, KWin (Wayland), SDDM, PipeWire,
  NetworkManager, Calamares with melon branding and the gauntlet, and the desktop profile for both installers.
- **Stage 3 (gaming):** Flatpak and dependencies, Flathub remote, Steam through Flatpak, gamepad udev rules.

## Git conventions

- Small, focused commits. The message says what changed and why.
- Build outputs and downloaded sources are never committed.
- If you change a recipe, bump `pkgrel` (or `pkgver`) in the same commit.
