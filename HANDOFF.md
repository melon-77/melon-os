# Handoff: carrying melon on with a plain Claude Code session

Written 16 October 2026 for a Claude Code session running **natively on melon** (the owner's PC now runs melon itself: Windows and WSL2 are
gone, and so are the Claude sessions that lived there). Read `AGENTS.md` first (build system, rules, product decisions that must not change),
then this file. `CLAUDE.md` points to both. Everything here is current as of this date; check `git log` and the open pull requests for what came after.

## Where things stand

- **melon 0.4 "Dudaim"** is the latest published release (Plasma desktop, GitHub + SourceForge, site points to it).
- **melon 0.5 "Esfahan" is not built, not released.** The owner's decision (12 October 2026): 0.5 ships with **Plasma** as the default desktop. Its branch is
  `claude/release-0.5-plasma`: the size round (PR #75: firmware split, one kernel copy on the ISO, JPEG wallpaper, zstd 19; every translation kept, see
  AGENTS.md "Desktop ISO size"), the superkernel (PR #76, `linux-melon` pkgrel 6 and `nvidia-open` pkgrel 2), the new calm GRUB menu (from #77), and the fix
  for missing standard folders (Downloads, Documents, ... now created by both installers; found 16 October 2026). Release name "Esfahan" was
  proposed, not confirmed by the owner: ask once when you publish.
  Measured before the Plasma decision (the combined branch with Wayfire): desktop ISO 512 MB, console ISO 251 MB (0.4: 794 / 380 MB); the Plasma
  desktop ISO will be larger (about 620 MB measured on 12 October). The owner accepted 612 MB desktop / 243 MB console as the size goal.
- **Wayfire + LXQt as the default desktop was tried and dropped** (owner, 12 October 2026: "lxqt kind of just hates working correctly without a
  decade of setup"). That work is PR #77 / branch `claude/release-0.5` (online `melon-lxqt` later). **XFCE is planned as the default for 0.6**:
  `docs/xfce-desktop-plan.md` on branch `claude/xfce-desktop` (needs Xorg and GTK 3 first: neither is in the tree). Not started beyond the plan.
- Open work to merge: PRs #75, #76, #77 target `testing`. Merge #75 and #76 with the Plasma branch's content in mind (the Plasma branch contains #75, #76 and
  the GRUB commit of #77, so merge that branch instead of the three separately); #77 stays open for the LXQt online install.
- Not yet verified anywhere: the Plasma desktop ISO's login and session (QEMU without KVM never gets Plasma up; the PC now boots melon natively, so
  `/dev/kvm` should work and `qemu-test.py desktop-install` can finally run for real), the NVIDIA modules on a real card, the 32-bit edition on the
  owner's MSI Wind U100.

## Build setup on melon

Follow `BUILDING.md`, "melon as the build machine": `doas apk upgrade && doas apk add git`, clone `https://github.com/melon-77/melon-os` (branch
`claude/release-0.5-plasma` for 0.5), `doas scripts/host-setup.sh`, then `doas scripts/build-everything.sh`.

- Never point `/lib/ld-musl-x86_64.so.1` at the sysroot on melon (rule 18); `host-setup.sh` handles the loader path.
- A native build from nothing takes many hours (toolchain, Qt, Plasma, Mesa and LLVM, the kernel is about an hour on 2 cores). Check whether the old
  `repo/`, `sources/` and `sysroot/` survived on a disk first (copy `keys/`, `sources/`, `repo/`, `sysroot/`; not `tools/` or `hosttools/`, BUILDING.md says why).
  Otherwise `scripts/fetch-sources.sh` re-downloads and checks every source against `sources/MANIFEST.tsv`.
- Run builds detached so a closed terminal does not kill them (WSL lost a run that way on 12 October): `setsid nohup scripts/build-everything.sh > logs/build.out 2>&1 &`
  or inside `tmux`. Poll the log sparingly; every poll costs the owner usage. `build-everything.sh` resumes where it stopped and skips what is in `repo/`.
- One session per checkout (AGENTS.md: the toolchain is tied to the checkout's path).
- ISOs and tests: `scripts/size-round.sh isos` then `scripts/size-round.sh tests`, or the commands in AGENTS.md "Testing". Read `LEAK:` lines after builds (rule 32).
  The desktop tests need KVM; check `ls -l /dev/kvm` before trusting a "Plasma did not start" result.
- Never install anything onto the build PC's own drive from a test or an installer run: the VM tests use disk *images* (`qemu-img create ... /tmp/disk.img`),
  and the installers must only ever be pointed at such an image.

## Signing key

`keys/melon-signing.rsa` (the everyday key) is on the owner's USB stick, used **in place**: `export MELON_SIGN_KEY=/path/on/the/stick/melon-signing.rsa`
(honoured by `build-everything.sh`, `melon-build` and `mkiso.sh`; never copy it into the repo or `keys/`). It must be plugged in to sign. The offline backup
key is not on any build machine. Never commit `keys/*.rsa`.

## Releasing (every meaningful change gets all four, the owner's rule of 7 October)

1. Build and test the ISOs (above). Release only what you tested; say plainly what could not be tested.
2. **GitHub release** (`gh` is freshly logged in as `xbfj` on the PC): tag `v0.5`, title `melon 0.5 “Esfahan”`, notes written for users (what is new, which file
   to download, `doas apk upgrade` for installed systems; the draft is `/mnt/project-files/releases/v0.5/NOTES.md` if the project files are reachable, else write
   them from the PR descriptions), both ISOs plus `SHA256SUMS`, marked Latest. Retitle the 0.4 release "(superseded)" with a link to the new one.
3. **SourceForge** (owner `melaboton`, project `melonl`): `SF_USER=melaboton SF_PROJECT=melonl scripts/upload-isos.sh sourceforge v0.5 <isos>`.
   The old upload key lived in WSL and is gone: make a new one (`ssh-keygen -t ed25519 -C melon-build-pc`) and ask the owner to add the public half under
   their SourceForge account's SSH keys. `docs/iso-hosting.md` has the details. Downloads on the site use SourceForge first and keep GitHub alongside.
4. **Website**: `scripts/site-release.py update --tag v0.5` (it reads sizes and checksums from the release), commit `site/`, pull request into `testing`, then
   `scripts/publish-site.sh` (or a PR into `gh-pages`) after it merges. Never edit the `<!-- release:... -->` regions by hand or `gh-pages` directly.
5. **Online package repo**: `scripts/publish-repo.sh` force-pushes `repo/<arch>/` to the `packages` branch, so start from its current contents when a machine has built
   only some packages. Mind rule 44 (publish melon-base 0.1-r36+ only with eudev 3.2.14-r2+) and rule 51 (a `linux-melon` change needs the matching `nvidia-open` build).

## The owner's standing rules (from the project memory)

- **Docs stay true:** a bug or significant change updates every `.md` it touches (README, AGENTS, BUILDING, `docs/`) and `site/` in the same PR. After each finished step,
  push to the melon-os GitHub and mention the update in the READMEs.
- **Pull requests go to `testing`**; `main` is protected; `testing` reaches `main` through its own PR (merge commit), then `testing` fast-forwards. Rebase, do not merge;
  squash-merge PRs. Delete a branch once its PR is merged and nothing references it (write its name and last commit down first).
- **No approval needed for simple, routine steps** (uploads, merging your own routine PR, link switches, cleanup, restarting a stuck tool): do them and report. Ask only for
  irreversible or goal-changing choices (a release's name, dropping a feature, anything about the signing key).
- **Size goal:** make the ISO smaller by removing only garbage nothing uses; never remove anything a general computer needs (Wi-Fi, graphics, audio, network drivers,
  translations). 612 MB desktop / 243 MB console was accepted. Anything moved off an ISO stays one `apk add` away.
- **Never write the hidden installer's name or the unlock commands' names anywhere** (files, comments, commits, tests, docs); only their hashes live in `zz-melon.sh`.
  Call `/usr/libexec/melon/.cold` by path in tests. Keep them out of `site/` and recordings too. Do not change the gauntlet or the product decisions in AGENTS.md "Installers".
- New sources come from upstream, not Ubuntu (AGENTS.md "Sources"); cloud sessions cannot reach most upstream hosts, so versions and checksums are fetched on the build machine.
- **Keep owner usage low:** run builds detached, do not poll them, write results in one short message, prefer the cheapest check that settles a question.

## Useful places

`docs/iso-hosting.md` (hosts), `docs/config.txt` (the owner's kernel and package choices), `docs/xfce-desktop-plan.md` (on `claude/xfce-desktop`), `docs/dinit-variant.md`
(runit stays melon's init, do not start a dinit port), `BUILDING.md`, the pinned issue #17 ("Owner <-> build machine"), issue #18 (niri + Noctalia idea).
