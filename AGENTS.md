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
| init | runit 2.3 | stages in `/etc/runit/{1,2,3}`, services in `/etc/sv`, enabled = symlink in `/var/service`; stays melon's init (see Roadmap) |
| devices | eudev 3.2 on the desktop profile (`udevd` service), BusyBox mdev (`mdevd` service) on the console profile | the desktop profile swaps `mdevd` for `udevd`; see rule 44 |
| packages | apk-tools 3.0.8 | repo index `Packages.adb`, signed with `keys/melon-signing.rsa` |
| kernel | Linux 7.0, `linux-melon` (generic) | config = `x86_64_defconfig` + `recipes/linux-melon/config-melon` |
| boot | GRUB 2.14, both `i386-pc` and `x86_64-efi` | ISO and installs boot on BIOS **and** UEFI |
| disks | GPT: 1 MiB BIOS boot, 1 GiB FAT32 `/boot` (also the ESP), XFS `/` | kernel boots with `root=PARTUUID=...`; only encrypted (LUKS) installs have an initramfs (rule 28); dual boot next to Windows on UEFI (see Installers) |
| filesystem | merged `/usr`: `/bin`, `/sbin`, `/usr/sbin` -> `usr/bin`, `/lib` -> `usr/lib` | packages must only ship files under `/usr`, `/etc`, `/var`, `/boot` |
| desktop | KDE Plasma 6.6 on Wayland (KWin, Xwayland), Qt 6.10, SDDM | desktop ISO and desktop profile; list in `scripts/desktop-packages.txt` |
| 32-bit edition | i686 (`MELON_ARCH=x86`, `-march=pentium-m -mfpmath=sse`: SSE2, as Qt 6 needs), LXQt 2.4 on Wayland with labwc 0.20 (wlroots), SDDM, Mesa without LLVM (i915, crocus, r300, softpipe) | the owner's decision (7 October 2026): LXQt is the 32-bit desktop, for netbooks such as the MSI Wind U100 (Atom N270, 1–2 GB); package list `scripts/desktop-packages-x86.txt`, recipes from `scripts/gen-lxqt-recipes.py` plus `recipes/melon-lxqt` (melon's LXQt defaults); no Plasma, Flatpak or NVIDIA there |
| desktop plumbing | D-Bus, elogind, polkit, PipeWire + WirePlumber, NetworkManager, BlueZ, CUPS, UDisks2 | console profile keeps `dhcp` + wpa_supplicant |
| graphics | Mesa 26.0 with LLVM: radeonsi/RADV, iris/ANV, nouveau, llvmpipe; zink (OpenGL on Vulkan) | |
| developer tools | gcc 15.2 + g++, binutils 2.46, make 4.4.1, pkgconf, patch (`gcc`, `g++`, `binutils`, `make`, `pkgconf`, `patch`) | built cross-native with the cross toolchain's settings (PIE, SSP); in the package repository only, not on the ISOs; `melon-first-boot` offers them on first login (default no) |
| build tools | perl 5.44, m4, bison, flex, gawk, gperf, GNU bc, texinfo, autoconf 2.73, automake 1.19, autoconf-archive, file; cmake 4.4, meson 1.12, ninja 1.13, git 2.56, nasm 3.01, tcl 8.6, rsync 3.5, lz4; xorriso, mtools, scdoc, itstool, dtc, pahole (`dwarves`), rpcgen (`rpcsvc-proto`), the Public Suffix List, Python's mako, Jinja2, pyparsing, PyYAML, packaging, pexpect and libxml2 bindings (`python3-*`) (`BUILDTOOLS`, `BUILDTOOLS2` in `build-everything.sh`) | what melon's recipes need to build on melon itself (self-hosting, see Roadmap); package repository only. gawk and GNU bc take over BusyBox's `awk`/`bc`/`dc` links; BusyBox's `/usr/bin` trigger puts them back when those packages go |
| compilers, VMs | clang 21 (+ `libclc`, `spirv-llvm-translator`), Go 1.27, gh (`github-cli`), QEMU 11.1 + OVMF (`qemu`, `ovmf`) (`STEP2` in `build-everything.sh`) | package repository only. clang: Mesa's OpenCL C shader compiler on melon; QEMU + OVMF run melon's own install tests on melon; OVMF is Ubuntu 26.04's prebuilt `ovmf-generic` at Ubuntu's paths |
| NVIDIA | the open driver: nouveau + `linux-firmware-nvidia` (GSP 570.144 from upstream linux-firmware) + NVK (`mesa-nvk`), with zink for OpenGL; or NVIDIA's own kernel driver: `nvidia-open` 615 (open modules + NVIDIA's firmware) | package repository only; `melon-first-boot` asks which one on computers with an NVIDIA card (GTX 16/RTX 20 and newer). NVIDIA's userspace needs glibc: with `nvidia-open` only Flatpak apps (Steam) get NVIDIA's libraries, from Flathub |
| gaming | Flatpak 1.16 + Flathub, GameMode | Steam is glibc-only, so it can't run natively on musl: `melon-first-boot` offers Steam (and Firefox, VLC, Prism Launcher) from Flathub on first login |
| game library | SDL3 3.4 + SDL3_image + SDL3_ttf (`sdl3`, `sdl3-image`, `sdl3-ttf`) | for melon's own games; SDL dlopen()s its Wayland/X11/audio backends |
| apps and games | NetHack 5.0 (both ISOs), melon pinball (desktop ISO; its own repo, a submodule), Cataclysm: DDA and GNU gettext (package repository only) | what goes on an ISO follows the size rule in "Contributing" |

Owner's config (`CONFIG_*` answers) lives in `docs/config.txt`. Don't change those choices without the owner's approval.

To build on another machine (Ubuntu 24.04 or WSL2, Debian or Devuan through the same apt path, or melon), follow
`BUILDING.md`: `scripts/host-setup.sh`, then `scripts/build-everything.sh`. Scripts find the repo from their own
location; recipes use `$M`. Never write a machine's absolute path into a script or recipe.

## Repository layout

```
scripts/env.sh          paths: M, TARGET, TOOLS, SYSROOT, SRC, WORK, REPO
scripts/toolchain.sh    cross toolchain, part 1 (binutils, headers, gcc stage 1)
scripts/toolchain-finish.sh  part 2 (musl, full gcc)
scripts/melon-build     build ONE recipe -> signed .apk(s) in repo/x86_64, reindex, install into sysroot
scripts/build-all.sh    build a list of recipes in order, logs to logs/pkg-<name>.log
scripts/check-order.sh  every makedepends is built before the recipe that needs it (build-everything.sh runs it first)
scripts/mkiso.sh        live/installer ISO: rootfs.sqfs (pristine apk-installed system, copied to disk by the
                        installers) + live.sqfs (small live-session layer) + a small repo of extras (VM guest tools,
                        a few base packages; never a second copy of what rootfs.sqfs already holds)
scripts/qemu-test.py    headless boot + install + reboot test over the serial console
recipes/<name>/MELONBUILD   one directory per recipe (see below)
recipes/<name>/*.patch      applied automatically with patch -p1, in name order
recipes/<name>/<pkg>.post-install etc.   apk scripts for (sub)package <pkg>
iso-files/init          live initramfs /init
branding/               logo (transparent PNGs)
site/                   the website: index.html, style.css, gauntlet.js (a demo of the installer's gauntlet), distro-finder/;
                        scripts/publish-site.sh adds it as a commit on the gh-pages branch (what GitHub Pages serves)
art/site.py             draws site/assets/melon-pixel.svg (melonfetch's melon) and net.svg
recipes/calamares-melon/    graphical installer branding + gauntlet (stage 2)
recipes/melon-pinball/      melon's own pinball game (C++/SDL3); the game is a submodule (game/ = melon-77/melon-pinball)
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
- Triggers: `triggers_<x>=(dir ...)` plus a `<pkg>.trigger` script; apk runs the script (with the changed
  directories as arguments) after any transaction that touches those directories, and when `<pkg>` itself is
  installed. Use them for caches other packages feed, e.g. `recipes/shared-mime-info`.
- `cmake_native <args>` configures CMake as a native build with melon's compilers and sysroot, so generators the build
  makes (LLVM's tablegens) run here (rule 18); `recipes/llvm`, `recipes/clang`. `cmake_setup` is the cross build.
- `py_install <name> <dirs...>` installs pure-Python modules into python3's site-packages with a small `.dist-info`
  (`recipes/python3-mako`). Modules with C parts need their build system (`recipes/python3-libxml2`).
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
    **On a melon build machine, never do this:** there the loader is the system's own musl, and pointing it at the
    sysroot swaps libc under every program. `host-setup.sh` only writes `/etc/ld-musl-x86_64.path` with the system's
    directories first and `sysroot/usr/lib` last (BUILDING.md, "melon as the build machine").
19. **`meson_setup` drops `-D` options the project doesn't have** (`scripts/meson-filter-opts.py`) and
    unsets the `PKG_CONFIG_*` variables for native lookups. Upstream renames options often; a stale
    option must not fail the whole build.
20. **Some generators must come from the host, not the sysroot:** `wayland-scanner` 1.24 in
    `/usr/local`, Mesa's `mesa_clc`/`vtn_bindgen2` in `hosttools/bin` (built against host LLVM), and the
    Qt 6 host tools in `hosttools/qt6` (`scripts/host-qt.sh`, same Qt version as the target, passed as
    `QT_HOST_PATH`). The host also needs `libltdl-dev` (libffi's autoreconf). Meson only finds programs for a
    cross build in the cross file's `[binaries]`, never on `PATH`, so `melon-build` lists the host Qt tools
    (moc, uic, rcc, ...) and `bwrap` there.
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
    (cryptsetup's autoreconf), `libltdl-dev`, `libxml2-utils` (shared-mime-info runs `xmllint`), `appstream` +
    `libappstream-dev` + `itstool` (appstream's metainfo), `nasm` (FFmpeg's x86 assembly), `bubblewrap` (meson checks
    for `bwrap` in flatpak and xdg-desktop-portal).
28. **Only encrypted installs have an initramfs.** `melon-mkinitramfs` builds it when
    `/etc/melon/encrypted-root` exists; `melon-update-grub` then writes `cryptroot=UUID=<luks>
    root=/dev/mapper/melonroot` and an `initrd` line. Everything else still boots straight from the kernel.
30. **musl 1.2.4+ hides the `*64` LFS types** (`off64_t`, `ino64_t`) unless `_LARGEFILE64_SOURCE` is
    defined. Add `-D_LARGEFILE64_SOURCE` to the recipe's CFLAGS (gpgme), as Alpine does.
31. **The builder's `apk add --upgrade` only reinstalls a package whose version-release changed.** Generated
    recipes take their `pkgrel` from the `PKGREL` table in `scripts/gen-simple-recipes.py`.
29. **Two build queues may run at once** for the same arch: `melon-build` takes `flock repo/<arch>/.lock`
    around sysroot installs and reindexing, and writes packages under a temporary name first.

32. **pkg-config puts the sysroot in front of path *variables*** (`xkb_base`, `dridriverdir`, dbus and polkit
    dirs, ...). Builds then either install under `/home/.../sysroot/...` (melon-build moves those files back and
    logs `LEAK: files installed under ...`) or compile the build machine's path into a program (logged as
    `LEAK: build-machine path compiled into ...`). **Read the LEAK lines after every build**: compiled-in paths
    need an explicit option (`-Dxkb_bin_dir=/usr/bin`, elogind's `-Ddbuspolicydir=...`) or a patch that strips
    `meson.get_external_property('sys_root')` (libxkbcommon, Xwayland). Paths in dev files (`.pri`, sbom) are harmless.
33. **Runtime dependencies nothing links against are not found automatically**: data (xkeyboard-config), QML
    modules, programs started by name (kded6, xkbcomp), Qt platform plugins (qt6-qtwayland). Declare them in
    `depends=` or list them in `scripts/desktop-packages.txt`.
34. **runit readiness:** `sv` defaults to `/service`, so `SVDIR=/var/service` is set in stage 2 and `/etc/profile`.
    `sv check <svc>` only means "the process exists" unless the service has a `./check` script; services others
    wait for ship one (dbus: bus socket exists; elogind: seat0 is set up; polkitd: owns its bus name). Only runit
    starts elogind and polkitd (their D-Bus activation files run /bin/false): otherwise the first daemon to ask for
    them gets a D-Bus-activated copy, and the supervised one exits and restarts every second because the name is
    taken (polkitd did that on installed systems). Services that need them at startup wait with `sv check`.
35. **Image assembly installs BusyBox, then melon-base, then everything else** (`mkiso.sh`): install scripts need
    /bin/sh and adduser, and the users they add (messagebus, polkitd, sddm) must not be overwritten by melon-base's
    `/etc/passwd` arriving later.
36. **Build Qt QML (qt6-qtdeclarative) before anything with QML parts.** Qt modules and frameworks built without it
    silently leave their QML modules out (`Could NOT find Qt6Qml` in the log); rebuild them with a pkgrel bump.
37. **KWin needs `KWIN_BUILD_X11=ON`** even in a Wayland-only session: in 6.6 that switch also carries Xwayland,
    and startplasma starts KWin with `--xwayland`.
38. **SDDM's greeter runs on VT 7** (`-DSDDM_INITIAL_VT=7`). SDDM only counts a VT as busy when a logind session
    is on it, so with its default (VT 1) the greeter fights getty-tty1 for the terminal and crash-loops
    (`Failed to take control of "/dev/tty1"`, helper exit 5). Installed systems keep text logins on tty1-3.
    The live ISO has no getty on tty1 and never showed it: test desktop changes on an *installed* system
    (`qemu-test.py desktop-install`).
39. **`set -e` and `VAR=$(cmd)`: a failing `cmd` ends the script.** An assignment's exit status is its last command
    substitution. Probes that may find nothing (no sound card, no network) need `|| true` inside the `$( )`.
40. **GCC 15 compiles C23 by default**, where `void f()` means "no arguments". Old code and old configure tests
    that call such functions with arguments fail ("too many arguments to function"): build them with
    `CFLAGS="$CFLAGS -std=gnu17"` (gmp). Old `config.sub` files don't know `*-linux-musl`: copy the build host's
    (`/usr/share/misc/config.sub`, `config.guess`) as libcanberra and libatasmart do. Tools a build runs on the
    build machine come from `CC_FOR_BUILD=gcc` when the project supports it.
41. **Absolute symlinks in `usr/lib` and `usr/include`** (`libfoo.so -> /usr/lib/libfoo.so.1`) point into the
    build machine's own `/usr` from the sysroot, so the linker can't find the library. melon-build makes them
    relative when packaging; a package built before that needs a pkgrel bump to reach the sysroot (rule 31).
42. **`.pc` files and `*-config` scripts copy pkg-config's sysroot answers** (`Libs.private: -L<sysroot>/usr/lib`).
    melon-build turns sysroot paths in `.pc` files into plain `/usr` paths; a `*-config` script needs its recipe to
    do the same (`recipes/cups`). The `LEAK:` lines name what is left.
43. **Kernel options with unmet dependencies vanish silently** in `olddefconfig` (the touchpad's
    `I2C_DESIGNWARE_PLATFORM` needed `I2C_DESIGNWARE_CORE`; `HP_WMI` needed `X86_PLATFORM_DRIVERS_HP`). The kernel
    recipe now stops when an option from `config-melon` isn't in the final `.config`: look its dependencies up in the
    Kconfig files and add them. The built config ships as `/boot/config-melon`; check it, not the fragment. For a
    real machine, `sudo melon-hwreport` lists every PCI/USB/ACPI/I2C device with the driver it got, the modaliases of
    devices that got none, graphics, network, sound, Bluetooth, power and service state, and the kernel's firmware
    messages; map IDs to modules with `modprobe -R <modalias>` against the built kernel. (`scripts/melon-diag.sh`, the
    old `curl | sh` tool, is now a wrapper that prints the same report.)
44. **Device events between stage 1's udevd and the udevd service are lost.** Stage 1 runs a udevd for the boot
    coldplug and stops it; the runit service starts a new one a moment later. A Wi-Fi card's interface appears only
    after its firmware loads (MT7921, iwlwifi, ath11k), often in that gap: udev never processed it, so it kept the
    kernel name `wlan0` and NetworkManager left it "unmanaged" (the ProBook's MT7921). The service replays the "add"
    events for net, ieee80211, rfkill and bluetooth devices once it runs (`recipes/eudev/udevd.run`).
45. **Packaged files must not keep the builder's account.** `cp -a $startdir/files/.` keeps the checkout's owner;
    apk records that account's name, and on a melon system without it the files become `nobody`'s. Service run
    scripts, `/etc/profile.d` and the installer's helpers were writable by `nobody`, then run by root. `melon-build`
    gives root every file owned by an account from 1000 up; system accounts a recipe sets on purpose (below 1000)
    stay. The desktop-install test fails if anything under `/usr` or `/etc` belongs to `nobody`.
46. **Rust recipes cross-compile with the build machine's Rust** (`scripts/host-rust.sh` puts upstream's release
    binaries in `hosttools/rust`, with the standard library for `$RUST_TARGET`). melon-build points cargo at
    `x86_64-unknown-linux-musl`, links with melon's gcc and passes `-C target-feature=-crt-static -C link-self-contained=no` in
    `RUSTFLAGS` (never `CARGO_TARGET_<triple>_RUSTFLAGS`, which cargo merges with a project's own `.cargo/config.toml`, and
    ripgrep's turns static linking back on): Rust's musl targets link statically by default, and melon's programs share its libc. Build scripts and proc-macros run on the build
    machine; their C parts get the build machine's gcc from the triple-named variables (`CC_x86_64_unknown_linux_gnu`,
    ...), never `HOST_CC`, which other build systems read too. `cargo_fetch` (in `prepare()`) downloads crates from
    crates.io into `sources/cargo`, pinned by the checksums in the project's `Cargo.lock`; `cargo_build` then builds
    offline with `--locked`, and `cargo_out` names the output directory. A build script that asks git for a commit
    hash would find melon's own checkout: melon-build exports `GIT_CEILING_DIRECTORIES=$WORK` for every build (clang's
    `--version` had named melon's repo). `-sys` crates build their own static copy of
    a C library for musl targets unless told otherwise (`PCRE2_SYS_STATIC=0` in ripgrep): link melon's. **Every recipe
    that puts Rust code into a package says `options=(rust)`**: the installers' "remove everything built with Rust"
    option finds the packages by it (see Installers).

47. **Flatpak must be built with X11 authorization (`-Dxauth=enabled`, libXau).** Without it, sandboxed X11 apps get
    the host's `DISPLAY` and a path to an Xauthority file that doesn't exist inside the sandbox. Xwayland refuses them
    ("Authorization required, but no authorization protocol specified") and they never open a window: Steam and VLC
    "refused to launch". The desktop-install test checks that `/usr/bin/flatpak` links libXau.

48. **A configure that must run its test programs can run as if native** (rule 18): perl's `Configure` gets `-Dcc=$CC`
    and the sysroot's `libpth`/`usrinc`, and the recipe strips the sysroot and the triple from the installed
    `Config.pm` afterwards, so modules built on melon use `gcc` and `/usr`. The same trick compiles `file`'s magic
    database with the `file` just built. Perl's MakeMaker writes the directory of every library it finds into the
    module's RPATH through `LD_RUN_PATH`: `make LD_RUN_PATH=` keeps the sysroot out (a `LEAK:` line otherwise).
    CMake's own `try_run` checks run the same way: `-DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/env` (recipes/cmake).
    A library built without a SONAME (perl's, Tcl's) gets one on its link line, or the `so:` dependency on it can't be met.

49. **melon's llvm-dev keeps only five static archives** (LLVMSupport, LLVMDemangle, LLVMTableGen, -Basic, -Common): the
    rest (about 1 GB) would pass GitHub's file limit, and everything links the shared libLLVM. Tablegen tools always link
    those five statically, and a standalone build against melon's LLVM (clang, recipes/clang) makes its own
    `clang-tblgen`. The recipe also drops the deleted archives from `LLVMExports-release.cmake`: `find_package(LLVM)`
    refuses to configure while an exported file is missing.
50. **Go records its defaults when it is built:** a set `CGO_ENABLED` becomes the permanent default, and the C compiler's
    name is baked in. recipes/go leaves `CGO_ENABLED` unset (cgo switches on by itself when a compiler is installed) and
    builds with `CC=gcc`, a wrapper that runs melon's cross compiler, so Go on melon calls plain `gcc`. Go programs in
    recipes (recipes/github-cli) build with melon's own `go` from the sysroot; their modules are downloaded in
    `prepare()` into `sources/gomod`, checked against `go.sum` and sum.golang.org, and the build runs with `GOPROXY=off`.
51. **NVIDIA's kernel modules fit one exact kernel.** `nvidia-open` is built against melon's kernel source with the exact
    `/boot/config-melon` (a full kernel build, for `Module.symvers`, which NVIDIA's configure checks read) and depends on
    that `linux-melon` version-release: **every linux-melon version or pkgrel change needs an nvidia-open pkgrel bump
    and rebuild in the same change**, or systems with it can't take the new kernel. Its firmware (`nvidia-open-firmware`)
    is NVIDIA's, shipped unmodified with NVIDIA's licence (the licence's condition for redistributing it). A real test
    needs an NVIDIA card: QEMU can't emulate one. What the build machine checks: the modules' vermagic, `depmod -e`
    against `System.map`, and loading them in a melon VM (`NVRM: No NVIDIA GPU found`, then a clean exit).
52. **NVK (recipes/mesa-nvk) is Rust**, so it is its own recipe with `options=(rust)` (the rest of Mesa isn't Rust, and
    "remove everything built with Rust" must not take the whole Mesa). Keep its `pkgver` equal to `mesa`'s. Meson
    cross-compiles its Rust through an extra cross file (`rust` with rule 46's flags, `bindgen`, `cbindgen`); bindgen and
    cbindgen are the build machine's (`scripts/host-rust.sh`) and load the build machine's libclang, which needs
    `-resource-dir` to find its own headers (`BINDGEN_EXTRA_CLANG_ARGS`). Mesa's Rust crates come through its meson wraps
    (pinned by hash) into `sources/mesa-packagecache`.

53. **On melon, the build machine's triple is not melon's.** `gcc -dumpmachine` on melon prints `x86_64-melon-linux-musl`,
    the cross toolchain's own target, so binutils and gcc would configure themselves as native compilers and autoconf
    would see `--build` equal to `--host`. `env.sh` sets `BUILD_TRIPLE` (`x86_64-pc-linux-musl` on melon, gcc's own
    answer elsewhere): `melon-build` passes it as `--build`, and `toolchain.sh` gives binutils and gcc
    `--build`/`--host` from `TOOLCHAIN_HOST_FLAGS` (empty on Ubuntu, so Ubuntu builds don't change). Programs built
    for the Ubuntu host (`tools/`, `hosttools/`) don't run on melon (no glibc): rebuild them there. Host Rust is
    upstream's musl-hosted build on melon (`host-rust.sh`); there the build machine's Rust triple equals
    `$RUST_TARGET`, so build scripts' C parts use melon's cross gcc too, and they still run because the build machine
    is melon. Recipes must not assume Ubuntu paths (`/usr/lib/llvm-*`): look for melon's (`/usr/lib`) as well.
    melon's `tar` is BusyBox's, whose xz decoder stops at a 64 MiB dictionary (Rust's tarballs use 128 MiB: "tar: corrupted
    data"): unpack `.xz` with `untar` (melon-build) or `xz -dc | tar -xf -`, never `tar xJf`. Host Rust programs that dlopen()
    (bindgen loads libclang) must be built with `-C target-feature=-crt-static` there: a static musl program can't dlopen.
    melon's `/usr/include` lacks `sys/cdefs.h` without `bsd-compat-headers` (rule 5), and its python3 has no pip.
    `find`, `grep` and `realpath` are BusyBox's too: no `find -uid/-gid` or size suffixes beyond `k`, no `grep --exclude`,
    no `realpath` options, no `ln -r`, no `diff --version` (libvpx's configure asks), and `sed` isn't GNU sed; melon's bison has no `yacc` command (`YACC="bison -y"`).
    Scripts and recipes use what both have (or python3). The build machine's gcc is GCC 15 (C23 by default,
    rule 40: the cross toolchain's in-tree GMP gets `-std=gnu17`), and its cmake is CMake 4
    (`CMAKE_POLICY_VERSION_MINIMUM=3.5` in melon-build for projects asking for less than 3.5). host-setup.sh links
    automake's `config.sub`/`config.guess` into `/usr/share/misc` and installs `libtool-dev` and `gettext-dev`
    (libtool.m4, autopoint) for autoreconf. Python modules a build imports on the build machine are melon packages too
    (no pip): elogind's build imports Jinja2 and flatpak's pyparsing, so host-setup.sh asks for `python3-jinja2` and
    `python3-pyparsing` and, while the online repository doesn't have them, copies each module from its sdist in
    `sources/` (a new recipe's package is online only once the owner publishes it, so whatever the build machine itself
    needs must also work before that). The same goes for tools: `rpcgen`
    (open-vm-tools' configure; melon's libtirpc has none) is built from `sources/` into `/usr/local/bin` until
    `rpcsvc-proto` is published.
    melon's `-dev` packages don't pull in the `-dev` packages their `.pc` files require, so a native pkg-config lookup
    on melon fails unless host-setup.sh asks for those too (`pcre2-dev` for glib-2.0, `xorgproto` and the libXau, Xdmcp,
    Xfixes and Xxf86vm `-dev` packages for x11 and gl): appstream's cross build looks the build machine's appstream up.
    QEMU's configure makes a venv with pip (`mkvenv.py`), which melon's python3 can't (no ensurepip): on melon,
    recipes/qemu and qemu-guest-agent use the host Python (`hosttools/python` keeps ensurepip) and put setuptools
    and wheel (PyPI wheels in `sources/`) next to QEMU's own in `python/wheels`, where its offline "tooling" group
    looks for them.
54. **i686 needs SSE2 and `libssp_nonshared.a`.** The 32-bit toolchain targets `-march=pentium-m -mfpmath=sse`
    (`GCC_ARCH`/`GCC_FPMATH` in `env.sh`): Qt 6 refuses to build without SSE2, and Alpine's x86 does the same. With
    `--enable-default-ssp`, position-independent i386 code calls the hidden `__stack_chk_fail_local`, which musl
    doesn't provide: `toolchain-finish.sh` and the musl recipe build `libssp_nonshared.a`, and
    `patches/gcc-i686/ssp-nonshared.patch` makes gcc link it (Alpine's way). Without it libatomic's configure fails.
55. **No text relocations on i686.** BusyBox's SHA-NI assembly (`CONFIG_SHA1_HWACCEL`, `CONFIG_SHA256_HWACCEL`) isn't
    position-independent on i386: the PIE BusyBox got a TEXTREL and every applet segfaulted at start. The recipe
    switches those off on i386. After a new 32-bit package, check `readelf -d` for `TEXTREL`.
56. **GRUB 2.14 with binutils 2.44 or newer links its kernel at the wrong address.** Its configure picks
    `-Wl,--image-base=0x400000`, which newer ld honours instead of `-Ttext`: `grub-mkimage` then stops with "kernel.img
    miscompiled ... start address is 0x9074 instead of 0x9000" (i386-pc). `recipes/grub` and `scripts/hostgrub.sh`
    preset `ax_cv_check_ldflags___Wl___image_base_0x400000=no`.

To resume a failed long build without unpacking again (for example the kernel):
`MELON_KEEP_SRC=1 scripts/melon-build linux-melon`. `build-everything.sh` does that by itself (`MELON_AUTO_RESUME=1`), but
only while the recipe and its patches are the ones the tree was unpacked with (`work/pkg/<name>/.prepared` holds their
checksum): after a fix to the recipe, the package unpacks fresh so the new patches and `prepare()` run.

The build container can be reclaimed while idle, which kills background builds. `scripts/resume.sh`
restarts the host Qt build and the Qt/KF6/Plasma queue (`scripts/queue-4.sh`); finished host Qt modules
and packages already in the repo (`MELON_SKIP_BUILT=1`) are skipped.

After an unclean shutdown (WSL restart, power loss), files written in the last minutes can be empty or
truncated. Before resuming: look for empty packages (`find repo -name '*.apk' -size 0`; the builder skips
nothing that is empty, but a truncated `.apk` still blocks reindexing), delete `work/pkg/<name>/.prepared`
of the package that was compiling so it unpacks fresh, and check `git fsck` (empty objects in `.git/objects`
can be restored with `git fetch` once they are moved aside). A package that fails to install into the sysroot
(a file conflict) stays in the sysroot's world file and breaks every later `apk add` there: remove it with the
builder's apk command and `del <pkg>` (see `sysroot_add` in melon-build) before rebuilding. `apk verify` needs an absolute `--keys-dir`:
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
scripts/qemu-test.py dualboot out/melon-*-x86_64.iso /tmp/dual.img        # install next to a fake Windows (UEFI), Windows untouched
MELON_EDITION=desktop scripts/mkiso.sh                                    # the Plasma live ISO (desktop edition)
scripts/qemu-test.py desktop out/melon-desktop-*-x86_64.iso              # services ready, Plasma running, a USB stick mounts through
                                                                          # UDisks2, screen-recording encoders work, a text file prints
                                                                          # to a virtual IPP Everywhere printer, nmcli online, and
                                                                          # NetworkManager joins WPA2 Wi-Fi on mac80211_hwsim radios;
                                                                          # LOOK at logs/qemu-desktop.ppm
qemu-img create -f raw /tmp/desk.img 16G
scripts/qemu-test.py desktop-install out/melon-desktop-*-x86_64.iso /tmp/desk.img   # install, SDDM greeter stays up,
                                                                          # log in through it, Plasma runs; LOOK at both screenshots
# --offline (any mode): the VM keeps its network card but reaches nothing outside; the installers must still work
```

**melon pinball** (`recipes/melon-pinball`; the game itself is the `game/` submodule, github.com/melon-77/melon-pinball:
change it there, then commit the new submodule commit here with a `pkgrel` bump): its build runs `melon-pinball-physics-test` (launches, flipper and mini
flipper shots, cradles, the ramp, 450 random balls that must never leave the cabinet) and `melon-pinball-game-test`
(a Harvest run through the Seed Market, melons and pests acting on the machine, kickback, spinner, magnet, a lost run),
so a change that breaks the table or the rules fails the package. To look at it without a screen:
`SDL_VIDEO_DRIVER=offscreen SDL_RENDER_DRIVER=software melon-pinball --screenshot out.png --seconds 30 --play`
(the demo plays a classic game; add `--harvest` for a run, `--lazy` to lose it quickly; `--shop`, `--packs` and
`--collection` show those screens; `--golden` the survivor look). The table's geometry lives in the game's `src/table.cpp` and drives
both the physics and the painted art; after moving anything, check the test's "at rest mid-table" count (a ball that
can come to rest off the flippers is a trap). Harvest's melons, grafts, pests and seed packs are data in `run.cpp`;
their effects are in `game.cpp` (`applyMachine` for the physics, `add` and `currentMult` for the scoring). Unlocks
are variety only (no permanent power) and live in the player's `~/.local/share/melon/pinball/unlocks.txt`.

Logs go to `logs/qemu-*.log`. The tests use KVM when `/dev/kvm` is usable (WSL2 has it); without it QEMU runs in
software emulation and everything is slow. Use generous timeouts. The ISO's GRUB and the installed system both use
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

- **Graphical installer (desktop ISO): Calamares with the gauntlet.** One Calamares view module
  (`GauntletViewStep`, `gauntlet.qml` in `recipes/calamares-melon/modules/gauntlet/`) in two parts:
  1. **The install gauntlet:** about 200 very easy questions (`questions.js`), one per screen, shuffled on every run
     (question order and answer order; attention checks stay after the question they name and show its new number).
     The Next question button waits a few seconds. A wrong answer sends you back 10 questions, never out of the
     installer. Calamares' Next button stays locked until the last question.
  2. **The final trial (optional):** 50 real questions (`trial.js`: the answers are only there as salted md5
     hashes; the plaintext draft lives outside the repo), shuffled, 90 seconds each. A wrong answer or a timeout
     sends you back 10; in the last 20 any mistake restarts the finale. Walking away still installs melon.
  It's deliberately slow, to put off people who are only there for status. Keep it that way. Only passing the trial
  (the page writes `/run/melon/.trial-passed`) earns the rewards: melon in gold (the `melon-gold` login screen via
  `/etc/sddm.conf.d/20-survivor.conf`, the MelonGold colours, the `melon-gold` Plasma style, the MelonGold Konsole
  profile and the golden melon on the lock screen, applied on first login by `melon-survivor-look`, and the gold
  boot menu, which melon-update-grub picks by the badge), three wallpapers ("the other side" portal, golden rain and
  the golden melon), an SVG certificate in `~/Pictures` and a melonfetch badge (`/etc/melon/gauntlet-survivor`). The
  rewards live hidden in melon-desktop (`/usr/share/melon/.rewards`) and `melon-rewards ROOT USER` hands them out
  (`cal-finish`, and the unlock command below).
- **melon's look (melon-desktop):** Plasma style `melon`, colours MelonDark, Konsole profile Melon, SDDM theme `melon`
  (its own QML, `usr/share/sddm/themes/melon`; the gold edition reuses the same `Main.qml` with another
  `theme.conf`), GRUB theme `usr/share/melon/grub/themes/melon` (the desktop ISO uses it too). The art generators are
  in `art/` (run from a checkout of the melon-art working directory with its fonts); `art/grubtheme.py` needs
  `grub-mkfont` (from Ubuntu's grub-common: `apt-get download grub-common` and `dpkg -x` it, no install needed).
- **Hidden owner commands** work like the console installer: `/etc/profile.d/zz-melon.sh` recognises them by the
  first 16 hex digits of their name's sha256 and nothing else. The same rules apply: never write their names in any
  file, comment, commit or test. `59c1a50f2e93bdc1` unlocks every gauntlet reward (`/usr/libexec/melon/.gold`,
  desktop only; tests call that path). `721b2b05ebcca53c` skips the gauntlet (`/usr/libexec/melon/.pass`, live desktop
  only): it creates `/run/melon/.gauntlet-skip`, the page then unlocks Calamares' Next, and a skipped gauntlet earns
  no rewards.
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
- **Dual boot.** On UEFI, when the chosen disk already has an EFI system partition and at least 20 GiB unallocated
  (Windows' Disk Management "Shrink Volume" makes that), the console installer offers `alongside` (the default
  then; `MELON_MODE=alongside|erase`): a 1 GiB FAT32 `/boot` (extended boot loader type) and `/` go into the free
  space, the existing EFI partition becomes `/boot/efi`, GRUB goes into `EFI/melon` with a firmware boot entry
  (efibootmgr), and nothing of the other system is touched: no BIOS boot code, and `EFI/BOOT` stays theirs.
  Calamares offers "Install alongside" (it shrinks NTFS with ntfs-3g's ntfsresize), "Replace a partition" and manual
  partitioning; nothing is preselected, and `cal-finish` only writes `EFI/BOOT` on an EFI partition that is
  melon's alone. `melon-update-grub` adds a Windows entry for Windows Boot Manager on any EFI partition (UEFI) or
  `bootmgr` on an NTFS partition (BIOS). Test: `qemu-test.py dualboot` (a fake Windows disk, persistent UEFI boot
  menu, checksums of everything Windows owns before and after).
- Unattended install variables: `MELON_DISK MELON_HOSTNAME MELON_ROOTPW MELON_USER MELON_USERPW
  MELON_PROFILE MELON_YES=1 MELON_SERIAL=1 MELON_WIFI_SSID MELON_WIFI_PSK MELON_ALPINE=y
  MELON_ENCRYPT=y MELON_LUKSPW MELON_MODE=alongside MELON_NORUST=y`. With `MELON_YES=1`, questions that have a default take it.
- **"Remove everything built with Rust"** (the owner's decision): a checkbox at the end of the gauntlet (next to the
  Alpine one) and a question in the console installer (`MELON_NORUST=y`). `melon-remove-rust ROOT` (melon-base) runs
  `apk del -r` on every installed package from `/usr/share/melon/rust-packages` (mkiso.sh lists the recipes with
  `options=(rust)`, which includes the `rust` and `cargo` packages), taking everything that depends on them along. **No safeguards, on purpose**: if the desktop
  needs a Rust package, the desktop goes too. The reward comes first, so it stays whatever the removal takes:
  `melon-rust-free ROOT USER` (melon-desktop, from `/usr/share/melon/.rewards/rust-free`) installs the
  "farewell, Ferris" wallpaper (drawn by `art/ferris.py`, which also writes the lossless master `art/ferris.png`), sets
  it on the user's first Plasma login and leaves the badge `/etc/melon/rust-free`.
- **Profiles** live in `/usr/share/melon/profiles/` on the live system: `<name>` is the package list,
  `<name>.services` the runit services (`name` enables one, `-name` drops a base service). `mkiso.sh`
  writes them. The desktop profile swaps `mdevd`/`dhcp` for `udevd` and NetworkManager, and the
  installer gives NetworkManager the Wi-Fi network instead of wpa_supplicant.
- **Other distros' repos are opt-in only.** Both installers offer Alpine as the tagged repo `@alpine`
  (`melon-repo enable alpine`); apk only uses it for packages asked for as `name@alpine`. Void isn't
  offered (xbps, not apk). Never make a foreign repo untagged or on by default.

## Website

The website is `site/`, published to the `gh-pages` branch by `scripts/publish-site.sh` (GitHub Pages serves that branch at
`https://melon-77.github.io/melon-os/`). Edit it here and open the pull request against `testing`; never edit `gh-pages`
by hand. Keep it dependency-free: hand-written HTML and CSS, the three fonts hosted in `site/fonts` (no Google Fonts or
other third-party requests, which the footer promises), no trackers. Its facts must stay true to the repository:
versions and sizes come from the latest release, and an edition that isn't published yet (the 32-bit LXQt one) says so.
`gauntlet.js` is only a taste of the real gauntlet, using easy questions that already appear in
`recipes/calamares-melon/modules/gauntlet/questions.js`; never copy anything from `trial.js` (not even its questions) into the site, and keep
the hidden owner commands out of it, as everywhere else. The pixel melon is generated from `melonfetch`'s own awk drawing
(`art/site.py`), so change the melon there, not in the SVG.

## Package repository (online)

`scripts/publish-repo.sh` puts `repo/<arch>/` on the `packages` branch of github.com/melon-77/melon-os (one
commit, force-pushed each time), served as
`https://raw.githubusercontent.com/melon-77/melon-os/packages/<arch>/Packages.adb`. The base URL is in
`/usr/share/melon/repo-url` (melon-base); the installers write it into `/etc/apk/repositories` before the
offline copy from the ISO, and the live ISO uses it too. Everything is signed with the melon key, so the
host doesn't need to be trusted. GitHub rejects files over 100 MB: split big packages (Intel Bluetooth
firmware is its own package for that reason). Publish after building packages people should get.

## Releases

Releases are named **melon <version> “<melon variety>”**, the varieties in alphabetical order: 0.1 “Antalya”,
0.2 “Bailan”, then 0.3 “Cantaloupe”, 0.4 “Dudaim”, 0.5 “Esfahan”, 0.6 “Fukui”, 0.7 “Galia”, 0.8 “Honeydew”, … (the
owner's choice, 29 September 2026). Tags are `v<version>` from 0.3 on (0.1 and 0.2 kept their date tags). A release
is marked Latest (the website's download button points at `releases/latest`), carries both ISOs and `SHA256SUMS`,
has notes written for users (what's new, which file to download, `doas apk upgrade` for installed systems), and the
release it replaces is retitled "(superseded)" with a link to the new one.

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

**New sources come from upstream, not from Ubuntu** (the owner's decision, 29 September 2026). Take each project's own
release: the tarball from its official site or a pinned tag of its official repository. Firmware comes from upstream
linux-firmware (kernel.org), NVIDIA's driver from NVIDIA. Verify every download: the release's signature where the
project signs (the GNU keyring, kernel.org's `.sign` files, the release manager's key), otherwise its published checksum,
cross-checked against another distribution's recipe (Alpine's APKBUILD, Arch's PKGBUILD); say in the commit how it was
checked. List the file in `URL` (method `url`) or `GIT` in `scripts/make-manifest.py` and add its row to
`sources/MANIFEST.tsv` (sha256 checked on every fetch).

History: the first build container could only reach the Ubuntu archive and GitHub, so many older recipes still use
Ubuntu 26.04's `*.orig.tar.*` (the upstream tarballs, repacked by name only; method `apt`) and some firmware and data
come from Ubuntu `.deb`s (method `pool`). That's no reason to keep pulling from Ubuntu: when you touch one of those
recipes (a version bump, a fix), move its source to upstream. The build machine happens to run Ubuntu (BUILDING.md);
that is the only place Ubuntu is still needed.

## Roadmap

- **Stage 1 (base): done.** Toolchain, base packages, the console ISO and the quick console installer, BIOS and
  UEFI, LUKS encryption, dual boot next to Windows, VM guest tools.
- **Stage 2 (desktop): done.** eudev, D-Bus, elogind, polkit, Mesa with LLVM, Qt 6, KDE Frameworks 6, Plasma and
  KWin on Wayland, SDDM, PipeWire, NetworkManager, Bluetooth, printing, Calamares with the gauntlet, the desktop ISO
  and the desktop profile for both installers (`qemu-test.py desktop` and `desktop-install`). Open hardware work
  is tracked in GitHub issues: Intel SOF audio, newer linux-firmware, Broadcom Wi-Fi.
- **32-bit (i686) edition with LXQt: in progress** (resumed by the owner, 7 October 2026, for an MSI Wind U100:
  Atom N270, 2 GB). `MELON_ARCH=x86 scripts/build-everything.sh` builds the toolchain, the base system, Qt, LXQt and
  labwc, then both ISOs (BUILDING.md). Done: the toolchain (rules 54 and 55), the base system and the console ISO,
  which installs and boots in `qemu-test.py live|disk --i686` on QEMU's Atom N270 CPU model. In progress: the LXQt
  desktop ISO and its test (`desktop-install --i686`): SDDM's greeter and the LXQt session on labwc.
- **Stage 3 (gaming): in progress.** Done: Flatpak, the Flathub remote (`melon-flathub`), Steam, Firefox, VLC and
  Prism Launcher offered from Flathub on first login, GameMode. Still to do: gamepad and controller udev rules,
  MangoHud (`docs/stage2-plan.md`).
- **Init: runit stays melon's init (the owner's decision).** dinit may one day become an *optional variant* the owner
  builds, never a switch forced on the whole OS. Don't start a dinit port. Improve runit instead:
  readiness through `./check` scripts, and a clear start order in run scripts (a small shared helper is being
  considered). A variant would need services for both inits, so keep run scripts simple and self-contained; what a
  dinit variant would have to cover is listed in `docs/dinit-variant.md`.
- **Smaller desktop ISO: done.** The ISO's offline repo no longer carries a second copy of the desktop (it did: about
  460 MB on the ISO and on every install, in `/var/lib/melon/repo`). The installers only need that repo for extras; a
  desktop app someone removes comes back from the online repo. Tested with `qemu-test.py ... --offline`.
- **Rust:** recipes can be written in Rust (rule 46; ripgrep is the first), and melon has its own `rust` and `cargo`
  packages (`recipes/rust`, rustc 1.98.1 built from source, in the package repository only, not on the ISOs). Built
  and tested on the build machine (29 September 2026): cargo builds and runs a program with a crates.io dependency on
  melon. `codegen-tests = false` in its `bootstrap.toml`, because melon's llvm ships no FileCheck. It builds cross-native with build = host = target = musl
  (`x86_64-unknown-linux-musl`), from upstream's musl-hosted stage 0 (manifest entries), against melon's LLVM 21
  (shared) and its native gcc; `musl-dynamic-by-default.patch` sets musl's `crt_static_default` to false as Alpine
  and Void do (upstream's own FIXME, compiler-team#422), so `cargo build` on melon links dynamically like melon's
  own programs. Both packages say `options=(rust)`, so "remove everything built with Rust" takes them off too. Watch
  for: RAM (rustc wants 2 to 3 GB per job), the size of `librustc_driver` against GitHub's 100 MB file limit
  (rule in "Package repository"), and `LEAK:` lines.
- **Self-hosting (building melon on melon): in progress.** Step 1, the build tools as recipes, is done. The first batch (perl,
  m4, bison, flex, gawk, gperf, bc, texinfo, autoconf, automake, autoconf-archive, file) is done and tested on melon
  (an autotools project with a bison grammar, a flex scanner, gperf and a Texinfo manual builds and runs; a Perl XS
  module builds and passes its tests). The second batch (cmake, meson, ninja, git, nasm, tcl, rsync, lz4) is done too:
  cmake and meson projects build with ninja, git clones over https, nasm output links and runs. The third batch
  (xorriso, mtools, scdoc, itstool, dtc, pahole, the Public Suffix List, and Python's mako, PyYAML, packaging, pexpect
  and libxml2 bindings) too: each was run on melon (an ISO and a FAT image made, itstool, scdoc, dtc and pahole output,
  pexpect driving a shell). **Step 2 is done too:** clang 21 with libclc and the SPIR-V translator (Mesa's
  `mesa_clc` and `vtn_bindgen2` build on melon from Mesa's source and turn OpenCL C into valid SPIR-V), QEMU 11.1 with
  libslirp and OVMF (melon's own `qemu-test.py live`, `disk` and `disk --uefi` pass when run inside melon with melon's
  QEMU, Python and pexpect), Go 1.27 (cgo on by default with melon's gcc) and gh. Claude Code installs on melon with
  Anthropic's own installer, which picks its musl build (README, "Developer tools"); it isn't packaged, as it isn't open
  source. Step 3: build melon on melon (`build-everything.sh` inside a melon system, the recipes' remaining build-machine
  tools such as Mesa's `hosttools/bin` and the host Qt and Rust replaced by melon's own), run the test suite there, and
  compare the packages with the WSL-built ones.
- **NVIDIA (the owner: both drivers, 29 September 2026): built, waiting for a test on real hardware** by a contributor
  with an NVIDIA card (GTX 16/RTX 20 or newer). Both ways are online installs chosen in `melon-first-boot`. Later: a
  `linux-melon-dev` package (the kernel's build files, `Module.symvers`) would spare nvidia-open its own kernel build and
  let people build other out-of-tree modules on melon.
- **Later:** a native Firefox build (needs clang and Node for melon as well as Rust; Firefox comes from Flathub until then).

## Contributing: workflow and the owner's rules

- **Pull requests go to `testing`.** `main` is protected ("changes must be made through a pull request"): nobody
  pushes to it directly, the owner and the build machine included. `testing` reaches `main` through its own pull
  request (`testing` -> `main`, merge commit), after which `testing` is fast-forwarded to `main` again.
- **Rebase, don't merge.** When `testing` moves, rebase your branch; no "Merge testing into ..." commits. PRs are
  squash-merged. No test/webhook commits and no personal email addresses in history (use GitHub's noreply address).
- **The build machine reviews PRs** about every 20 minutes. Its comments start with "Automated check from the melon
  build machine:" or "Automated reply ...". It reads the diff, builds every changed recipe, installs and runs the
  packages on a scratch melon root, runs the QEMU install tests when ISOs or installers change, and then requests
  changes with the exact errors, asks the owner about product decisions, or merges into `testing`. It never pushes
  to a contributor's branch. The owner answers on PRs or on the pinned issue #17 ("Owner <-> build machine").
- **What goes on an ISO (the owner's size rule):** small packages, below about 45 MB, may go on the ISOs; anything
  bigger is an online install from the package repository (gcc and friends, Cataclysm: DDA). **Extra desktops are
  always online installs, whatever their size**: Plasma stays the only desktop on the 64-bit ISO (issue #18, niri +
  Noctalia). The 32-bit desktop ISO carries LXQt instead of Plasma (the owner's decision); LXQt is in the 64-bit
  package repository too, as an online install.
- **Decided, don't re-propose:** Nix is available but off by default (not on the ISOs, no service, only root trusted);
  foreign repos stay opt-in and tagged; the gauntlet's rewards are only for people who pass its final trial; melon's
  own games may live in their own melon-77 repositories as submodules.

## Git conventions

- Small, focused commits. The message says what changed and why.
- Build outputs and downloaded sources are never committed.
- If you change a recipe, bump `pkgrel` (or `pkgver`) in the same commit.
- **Keep the docs true (the owner's rule, 7 October 2026):** when you find a bug or make a significant change, update every
  `.md` file it touches (README, AGENTS, BUILDING, `docs/`) and the website (`site/`) in the same pull request, so nothing is out of date.
