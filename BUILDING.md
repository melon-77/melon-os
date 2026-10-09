# Building melon on your own machine

Everything melon ships is built by the scripts in this repo, so any x86_64 machine with Ubuntu 24.04
can build it. On an 8-core/16-thread CPU a full build (toolchain, base system, Plasma desktop, ISOs)
takes roughly 8–12 hours; on 2 cores it takes days.

## Windows + WSL2 build machine

Set up and used for the round-2 ISOs on a Windows 10 IoT Enterprise LTSC 2021 laptop (build 19044, Ryzen 7 5800U, 16 threads,
32 GB RAM, no Microsoft Store). Windows 11 is the same without the workarounds marked *(Windows 10)*. WSL2 needs Windows 10
build 19041 or newer.

1. Turn WSL2 on. This needs an **administrator** PowerShell and a **reboot**:

       wsl --install --no-distribution

   *(Windows 10 without the Store)* if that isn't available, `dism /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart`
   and the same for `VirtualMachinePlatform`. After the reboot, still as administrator, get the current WSL (the inbox
   `wsl.exe` is old and only lists "Ubuntu"): `wsl --update --web-download`. The new program is
   `C:\Program Files\WSL\wsl.exe`; use that path in a shell that started before the update.

2. Install Ubuntu 24.04 (no Store needed with `--web-download`; `--no-launch` skips the first-run user prompt, the build runs as root anyway):

       wsl --install -d Ubuntu-24.04 --web-download --no-launch

3. Give WSL enough memory and never let it idle out. Qt and KWin need about 1 GB per compile job, and WSL only gets half the
   RAM by default. Create `%UserProfile%\.wslconfig`:

       [wsl2]
       memory=24GB        # 32 GB machine: leave Windows ~8 GB (use 12GB on 16 GB)
       swap=16GB
       processors=16
       vmIdleTimeout=-1

   then `wsl --shutdown`. Use `JOBS=12` for builds on 16 threads; with 16 GB of RAM or less, `JOBS=10`.

4. Inside Ubuntu, `/etc/wsl.conf` (the `[boot]` part is already there). **`appendWindowsPath=false` is required**: WSL appends the
   Windows `PATH`, which has entries with spaces (`/mnt/c/Program Files/...`), and a build step that expands `$PATH`
   unquoted breaks on them (`host-rust.sh` did: `env: 'Files/GnuPG/bin:...': No such file`, exit 127; fixed, other scripts may follow):

       [boot]
       systemd=true
       [interop]
       appendWindowsPath=false

   Also keep scripts that you write on Windows with LF line endings (Windows tools write CRLF, and `bash` then fails on the last line).

5. Build inside the Linux file system (`~/melon`), not under `/mnt/c`: it is many times faster, and keep the helper scripts there too. Start long builds detached (`setsid nohup script >log 2>&1 < /dev/null &`) and keep one
   `wsl.exe -d Ubuntu-24.04 -- sleep infinity` open, so WSL keeps its VM while no terminal is attached.

6. **Do not let Windows sleep.** Modern/connected standby freezes the WSL2 VM: after it wakes, every new `wsl` command hangs
   (even `wsl --shutdown` and `wsl --exec /bin/true`) and the builds inside do not progress, although `wsl -l -v` says
   "Running". This happened twice on AC power. Keep the machine plugged in, don't close the lid, set sleep to "never"
   (Settings → System → Power), or run something that holds a "system required" request while it builds. To recover
   without rebooting, in an administrator PowerShell: kill `wsl`, `wslhost`, `wslrelay`, `wslservice`, `vmwp` and `vmmem`
   with `Stop-Process -Force`, then `Start-Service WslService`. The Linux disk is intact; builds are resumable (below).

7. **There is no KVM on Windows 10, or on a CPU Windows can't nest on.** `/dev/kvm` may exist and QEMU still says
   "failed to initialize kvm: No such device" (`wsl` itself prints "Nested virtualization is not supported on this machine").
   `qemu-test.py` then uses software emulation (AGENTS.md, "Testing"): the console install and boot tests pass in about a minute,
   the Plasma desktop tests were not usable on that laptop.

8. A USB stick (or any drive Windows doesn't auto-mount in WSL) needs a mount: `D: /mnt/d drvfs defaults,nofail 0 0` in `/etc/fstab`.
   The signing key can stay on it and be used in place (`MELON_SIGN_KEY`, "The signing key" below). Keep the offline **backup** key
   (AGENTS.md, "Signing keys and rotation") off any drive that is plugged into the build machine.

9. **Two sessions on one machine: one checkout each.** A checkout's `tools/` and `sysroot/` are tied to its path, and a
   second session switching branches under a running build breaks both. Give each its own worktree:

       git -C ~/melon worktree add ~/melon-r2 <branch>
       cd ~/melon-r2
       for e in ~/melon/sources/*; do [ "${e##*/}" = MANIFEST.tsv ] || ln -sfn "$e" sources/; done   # shared downloads
       ln -sfn ~/melon/hosttools hosttools                                                            # shared host tools
       git --git-dir=$HOME/melon/.git archive origin/packages x86_64 | tar -x -C repo                 # its own copy of the online repo
       echo ~/melon-r2/sysroot/usr/lib >> /etc/ld-musl-x86_64.path                                    # rule 18: the loader must find this tree's libraries
       /root/melon-r2/scripts/toolchain.sh      # its own cross toolchain (about 15 minutes with 12 jobs); absolute path

   Then `melon-build` the recipes you changed: it installs their `makedepends` from `repo/x86_64`, but a dev package's
   private requires (kmod-dev needs zlib, zstd, xz, openssl) are not apk dependencies, so first `apk add` the base and
   plumbing packages of `build-everything.sh` and their `-dev` packages into `sysroot/` (see `BASE` and `PLUMBING` there).
   A KDE recipe (`breeze`) has no `makedepends` at all: the full build relies on every earlier package being in the sysroot.
   So `apk add` (with `--root sysroot --keys-dir keys/trusted --repository repo/x86_64/Packages.adb`) the Qt and KDE packages
   that `python3 scripts/gen-kde-recipes.py` lists before it, their `-dev` packages, and the X11 and XCB client libraries
   (`libx*`, `xcb*`; KF6 WindowSystem's CMake config needs X11). The packages in the online repository were built on a
   machine whose checkout was `/home/hi/melon`, and some of their CMake files name that path (`KF6::WindowSystem` includes
   `/home/hi/melon/sysroot/usr/include`: "includes non-existent path"). Until you have rebuilt those packages yourself, let the
   path resolve: `mkdir -p /home/hi/melon && ln -s ~/melon-r2/sysroot /home/hi/melon/sysroot && ln -s ~/melon/hosttools /home/hi/melon/hosttools`.

Measured on that laptop (12 jobs): `host-setup.sh` about 35 minutes for apt and the 2 GB of sources, a few minutes for GRUB, Python and Rust,
then 50 minutes for the host Qt; cross toolchain 14 to 21 minutes; `linux-melon` 22 minutes; `grub` 3 minutes; small recipes seconds;
the console ISO 14 seconds and the desktop ISO about a minute (they install the published packages from `repo/`).

Sources that failed here and what worked: `ftp.gnu.org` did not answer at all (`https://mirrors.kernel.org/gnu/<project>/<file>` did;
check the sha256 against `sources/MANIFEST.tsv` before using a file from a mirror), and a pinned Ubuntu `.deb` that had left the archive
pool (`ovmf-generic_2025.11-3ubuntu7.2_all.deb`) was still on `https://launchpad.net/ubuntu/+archive/primary/+files/<name>`.
`apt` prints "Job for systemd-binfmt.service failed" in WSL: harmless.

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
