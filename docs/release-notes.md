# Release notes: the format

The owner's format (16 October 2026): short, about what users get, nothing about how it was built or tested.
Testing details go in the pull requests, not in the release text. Four parts, in this order:

1. **Title:** `melon <version> “<melon variety>”`, then one line: melon releases are named after melon varieties, in alphabetical order.
2. **Get it:** the desktop ISO's file name, how to write it to a USB stick (Rufus, balenaEtcher, Ventoy or `dd`) and boot it.
   In small text (`<sub>`): the console ISO is for people who barely want a computer, and `SHA256SUMS` to check downloads.
   One line for existing installs: `doas apk upgrade`, then restart.
3. **What's new:** each change as a benefit in plain words (bold lead-in, one sentence), then **Coming next**: what the
   next release is planned to bring.
4. **Problems?** `sudo melon-hwreport`, and attach the file it writes to an issue.

Leave out package versions, test lists and build-machine details unless a user has to act on them.
`releases/v0.5/NOTES.md` is the first release written this way.
