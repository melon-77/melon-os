# melon 0.5.2 “Esfahan”

melon releases are named after melon varieties, in alphabetical order. 0.5.2 fixes the graphical installer.

## Get it

Download `melon-desktop-YYYYMMDD-x86_64.iso`, write it to a USB stick with Rufus, balenaEtcher, Ventoy or `dd`, and boot from it. Try melon in the live session, then install it from there.

<sub>`melon-YYYYMMDD-x86_64.iso` is the console edition: no desktop, just a terminal, for people who barely want a computer. Check either download against `SHA256SUMS`.</sub>

Already on melon? Run `doas apk upgrade` and restart.

## What's new

- **The installer on the desktop works again.** Installing from the 0.5 and 0.5.1 desktop ISOs stopped with "Command unsquashfs finished with exit code 2". It now installs.
- **Stronger passwords.** The installer saved passwords in a way that only counted their first 8 characters, and the text console refused them. Passwords are now stored properly. If you installed with 0.5 or 0.5.1, change your password once with `passwd` to get the same.
- **No more "Could not unmount revokefs-fuse filesystem"** when you install Flatpak apps.
- Everything from 0.5.1 is still here: working password prompts, Flathub ready from the start, time zones and 12- or 24-hour time.

## Coming next

- More polish for the desktop, and fixes from your reports.
- More apps in the online package repository.

## Problems?

If something on your hardware doesn't work, run `sudo melon-hwreport` and attach the file it writes to an issue on GitHub.
