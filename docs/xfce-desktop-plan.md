# XFCE as melon's default desktop: plan

The owner's decision (12 October 2026): after a hardware test of the 0.5 test ISO, LXQt on Wayfire "kind of just hates working
correctly without a decade of setup". The default 64-bit desktop becomes **XFCE** (the owner picked it from XFCE, MATE and COSMIC).
This file is the work list. Nothing here is built yet; the cloud session that wrote it cannot reach any upstream host
(archive.xfce.org, download.gnome.org, x.org, gitlab.freedesktop.org all time out), so **versions and checksums must be taken on the
build machine** (AGENTS.md, "Sources": upstream tarballs, signature or published checksum, cross-checked against Alpine's APKBUILD).

## What stays

- Wayfire + LXQt stay as recipes and become an **online install** (`melon-lxqt`, like `melon-plasma`). The 32-bit edition keeps LXQt on labwc.
- SDDM stays the display manager. Its greeter runs on labwc today; on the 64-bit desktop it can keep doing so (Wayland greeter, X11 session
  afterwards), which avoids touching the greeter. Check that SDDM starts an X11 session through `xinit`/`Xorg` for `xfce.desktop`
  (`[General] DisplayServer` stays `wayland` for the greeter, sessions come from `/usr/share/xsessions`).
- Calamares (Qt) and the gauntlet are untouched; `melon-survivor-look` and the rewards need an XFCE branch (see "melon-xfce").
- Everything in "Installers: do not break these product decisions" of AGENTS.md still holds.

## What is missing (nothing of it exists in `recipes/` today)

The tree has Xwayland and the X client libraries (libx11, libxcb, libxi, ...) but **no Xorg server and no GTK**. Build order:

1. **Xorg**: `xorg-server` (meson; `-Dxorg=true -Dxwayland=false`, glamor on, no systemd), `xf86-input-libinput`, `xinit`, `xauth`, `xrdb`,
   `setxkbmap`, `xset`, `xmodmap`, `xrandr`, `xsetroot`, `xdpyinfo`, `xprop` and the libraries those pull in (libICE, libSM, libXt, libXmu,
   libXpm, libXv, libXres, libXaw only if something asks). Video: the server's `modesetting` driver covers Intel/AMD/Nouveau/virtio
   through KMS; add `xf86-video-vesa` only if a test machine needs it. Rule 32 applies (pkg-config sysroot answers: `-Dxkb_dir`,
   `-Dxkb_bin_dir=/usr/bin`, `-Dxorg-module-dir`, `-Ddefault-font-path`), so read the `LEAK:` lines.
2. **GTK 3**: `at-spi2-core` (with `atk`), `libepoxy` (exists), `gdk-pixbuf` (exists, needs `librsvg` for SVG icons: `librsvg` is Rust, so
   `options=(rust)`; or build gdk-pixbuf's SVG loader off and ship the icon theme as PNG+SVG through librsvg anyway), `libxkbcommon`
   (exists), `iso-codes`, `gsettings-desktop-schemas`, `dconf`, `libwnck3`, `libxklavier`, `libnotify`, `gtk3` (meson, `-Dintrospection=false`,
   X11 and Wayland backends, no cloudprint), `adwaita-icon-theme` (or `elementary-xfce-icon-theme`), `gtksourceview4`/`vte` for Mousepad and the terminal.
   Qt-free: none of this needs the host Qt (rule 20). Host tools needed on the build machine: `glib-compile-resources` (melon's, rule 18),
   `gdk-pixbuf-pixdata`, `gtk-update-icon-cache` (host or melon's).
3. **XFCE 4.20 or newer** (check archive.xfce.org for the current stable series; 4.22 aimed at GTK4 and Wayland, so stay on the GTK3 series
   unless the release is plainly stable): `libxfce4util`, `xfconf`, `libxfce4ui`, `libxfce4windowing`, `garcon`, `exo`, `thunar`, `tumbler`,
   `xfce4-panel`, `xfce4-settings`, `xfwm4`, `xfdesktop`, `xfce4-session`, `xfce4-appfinder`, `xfce4-terminal`, `xfce4-notifyd`,
   `xfce4-power-manager`, `xfce4-pulseaudio-plugin`, `xfce4-screensaver` (or `light-locker`), `xfce4-whiskermenu-plugin` (a real application
   menu, CMake), `mousepad`, `ristretto`, `xfce4-taskmanager`, `thunar-volman`, `gvfs` (trash and removable media in Thunar; UDisks2 exists),
   `xfce-polkit` (or `polkit-gnome`), `network-manager-applet` (+ `libnma`), `blueman` or `xfce4-bluetooth` equivalent (BlueZ exists).
   `xfce4-dev-tools` is only needed if a tarball needs `autoreconf`; release tarballs do not.
4. **`melon-xfce`** (melon's defaults, like `melon-lxqt` and `melon-wayfire`): the look is designed in `docs/xfce-theme.md` (GTK theme, window frames, icons, fonts, panel layouts, one look across Qt, GTK and Flatpak); the dark grey-green look from AGENTS.md ("calm, the palette as
   accents": `#121714`, `#1b221d`, melon green `#6fbf4a` for selection, focus and links; the melon only on the menu button), panel layout,
   `/etc/xdg/xfce4/xfconf/xfce-perchannel-xml/*.xml` defaults, the wallpaper `usr/share/wallpapers/melon`, `/usr/share/xsessions/xfce.desktop`,
   Qt following the GTK look (`QT_QPA_PLATFORMTHEME`, Calamares excepted), gold survivor look via xfconf (accent colour, lock screen),
   `melon-survivor-look`'s XFCE branch (it has KDE and LXQt/Wayfire branches), `melon-set-wallpaper`'s XFCE branch.

## Wiring (each is a small edit; find them with `rg -n "wayfire|lxqt"`)

- `scripts/desktop-packages.txt`: replace the Wayfire/LXQt names with `melon-xfce` and its dependencies (rule 33: things nothing links against).
- `scripts/build-everything.sh`: the order (Xorg, GTK, XFCE before `melon-xfce`; `check-order.sh` must pass); `scripts/gen-xfce-recipes.py` for the
  regular tarballs, in the style of `gen-lxqt-recipes.py` (rule 57: generated recipes are edited in the generator).
- `scripts/mkiso.sh` / `scripts/qemu-test.py`: tests look for `xfwm4` and `xfce4-panel` (not `wayfire`/`lxqt-panel`) and take a screenshot to look at.
- The Wayfire/LXQt recipes move to the online package list (AGENTS.md "What goes on an ISO": extra desktops are always online installs).
- `AGENTS.md` (desktop row, rules 62 and the Roadmap), `README.md`, `BUILDING.md`, `site/` and the 0.5 release notes say XFCE; the gold-look
  and rewards sections keep working through `melon-survivor-look`.

## Size and speed

GTK 3 + XFCE is not smaller than LXQt on disk (expect about 100 to 200 MB more compressed than the 512 MB Wayfire ISO measured on
12 October 2026, because of GTK, librsvg and the icon theme), but its idle memory is small and it needs no tuning. If the ISO passes the
owner's earlier line (612 MB / 243 MB were accepted) keep it; do not drop translations or drivers to save bytes (rule 61).

## Test plan

`qemu-test.py desktop` and `desktop-install` need KVM for the session part (software emulation never gets Plasma or the Wayfire session up
either), so the real check is the owner's USB boot: login screen, XFCE session, panel, Wi-Fi, sound, Thunar with a USB stick, the installer.

## Release order (the owner, 12 October 2026)

melon 0.5 ships with **Plasma** as the default desktop (branch `claude/release-0.5-plasma`: the size round (#75), the superkernel (#76) and
the new GRUB menu from #77; the Wayfire/LXQt desktop is not in it). XFCE becomes the default in 0.6, built from this plan. The Wayfire/LXQt
work stays in the tree on `claude/release-0.5` (PR #77) for the online `melon-lxqt` install.

## Boot time

XFCE does not change the part of boot the owner measured (3.76 s on 12 October 2026, firmware to the login screen): the kernel, stage 1, runit and
SDDM are the same. What changes is login to desktop: an X11 session needs `Xorg` first, then `xfwm4`, `xfce4-panel` and `xfdesktop`, which is
lighter than Plasma and heavier than a bare compositor. Measure it, don't guess: `melon-boottime` for boot, and the time from the password to a
usable panel on the same machine, before and after.
