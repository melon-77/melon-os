# Hosting the ISOs on real file hosts

Today the ISOs are release assets on GitHub (`releases/download/<tag>/...`). That works and costs nothing, but the link says
"github.com", and the owner wants downloads that look more polished. This note compares the free options, says what each needs
from the owner, and describes what is already prepared in the repository. (Written 7 October 2026; check a host's current limits
before relying on one.)

## What a download needs

- a file a browser can fetch with one click, at an address that doesn't change when a release is replaced;
- the **SHA-256 checksum** next to it (already on the site's download page and in `SHA256SUMS`);
- room for a few ISOs at once: the desktop one is about 835 MB, the console one about 396 MB, and the 32-bit editions will add two more;
- no money, and nothing that stops working when one person's laptop is off.

## The options

| Host | Cost and limits (from the host's own documentation) | What it is good for | What the owner has to do |
|---|---|---|---|
| **SourceForge** | Free for open source. Upload with rsync/SFTP to `frs.sourceforge.net`; files are replicated to SourceForge's worldwide mirror network. A single file over 10 GB is not mirrored (melon's are far below that). | The classic place for distribution ISOs. Real mirrors near the downloader, a project page with a file browser and a README, download counts. | Create a SourceForge account and a project (e.g. `melon-linux`), add an SSH public key to the account. Give the project name and username. |
| **Internet Archive** | Free. Up to 1 TB per item (they ask for under 500 GB and 1000 files). Makes a torrent for every item by itself. | A permanent second copy that people can also get by torrent. Good as the backup, and for old releases. | Create an account and run `ia configure` once on the machine that uploads (the keys stay on that machine). |
| **Cloudflare R2** | Free tier of 10 GB storage a month and 10 million reads, and no charge for download traffic. Public access through the free `r2.dev` address is rate-limited and "should only be used for development", so a real download address needs a custom domain that is on Cloudflare. | The most polished: `downloads.<your domain>/...`, fast, nothing to do with another project's page. | A Cloudflare account and **a domain name** (they cost roughly ten to fifteen dollars a year; not free). An R2 API token for the uploader. |
| GitHub release assets (now) | Free. | Release notes sit next to the files. | Nothing. |
| University and ISP mirrors | Free, run by others. | Reaching many people. | Mirror operators usually want an established project with a rsync source; ask once melon has users. Not now. |

## Recommendation

1. **SourceForge as the main download**, because it is free, it is built for ISOs and it spreads the files over mirrors. Needs only an account.
2. **The Internet Archive as the permanent backup and the torrent**, also free.
3. Keep **GitHub** for release notes and as a third source. Its asset links keep working.
4. If the owner wants the address to be the polish (`downloads.example.org`), add **Cloudflare R2 with a domain** later: it is the only
   option here that costs anything, and the 10 GB free storage holds all of melon's ISOs.

This is a recommendation; the owner decides, and signing up is the owner's to do (accounts, keys and domains belong to a person).

## What is already prepared

- **`scripts/upload-isos.sh [--dry-run] HOST RELEASE FILE...`** writes `SHA256SUMS` and uploads to `sourceforge`, `archive` or `r2`.
  Logins come from the environment or the host tool's own config (the variables are listed at the top of the script); nothing secret is in
  the repository. `--dry-run` prints the commands. It needs the real accounts to run for real, so it is only dry-run tested.
- **`site/download/`**, the download page: every file with its size, checksum and download link, how to check it and how to write it.
  The site's buttons point there, not at GitHub, so moving the files is a change to this one page.

## Adding a host to the site (when an account exists)

1. Run `scripts/upload-isos.sh` for the host with the release's ISOs.
2. In `site/download/index.html`, give each file a second link (the script prints the page address) and drop the sentence that says
   the files are served from GitHub. Keep the SHA-256 lines.
3. Put the same link in the release notes, update README and `docs/`, and open the pull request against `testing`
   (the site follows `AGENTS.md` "Website"; `scripts/publish-site.sh` publishes it).
4. When the 32-bit editions are published, add their rows (size, SHA-256) at the same time: the `melon 32-bit` row on the download page
   and in the ISO list on the home page says "coming" until then.
