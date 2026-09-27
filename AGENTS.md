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
| privileges | doas (OpenDoas 6.8.2) with a `sudo` command on top | members of `wheel` |
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

To build on another machine (Ubuntu 24.04 or WSL2), follow `BUILDING.md`: `scripts/host-setup.sh`, then
`scripts/build-everything.sh`. Scripts find the repo from their own location; recipes use `$M`. Never write
a machine's absolute path into a script or recipe.

## Repository layout

```
scripts/env.sh          paths: M, TARGET, TOOLS, SYSROOT, SRC, WORK, REPO
scripts/toolchain.sh    cross toolchain, part 1 (binutils, headers, gcc stage 1)
scripts/toolchain-finish.sh  part 2 (musl, full gcc)
scripts/melon-build     build ONE recipe -> signed .apk(s) in repo/x86_64, reindex, install into sysroot
scripts/build-all.sh    build a list of recipes in order, logs to logs/pkg-<name>.log
scripts/mkiso.sh        live/installer ISO: rootfs.sqfs (pristine apk-installed system, copied to disk by the
                        installers) + live.sqfs (small live-session layer) + a repo of extra packages
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
10. **Module handling uses kmod, not BusyBox.** BusyBox's `depmod -b` produced a modules.dep that
    `modprobe` couldn't resolve, so no drivers loaded. The kernel package runs `depmod -a` (kmod) in its
    post-install, and `kmod` is a dependency of `linux-melon`.
11. **The build host needs `zstd`** (the kernel compresses bzImage with it), `mtools`, `xorriso` and
    `squashfs-tools`.
12. **GRUB on a serial console:** `terminal_output gfxterm` replaces the serial output unless `serial`
    is listed too. `melon-update-grub` handles this when `GRUB_SERIAL=1` is set in `/etc/default/grub`.
13. **`melon-update-grub` is given the root and boot devices explicitly** by the installer
    (`MELON_ROOT_DEV`, `MELON_BOOT_DEV`). Guessing them from inside a chroot picked the wrong filesystem UUID.

14. **Services never write to the console.** Every `/etc/sv/<svc>/run` starts with `exec 2>&1`, and
    anything chatty has a `log/run` that pipes it to `svlogd` in `/var/log/<svc>/`. A crash-looping
    service that prints to the console floods the screen (this happened with wpa_supplicant).
15. **Only pass options the binary was built with.** wpa_supplicant's `-s` needs `CONFIG_DEBUG_SYSLOG=y`.
    Without it, every start printed the help text.
16. **Shutdown unmounts `/boot` explicitly** (see `/etc/runit/3`). BusyBox's `umount -a -t no...` left the
    FAT `/boot` dirty.
17. **`sudo` is a small front end for doas** (`recipes/opendoas/sudo`). Rules live in `/etc/doas.conf`
    (`permit persist :wheel`). New users get their shell setup from `/etc/skel`.
18. **melon binaries run on the build host.** `/lib/ld-musl-x86_64.so.1` points at the sysroot's `libc.so`
    and `/etc/ld-musl-x86_64.path` lists `sysroot/usr/lib` (same for `i386` in 32-bit builds), so
    build-time generators from earlier packages (glib-compile-resources, kconfig_compiler, ...) just run.
    Meson's cross file sets `needs_exe_wrapper=false` for the same reason.
19. **`meson_setup` drops `-D` options the project doesn't have** (`scripts/meson-filter-opts.py`) and
    unsets the `PKG_CONFIG_*` variables for native lookups. Upstream renames options often; a stale
    option must not fail the whole build.
20. **Some generators must come from the host, not the sysroot:** `wayland-scanner` 1.24 in
    `/usr/local`, Mesa's `mesa_clc`/`vtn_bindgen2` in `hosttools/bin` (built against host LLVM), and the
    Qt 6 host tools in `hosttools/qt6` (`scripts/host-qt.sh`, same Qt version as the target, passed as
    `QT_HOST_PATH`). The host also needs `libltdl-dev` (libffi's autoreconf).
21. **Never kill build processes with `pkill -f <pattern>`** when your own shell's command line contains
    the pattern: it kills your shell too. Find the PID and kill that.
22. **Edit scripts that may be running (`melon-build`, `mkiso.sh`, queue scripts) through a temporary file
    and `mv`.** bash reads scripts while running them, so an in-place edit corrupts a running build.
23. **Never type keys or checksums from memory.** The Flathub key is downloaded from Flathub's own
    `.flatpakrepo` at first use; Alpine's public keys in `melon-base` were fetched from Alpine's aports
    and checked against the sha512sums in Alpine's `alpine-keys` APKBUILD.
24. **musl ships `libc.musl-<arch>.so.1`** (a symlink to the loader), the library name Alpine binaries link
    against. That's what lets opt-in `@alpine` packages run on melon.
25. **Cross autoconf answers live in `scripts/config.site`** (`CONFIG_SITE`). Without them `AC_FUNC_MALLOC`
    assumes a broken malloc and the library ends up with undefined `rpl_malloc` (libndp).
26. **`msgfmt` needs `GETTEXTDATADIRS`** pointing at the sysroot's `/usr/share/gettext` to find the ITS
    rules (polkit policies, GSettings schemas) that earlier melon packages installed (upower).
27. **Host packages the build needs** besides the toolchain: `tcl` (sqlite's amalgamation), `hwdata`
    (libdisplay-info reads `pnp.ids` at build time), `publicsuffix` (libpsl's built-in list), `autopoint`
    (cryptsetup's autoreconf), `libltdl-dev`, `libxml2-utils` (shared-mime-info runs `xmllint`).
28. **Only encrypted installs have an initramfs.** `melon-mkinitramfs` builds it when
    `/etc/melon/encrypted-root` exists; `melon-update-grub` then writes `cryptroot=UUID=<luks>
    root=/dev/mapper/melonroot` and an `initrd` line. Everything else still boots straight from the kernel.
30. **musl 1.2.4+ hides the `*64` LFS types** (`off64_t`, `ino64_t`) unless `_LARGEFILE64_SOURCE` is
    defined. Add `-D_LARGEFILE64_SOURCE` to the recipe's CFLAGS (gpgme), as Alpine does.
31. **The builder's `apk add --upgrade` only reinstalls a package whose version-release changed.** Generated
    recipes take their `pkgrel` from the `PKGREL` table in `scripts/gen-simple-recipes.py`.
29. **Two build queues may run at once** for the same arch: `melon-build` takes `flock repo/<arch>/.lock`
    around sysroot installs and reindexing, and writes packages under a temporary name first.

To resume a failed long build without unpacking again (for example the kernel):
`MELON_KEEP_SRC=1 scripts/melon-build linux-melon`.

The build container can be reclaimed while idle, which kills background builds. `scripts/resume.sh`
restarts the host Qt build and the Qt/KF6/Plasma queue (`scripts/queue-4.sh`); finished host Qt modules
and packages already in the repo (`MELON_SKIP_BUILT=1`) are skipped.

After an unclean shutdown (WSL restart, power loss), files written in the last minutes can be empty or
truncated. Before resuming: look for empty packages (`find repo -name '*.apk' -size 0`; the builder skips
nothing that is empty, but a truncated `.apk` still blocks reindexing), delete `work/pkg/<name>/.prepared`
of the package that was compiling so it unpacks fresh, and check `git fsck` (empty objects in `.git/objects`
can be restored with `git fetch` once they are moved aside). `apk verify` needs an absolute `--keys-dir`:
a relative one is resolved against `/` and reports every package as UNTRUSTED.

## Testing (required before you commit a change that affects boot or install)

```sh
scripts/mkiso.sh
qemu-img create -f raw /tmp/disk.img 12G
scripts/qemu-test.py live out/melon-*-x86_64.iso /tmp/disk.img            # BIOS: boot ISO + unattended install
scripts/qemu-test.py disk /tmp/disk.img                                   # BIOS: boot the installed system
scripts/qemu-test.py disk /tmp/disk.img --uefi                            # UEFI: same disk
scripts/qemu-test.py live out/melon-*-x86_64.iso /tmp/disk.img --luks     # install with an encrypted root
scripts/qemu-test.py disk /tmp/disk.img --luks                            # boot it, typing the passphrase
MELON_EDITION=desktop scripts/mkiso.sh                                    # the Plasma live ISO (desktop edition)
```

Logs go to `logs/qemu-*.log`. There's no KVM in the usual build container, so QEMU runs in software
emulation and everything is slow. Use generous timeouts. The ISO's GRUB and the installed system both use
the serial port, so tests don't need a screen. The test also records the sound card output to
`logs/audio-capture.wav`, which lets you check that the installer music really plays.

## Running melon in a VM

- **Kernel:** the virtual disk controllers are built in (virtio, VMware PVSCSI and LSI, Hyper-V storvsc,
  Xen blkfront, AHCI), because installed systems boot without an initramfs. Network, video, balloon,
  vsock and VirtualBox drivers are modules (`recipes/linux-melon/config-melon`, "virtual machine guest").
- **Guest tools:** `melon-vm-guest` pulls in `qemu-guest-agent`, `open-vm-tools` and `hvtools`. Their runit
  services are always enabled but switch themselves off (`sv down "$PWD"`) outside their hypervisor.
  mdev doesn't create `/dev/virtio-ports/*` links, so `qemu-ga` finds its port through sysfs.
- **Installers** add `melon-vm-guest` when `melon-detect-virt` says they run in a VM. The console ISO carries
  it in its package repo; the desktop image includes it.
- **vmwgfx is blacklisted except on real VMware** (`/etc/modprobe.d/melon-vm.conf`, loaded from `/etc/runit/1`
  when `melon-detect-virt` says `vmware`). VirtualBox's default VMSVGA adapter and QEMU's vmware-svga look
  like VMware's; vmwgfx switched off the boot console there and then failed, so the screen froze at
  "Starting devices" (reported from VirtualBox). GRUB passes a framebuffer (`gfxpayload`) so simpledrm
  keeps the console alive without any GPU driver. Check the screen, not just the serial log:
  boot with `-vga vmware -serial none` and take a `screendump` from the QEMU monitor.
- **Not included (yet):** VirtualBox's userspace (VBoxClient/VBoxService: clipboard, drag and drop) and
  VMware's X11 helper. Shared folders on VirtualBox work with `mount -t vboxsf`.
- **Test:** `scripts/qemu-test.py live|disk ... --vmware` uses PVSCSI + VMXNET3 + VMware SVGA; every test
  also checks the QEMU guest agent and ends with a shutdown requested by the host through it.

## Installers: do not break these product decisions

- **Graphical installer (desktop ISO): Calamares with the gauntlet.** About 200 very easy questions, one
  per screen. The Next button waits a few seconds. A wrong answer sends you back 10 questions, never out
  of the installer. Questions are in `recipes/calamares-melon/modules/gauntlet/questions.js`; the page is
  a small Calamares view module (`GauntletViewStep`, `gauntlet.qml`) that keeps Calamares' Next button
  locked until the last question. It's deliberately slow, to put off people who are only there for
  status. Keep it that way. Survivors get the `melon-survivor` wallpaper (only installed by Calamares),
  an SVG certificate in `~/Pictures` and a melonfetch badge (`/etc/melon/gauntlet-survivor`).
- **Calamares runs melon's own jobs** (`cal-prepare`, `cal-finish` in `calamares-melon`): Calamares'
  users job calls shadow's `useradd`/`usermod`/`groupadd`, which melon doesn't have, so `cal-prepare`
  puts BusyBox-backed shims into the target's `/usr/local/bin` and `cal-finish` removes them.
- **Live-only packages** (`/usr/share/melon/profiles/live-only`, e.g. `calamares-melon`) are in the
  desktop ISO's system image and both installers remove them from the new system. Both installers make
  the new system's `/etc/apk/world` match the chosen profile.
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
  MELON_PROFILE MELON_YES=1 MELON_SERIAL=1 MELON_WIFI_SSID MELON_WIFI_PSK MELON_ALPINE=y
  MELON_ENCRYPT=y MELON_LUKSPW`. With `MELON_YES=1`, questions that have a default take it.
- **Profiles** live in `/usr/share/melon/profiles/` on the live system: `<name>` is the package list,
  `<name>.services` the runit services (`name` enables one, `-name` drops a base service). `mkiso.sh`
  writes them. The desktop profile swaps `mdevd`/`dhcp` for `udevd` and NetworkManager, and the
  installer gives NetworkManager the Wi-Fi network instead of wpa_supplicant.
- **Other distros' repos are opt-in only.** Both installers offer Alpine as the tagged repo `@alpine`
  (`melon-repo enable alpine`); apk only uses it for packages asked for as `name@alpine`. Void isn't
  offered (xbps, not apk). Never make a foreign repo untagged or on by default.

## Package repository (online)

`scripts/publish-repo.sh` puts `repo/<arch>/` on the `packages` branch of github.com/xbfj/melon-os (one
commit, force-pushed each time), served as
`https://raw.githubusercontent.com/xbfj/melon-os/packages/<arch>/Packages.adb`. The base URL is in
`/usr/share/melon/repo-url` (melon-base); the installers write it into `/etc/apk/repositories` before the
offline copy from the ISO, and the live ISO uses it too. Everything is signed with the melon key, so the
host doesn't need to be trusted. GitHub rejects files over 100 MB: split big packages (Intel Bluetooth
firmware is its own package for that reason). Publish after building packages people should get.

## Signing keys and rotation

Installed systems trust two keys, both shipped in `/etc/apk/keys` by `apk-tools` (and in `keys/trusted/`):

- `melon-signing.rsa`: the everyday key. Everything is signed with it. Keep it on **one** build machine.
- `melon-backup.rsa`: the offline backup key. Its private half lives off every build machine (a USB stick
  in a drawer) and is used only for a rotation. Never leave it in `keys/`.

If `melon-signing.rsa` leaks or is lost:

1. Make a new everyday key: `openssl genrsa -out keys/melon-signing.rsa 4096`, then
   `openssl rsa -in keys/melon-signing.rsa -pubout -out keys/melon-signing.rsa.pub`, and copy the `.pub`
   into `keys/trusted/` (replacing the old one).
2. Bump `pkgrel` of `apk-tools` (it now ships the new public key and the backup key, not the old key) and
   build it signed with the backup key: `MELON_SIGN_KEY=/path/to/melon-backup.rsa scripts/melon-build apk-tools`.
   Installed systems accept it because they already trust the backup key.
3. Publish that transition repo (its index is signed with the backup key too) and let systems upgrade.
4. Rebuild and re-sign everything with the new key (`build-everything.sh` with an empty `repo/`), publish.
   Systems that upgraded in step 3 trust only the new key and the backup key from then on.
5. Make a new backup key the same way, ship its public half in `apk-tools` (bump `pkgrel`), put the private
   half offline.

## Sources

The original build container can't reach kernel.org, gnu.org or most upstream sites. It can reach the
Ubuntu archive and public GitHub. Upstream tarballs were therefore taken from the Ubuntu 26.04 source
archive (`apt-get source --download-only <pkg>`, then the `*.orig.tar.*`, symlinked into `sources/` under
its upstream name) or from GitHub (apk-tools). Firmware blobs come from Ubuntu's `linux-firmware-*`
.debs. If you have normal internet access, fetching the same versions from upstream is fine, as long as
the tarballs are identical.

## Roadmap

- **Stage 1 (base):** toolchain, base packages, live ISO, installers. In progress. See the task list in the PR/issue.
- **Stage 2 (desktop):** udev replacement (eudev or libudev-zero), dbus, elogind or seatd, Mesa with LLVM
  (radeonsi for the Ryzen iGPU), Qt 6, KDE Frameworks 6, Plasma, KWin (Wayland), SDDM, PipeWire,
  NetworkManager, Calamares with melon branding and the gauntlet, and the desktop profile for both installers.
- **32-bit (i686) console edition: paused by the owner** (resume later). Everything is arch-aware
  (`MELON_ARCH=x86`), but the i686 toolchain doesn't finish yet: GCC's final build fails in libatomic's
  configure because `--enable-default-ssp` on i386 needs `__stack_chk_fail_local`, which comes from a
  `libssp_nonshared.a` (Alpine builds one in its musl package). Add that to the musl build (or drop
  default SSP for i686), then run `scripts/queue-2.sh`'s 32-bit part.
- **Stage 3 (gaming):** Flatpak and dependencies, Flathub remote, Steam through Flatpak, gamepad udev rules.

## Git conventions

- Small, focused commits. The message says what changed and why.
- Build outputs and downloaded sources are never committed.
- If you change a recipe, bump `pkgrel` (or `pkgver`) in the same commit.
