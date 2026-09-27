# Stage 2 and 3 plan: the melon desktop ISO

Built from the owner's answers in `docs/desktop-config.txt`. Decisions made with the owner:

- **Firefox:** ship the desktop ISO first with Firefox from Flathub (offered on first boot). Build a
  native melon Firefox afterwards and swap it in. Native Firefox needs Rust, clang and Node for melon,
  and about 25 GB of free disk, which is more than the usual build container has.
- **Other distros' repos:** Alpine is offered as an **opt-in, tagged** repo (`@alpine`) from the installer.
  apk only uses it for packages requested as `name@alpine`. Void isn't offered: it uses xbps, not apk.
- **Updates hosting:** decided later. For now packages come from the ISO.
- **Size:** the hard limit is 4 GB, the goal is under 2 GB. Speed and efficiency come first.

## What goes on the desktop ISO

| Area | Choice |
|---|---|
| Devices / sessions | eudev, elogind, D-Bus, polkit (duktape backend), Linux-PAM (SDDM and the screen locker need it) |
| Graphics | Mesa: radeonsi + RADV (AMD), iris + ANV (Intel), nouveau (NVIDIA GL; NVK once Rust exists), llvmpipe. LLVM for radeonsi/llvmpipe |
| Display | Wayland + Xwayland (no separate X11 session) |
| Desktop | Plasma **minimal**: KWin, plasma-workspace, plasma-desktop, systemsettings, kscreen, powerdevil, plasma-nm, plasma-pa, bluedevil, breeze, kscreenlocker, polkit agent, xdg-desktop-portal-kde |
| Login | SDDM with a melon theme |
| Apps | Dolphin, Konsole, Alacritty (needs Rust), nano, Spectacle, System Monitor, Discover (Flatpak backend) |
| From Flathub on first boot | Firefox, VLC, Steam, Prism Launcher |
| Gaming | GameMode, MangoHud, controller udev rules, Flatpak |
| Hardware | Bluetooth (BlueZ), printing (CUPS), fingerprint (fprintd), power-profiles-daemon, UPower, UDisks2 |
| Audio | PipeWire + WirePlumber |
| Network | NetworkManager + wpa_supplicant (D-Bus build) |
| Look | melon dark (Breeze + green/orange accents), Noto fonts + Hack, bottom panel; logo on wallpaper, splash, SDDM, GRUB theme, launcher icon |
| Installers | Calamares + gauntlet (3 s Next delay, wrong answer = back 10, same music, finish rewards: certificate, exclusive wallpaper, melonfetch badge). The quick console installer gets the desktop profile. Both: dual boot, optional LUKS encryption, zram + swap file, US/English/UTC, optional Alpine repo |

## Build layers (in order)

1. **Plumbing:** libffi, expat, pcre2, glib, dbus, libcap, Linux-PAM, eudev, elogind, duktape, polkit,
   util-linux rebuilt with udev support, kmod already done.
2. **Graphics stack:** libdrm, wayland, wayland-protocols, libxkbcommon + xkeyboard-config, libevdev,
   mtdev, libinput, pixman, libpng, libjpeg-turbo, freetype, harfbuzz, fontconfig, fonts, libepoxy, the
   X11 client libraries Xwayland needs, LLVM, SPIRV-Tools/glslang, Mesa, Xwayland.
3. **Qt 6:** a host Qt build first (cross-compiling Qt needs host tools of the same version), then
   qtbase, qtdeclarative, qtshadertools, qtsvg, qtwayland, qttools, qt5compat, qtmultimedia for melon.
4. **KDE Frameworks 6**, then **Plasma 6** and the apps above.
5. **Services:** NetworkManager, BlueZ, PipeWire + WirePlumber, CUPS, fprintd, power-profiles-daemon,
   UPower, UDisks2, xdg-desktop-portal, SDDM.
6. **Flatpak** (ostree, bubblewrap, xdg-dbus-proxy, appstream, libarchive, gpgme, fuse3), GameMode, MangoHud.
7. **Installers:** Calamares + kpmcore, gauntlet QML module, melon branding; console installer desktop
   profile, dual boot, LUKS (adds a small initramfs for encrypted installs), zram + swap file.
8. **Theme and branding**, desktop ISO assembly, QEMU tests (the VM uses virtio-gpu, so a Plasma
   session is tested with llvmpipe/virgl).
9. Later: Rust for melon, then native Firefox, Alacritty and NVK.

Expected compile time on the 2-core build container: 2–4 days in total. The build scripts delete each
build tree after a successful package to stay inside the disk budget.
