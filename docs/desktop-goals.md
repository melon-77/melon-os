# Goal: the everyday desktop works out of the box

Adopted by the owner on 16 October 2026.

## Why

Some distributions give you a bare base and leave the rest to you: automounting USB drives, suspend on lid close,
Bluetooth, sound, a Wi-Fi picker, brightness keys. Each piece is easy once you know it, and a fresh install that lacks
one looks "broken" to whoever hasn't assembled a desktop before. melon wants to be as quick and small as the lean
distributions while working as well as the mainstream ones, so this is a goal, not a nice-to-have.

## The goal

On a fresh install of any melon desktop (Plasma on the 64-bit ISO, LXQt on the 32-bit one, and any desktop that is
added later as an online install), the everyday things work **with no setup after installing**: no package to add, no
service to enable, no file to edit. If one of them doesn't, that is a bug in melon, not a missing step in the user's
homework. Report it with `sudo melon-hwreport` attached.

## The checklist

"Checked by" says what tests the row today. `qemu-test.py desktop` and `desktop-install` are described in `AGENTS.md`
("Testing"). Rows marked *not yet* have no automated check; that is open work, not a claim that they're broken.

| Everyday thing | What must be true on a fresh install | Pieces (see "desktop plumbing" in `AGENTS.md`) | Checked by |
|---|---|---|---|
| USB drives | Plugging in a stick mounts it for the logged-in user, no password prompt | UDisks2, polkit, udevd | `qemu-test.py desktop` (a USB stick mounts through UDisks2) |
| Wi-Fi and wired network | The desktop lists networks and joins one (WPA2 at least) | NetworkManager and the desktop's network applet | `qemu-test.py desktop` (`nmcli` online, WPA2 on mac80211_hwsim radios) |
| Printing | A network printer is found and a page prints | CUPS | `qemu-test.py desktop` (a text file prints to a virtual IPP Everywhere printer) |
| Sound | Output and input devices appear, the volume control works | PipeWire and WirePlumber, started with the session | *not yet* (the test only records the installer music) |
| Bluetooth | The desktop pairs and connects a device | BlueZ and the desktop's Bluetooth applet | *not yet* |
| Suspend and lock | Closing the lid or pressing the power button does what the desktop's power settings say; the screen locks | elogind, the desktop's power management and screen locker | *not yet* |
| Brightness and volume keys | The keys on the keyboard change brightness and volume, with an on-screen indicator | the desktop's shortcuts and the kernel's keyboard and backlight drivers | *not yet* |
| Privilege prompts | Settings that need an administrator ask for a password instead of failing | polkit and the desktop's authentication agent | *not yet* |
| Notifications, file manager, screenshots | They exist and open | the desktop's own | *not yet* |
| Look | Fonts, cursor and theme are the same in Qt apps, GTK apps and Flatpak apps | `melon-desktop`, `melon-lxqt`, Flatpak's theme extensions | *not yet* |

## Rules that follow from the goal

- A desktop is not offered as an ISO edition until every row works on it, by hand at least, on a real machine or a
  VM. Extra desktops stay online installs (the size rule in `AGENTS.md`) and say on their page which rows they cover.
- A package in the desktop profile that needs a service ships it enabled in the profile's `.services` file, with a
  `./check` script when other services wait for it (rule 34 in `AGENTS.md`). "Install it and enable the service
  yourself" is not an acceptable state for the desktop profile.
- A change to the desktop plumbing (D-Bus, elogind, polkit, PipeWire, NetworkManager, BlueZ, CUPS, UDisks2) updates this
  table in the same pull request: it is where a reviewer sees what was supposed to keep working.
- A row that has no automated check gets one when the pieces under it change, as far as QEMU can show it (a
  virtual sound card, `hci_vhci` for Bluetooth, a lid switch through the QEMU monitor).

## What this goal does not cover

glibc-only programs (Steam, Discord, many AppImages) can't run natively on musl; Flatpak is how melon runs them, and
`melon-first-boot` offers the common ones. A rolling release can still break something between updates. Both are
trade-offs of melon's design, described in the README.
