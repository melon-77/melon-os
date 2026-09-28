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
because the host needs `/lib/ld-musl-x86_64.so.1` (see AGENTS.md, rule 18).

## The signing key

Packages and the repository index are signed with `keys/melon-signing.rsa`. It is deliberately not in
git. Copy it into `keys/` on the build machine and keep it private: whoever has it can publish packages
that every melon system trusts. Losing it means installed systems need a new public key before they
accept new packages.

## Sources

`sources/MANIFEST.tsv` lists every source file with its sha256 and where it comes from (mostly the
Ubuntu source archive, some git tags on GitHub). `scripts/fetch-sources.sh` downloads them; after
adding a source, run `scripts/make-manifest.py` and commit the manifest.
