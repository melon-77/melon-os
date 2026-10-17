# melon 0.5 “Esfahan”

melon releases are named after melon varieties, in alphabetical order.

## Get it

Download `melon-desktop-YYYYMMDD-x86_64.iso`, write it to a USB stick with Rufus, balenaEtcher, Ventoy or `dd`, and boot from it. Try melon in the live session, then install it from there.

<sub>`melon-YYYYMMDD-x86_64.iso` is the console edition: no desktop, just a terminal, for people who barely want a computer. Check either download against `SHA256SUMS`.</sub>

Already on melon? Run `doas apk upgrade` and restart.

## What's new

- **Smaller downloads.** The desktop ISO is down to about 626 MB from 794 MB, and the console ISO to about 247 MB from 380 MB. Nothing you need was removed: every translation is still there, and rarely needed firmware is one `apk add` away.
- **A welcome window** greets you on first boot, with your folders, updates, games and links in one place.
- **Updates without the terminal.** An "Update melon" button, and a notice in the tray when updates are waiting.
- **Your usual folders are there.** Downloads, Documents, Pictures, Music and Videos are created for every new user.
- **Newer kernel and NVIDIA support.** Linux 7.0 with melon's tuned kernel, plus NVIDIA's open driver for recent NVIDIA cards.
- **A calmer boot menu**, and melon now says "melon 0.5" everywhere, with a sharper logo.
- **Fixes:** the power-mode switch works from the terminal (`powerprofilesctl`), and apps can add their own device rules again.
- **melon now builds itself.** This release was built and tested entirely on a PC running melon.

## Coming next

- More polish for the desktop, and fixes from your reports.
- More apps in the online package repository.

## Problems?

If something on your hardware doesn't work, run `sudo melon-hwreport` and attach the file it writes to an issue on GitHub.
