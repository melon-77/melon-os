# Changelog

What changed for people who use melon. Releases are named after melon varieties (see AGENTS.md, "Releases");
installed systems update with `doas apk upgrade`.

## Unreleased

### XFCE's look, ready for 0.6 (16 October 2026)

- The packages that will make XFCE (the default desktop from 0.6) look modern instead of stock: a libadwaita-style GTK theme in
  melon's colours, melon's own window frames with minimise, maximise and close on every window, Papirus-Dark icons with grey
  folders, Inter and JetBrains Mono, and a bottom taskbar with the apps truly centred on the screen. Qt, GTK and Flatpak
  apps get the same theme, font and cursor, and a test checks it. XFCE itself isn't built yet, so nothing changes on 0.5 systems.
- melon's own start menu, `melon-menu`: a rounded card centred above the taskbar with search, pinned apps, recent files,
  "All apps", and settings, lock and power. The melon on the taskbar and the Super key open it; right click pins an app.
- Thunar without the old menu bar, with the path as buttons and your Documents, Downloads, Music, Pictures and Videos in the
  sidebar.

### Wayfire is the default desktop (12 October 2026)

- The desktop ISO and the desktop profile of both installers now use **Wayfire** (Wayland) with LXQt's panel, menu,
  launcher (Alt+Space), notifications and file manager (PCManFM-Qt), instead of KDE Plasma. It starts faster and makes
  the desktop ISO smaller: Plasma's packages and the KDE libraries only it used come to about 110 to 145 MB compressed,
  Wayfire and LXQt to about 15 MB (the 0.4 desktop ISO was 793.6 MB; the real size comes with the next ISO).
- Comes with it: swaylock (Super+L, after 10 minutes idle and before sleep), swayidle, grim screenshots (Print, into
  `~/Pictures/Screenshots`), nm-tray for networks, PipeWire volume in the panel and pavucontrol-qt, QTerminal
  (Super+Enter), the LXQt and wlroots desktop portals (file dialogs, screen sharing for Flatpak apps). The login screen
  is SDDM on labwc, as on the 32-bit edition.
- **Plasma is still there:** `sudo apk add melon-plasma` installs the set the ISO used to carry (Plasma, Dolphin, Konsole,
  Spectacle, Discover); pick "Plasma (Wayland)" on the login screen.
- `melon-boottime` now lists when `wayfire`, `lxqt-session`, `lxqt-panel` and `pcmanfm-qt` started, so you can see
  how long the desktop takes to appear.

### A new boot menu

- GRUB's menu is now plain and readable everywhere (both ISOs, installed systems, BIOS and UEFI): one dark colour, a
  small melon, the entries in Noto Sans, the selected one outlined in green, a countdown and one line of help. The live
  ISO's entries are shorter ("melon live", "run from RAM", "safe graphics", "serial console").
- The theme moved to melon-base, so console installs get it too. Systems that still show the old picture menu get the
  new one when melon-base is upgraded. Gauntlet survivors keep a gold edition of the same design.

### Calmer theming

- melon's palette is now an accent, not a coat of paint: dark grey-green surfaces, light text, melon green only for
  selection, focus, links and the lock screen's ring. The wallpaper is a quiet dark gradient; the melon itself appears
  only on the panel's menu button, the boot menu and the installer.
- The installer (Calamares) uses the same dark colours with a green accent, a plain melon logo and slides that describe
  the new desktop. The gauntlet's look follows.
- The gauntlet's rewards work on the new desktop: survivors get a gold accent, a gold lock screen ring and their
  wallpaper (and the full gold Plasma look if they add Plasma).

## 0.4 "Dudaim" (9 October 2026)

See the [release notes](https://github.com/melon-77/melon-os/releases/tag/v0.4).
