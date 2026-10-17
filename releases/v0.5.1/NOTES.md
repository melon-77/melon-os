# melon 0.5.1 “Esfahan”

melon releases are named after melon varieties, in alphabetical order. 0.5.1 fixes what broke in 0.5.

## Get it

Download `melon-desktop-YYYYMMDD-x86_64.iso`, write it to a USB stick with Rufus, balenaEtcher, Ventoy or `dd`, and boot from it. Try melon in the live session, then install it from there.

<sub>`melon-YYYYMMDD-x86_64.iso` is the console edition: no desktop, just a terminal, for people who barely want a computer. Check either download against `SHA256SUMS`.</sub>

Already on melon? Run `doas apk upgrade` and restart.

## What's new

- **Password prompts work again.** In 0.5, every window that asked for your password failed: adding Flathub, installing from Discover, the "Update melon" button and unlocking the screen.
- **Flathub is ready from the start.** It comes set up, so Discover and the first-login app offer never ask for a password just to add it.
- **Your time zone and clock format.** Date & Time in System Settings now lists every time zone and saves your choice, "Set date and time automatically" works, and 12- or 24-hour time follows Region & Language. Websites that check your browser stop tripping over a clock stuck on UTC.
- **No more Flatpak warning** about a missing `flatpak` user when you install apps.
- **Steadier Wi-Fi** on desktops installed with the console installer: NetworkManager is the only thing managing the card now.
- Everything new in 0.5 is still here: smaller downloads, the welcome window, updates without the terminal, your usual folders, Linux 7.0 and NVIDIA support.

## Coming next

- More polish for the desktop, and fixes from your reports.
- More apps in the online package repository.

## Problems?

If something on your hardware doesn't work, run `sudo melon-hwreport` and attach the file it writes to an issue on GitHub.
