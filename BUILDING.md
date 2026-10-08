# Building melon on your own machine

Everything melon ships is built by the scripts in this repo, so any x86_64 machine with Ubuntu 24.04
can build it. On an 8-core/16-thread CPU a full build (toolchain, base system, Plasma desktop, ISOs)
takes roughly 8–12 hours; on 2 cores it takes days.

## Windows 11: WSL2

1. Install Ubuntu 24.04 in WSL2 (PowerShell as administrator):

       wsl --install -d Ubuntu-24.04

2. Give WSL enough memory. Qt and KWin need about 1 GB per compile job, and WSL only gets half the
   RAM by default. Create `%UserProfile%\.wslconfig`:

       [wsl2]
       memory=12GB        # leave Windows ~4 GB
       swap=16GB
       processors=16

   then `wsl --shutdown` and open Ubuntu again. With 16 GB of RAM or less, build with `JOBS=10`.

3. Build inside the Linux file system (`~/melon`), not under `/mnt/c`: it is many times faster.

4. Keep the laptop plugged in and stop it from sleeping (Settings → System → Power) while it builds.

## Steps (Ubuntu 24.04 or WSL2)

    git clone https://github.com/melon-77/melon-os ~/melon
    cd ~/melon
    git submodule update --init       # melon's own games (recipes/melon-pinball/game)
    sudo scripts/host-setup.sh        # build tools, sources (~2 GB), host tools; about an hour
    # put the signing key in place (below)
    sudo JOBS=16 scripts/build-everything.sh

`build-everything.sh` is resumable: after a reboot or a crash, run it again. It skips finished packages
and continues an interrupted one. Logs are in `logs/` (`logs/pkg-<name>.log` per package). The ISOs
land in `out/`. `scripts/publish-repo.sh` then puts the packages online for installed systems.

The build runs as root because packages are installed into `sysroot/` with their real owners and
because an Ubuntu host needs `/lib/ld-musl-x86_64.so.1` (see AGENTS.md, rule 18).

## Debian or Devuan as the build machine

The Ubuntu steps above work on Debian 13 and Devuan 6 too (not yet tried on a real install). `host-setup.sh` takes
Ubuntu's path there: it installs the same packages with apt, adds Ubuntu's archive key (`ubuntu-keyring`) because
older melon sources still come from Ubuntu's source archive, and lists any package name your archive doesn't have
instead of stopping (paste that warning line if you get one). Without `sudo`, run the commands as root (`su -`).

**Moving from another build machine (melon, Ubuntu, WSL):** copy `keys/`, `sources/`, `repo/` and `sysroot/` into the
new checkout, but not `tools/`, `hosttools/` or `work/`: they hold programs built for the old machine (melon's are linked
against musl, Ubuntu's against another glibc). `host-setup.sh` rebuilds the host tools and `build-everything.sh` the
cross toolchain before it carries on with the packages; finished packages in `repo/` are not built again.

## melon as the build machine

melon builds itself. On a melon install (desktop or console profile), starting with no checkout:

1. `doas apk upgrade`, then `doas apk add git` (git is in the online repository, not on the ISOs).
2. `git clone -b testing https://github.com/melon-77/melon-os ~/melon`, then `cd ~/melon`.
3. If you are moving from an old build machine, copy its `keys/`, `sources/`, `repo/` and `sysroot/` into `~/melon`
   (not `tools/` or `hosttools/`, see below). Otherwise put the signing key in `keys/` ("The signing key" below).
4. `doas scripts/host-setup.sh` (it lists any build tool melon's repository doesn't have; if a step fails, it prints
   the end of `logs/host-setup.log`, where the full output of its builds goes)
5. `doas scripts/build-everything.sh` (it uses every core; for fewer, `doas env JOBS=10 scripts/build-everything.sh`:
   doas doesn't take `NAME=value` before the command the way sudo does)

What's different from an Ubuntu build machine:

- `host-setup.sh` installs the build tools from melon's own package repository instead of Ubuntu's (it lists any name
  it can't find), adds a small `dpkg-deb -x` replacement for the recipes that unpack `.deb` files, uses melon's
  own `apk` and `wayland-scanner`, and builds `rpcgen` into `/usr/local/bin` while the repository has no `rpcsvc-proto`
  (Python's Jinja2 and pyparsing likewise come from their source tarballs until their packages are published).
- A package that failed is unpacked fresh on the next run when its recipe changed since (after a `git pull` with a fix);
  otherwise `build-everything.sh` carries on where it stopped.
- **Moving from an Ubuntu or WSL build machine:** keep `keys/`, `sources/`, `repo/` and `sysroot/`, but delete
  `tools/` and `hosttools/` (except `hosttools/bin/apk`, which `host-setup.sh` replaces anyway): they hold programs
  built against Ubuntu's glibc, which melon doesn't have. `host-setup.sh` rebuilds the host tools, and
  `build-everything.sh` rebuilds the cross toolchain (about an hour or two) before carrying on with the packages,
  skipping every package already in `repo/`.
- The loader is melon's own: `/lib/ld-musl-x86_64.so.1` is never pointed at the sysroot (AGENTS.md, rule 18).
  `/etc/ld-musl-x86_64.path` lists the system's library directories first and the sysroot's last.
- Keep the build machine's own packages up to date from the online repository, and don't upgrade it from packages it
  just built until they pass the QEMU tests: a broken gcc or musl on the build machine stops it from fixing itself.
  Keep a melon live USB around for that case.

## The 32-bit (i686) edition

The same checkout builds the 32-bit edition next to the 64-bit one; its files go to `tools-x86/`, `sysroot-x86/`,
`work-x86/` and `repo/x86/`, so the two never mix:

    sudo MELON_ARCH=x86 JOBS=16 scripts/build-everything.sh

(on melon: `doas env MELON_ARCH=x86 scripts/build-everything.sh`). It builds the i686 cross toolchain, the base
system, Qt and the LXQt desktop (not Plasma, Flatpak or the NVIDIA drivers), then `out/melon-*-i686.iso` and
`out/melon-desktop-*-i686.iso`. Logs carry `-x86` in their names (`logs/everything-x86-1.log`). Test with
`scripts/qemu-test.py ... --i686`: it runs `qemu-system-i386` with an Atom N270 CPU. The build machine runs i686
programs from the sysroot during the build, so its kernel needs 32-bit support (`CONFIG_IA32_EMULATION`, on in
Ubuntu, Debian, Devuan and melon); `host-setup.sh` points `/lib/ld-musl-i386.so.1` at `sysroot-x86`. Disk: the
finished 32-bit tree takes about 6 GB (toolchain, sysroot, packages, work), more while LLVM and Qt build; keep
20 GB free.

## The signing key

Packages and the repository index are signed with `keys/melon-signing.rsa`. It is deliberately not in
git. Copy it into `keys/` on the build machine and keep it private: whoever has it can publish packages
that every melon system trusts. Losing it means installed systems need a new public key before they
accept new packages.

## Sources

`sources/MANIFEST.tsv` lists every source file with its sha256 and where it comes from: upstream release
tarballs and git tags, and for older recipes the Ubuntu source archive (being replaced by upstream as recipes are
updated; new sources never come from Ubuntu, see AGENTS.md "Sources"). `scripts/fetch-sources.sh` downloads them; after
adding a source, run `scripts/make-manifest.py` and commit the manifest.
