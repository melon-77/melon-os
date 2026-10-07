#!/usr/bin/env python3
"""Keeps the website's download pages in step with the latest release.

The release's facts live in site/releases.json (version, name, date, tag, the editions and, for each ISO that is published,
its file name, size, SHA-256 and link). This script writes them into the marked regions of site/index.html and
site/download/index.html, so a new release is two commands and no HTML:

    scripts/site-release.py update --tag v0.3          # read the release's assets from GitHub, fill releases.json, render
    scripts/site-release.py render                     # only re-render the pages from releases.json
    scripts/site-release.py check                      # exit 1 if the pages don't match releases.json (for reviews)

`update` takes the version, name and date from the release itself ("melon 0.3 “Cantaloupe”") unless you pass --version, --name
and --date. It matches the release's assets to the editions in releases.json by file name pattern; an edition with no asset is
shown as "coming" (the 32-bit editions, until their ISOs are in a release). Sizes and SHA-256 come from the release assets
(GitHub records both); set GITHUB_TOKEN if the API rate limit bites. To list other file hosts next to every file, put them in
"mirrors" in releases.json as {"label": "SourceForge", "url": "https://.../{file}"}.

Then commit site/ and open the pull request against testing; scripts/publish-site.sh publishes it. Standard library only.
"""
import argparse, fnmatch, html, json, os, re, sys, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "site", "releases.json")
PAGES = {"index": os.path.join(ROOT, "site", "index.html"), "download": os.path.join(ROOT, "site", "download", "index.html")}
MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]


def esc(s):
    return html.escape(s, quote=True)


def pretty_date(iso):
    y, m, d = (int(x) for x in iso.split("-"))
    return "%d %s %d" % (d, MONTHS[m - 1], y)


def mb(size):
    return "about %d MB" % round(size / 1e6)


def split_editions(data):
    """-> list of rows: ("file", edition, file) for published ISOs, ("coming", group) once per group of unpublished ones."""
    rows, seen = [], set()
    for ed in data["editions"]:
        f = data["files"].get(ed["id"])
        if f:
            rows.append(("file", ed, f))
            continue
        g = ed.get("coming_group")
        # one shared row while nothing of the group is published; single rows once part of it is
        if g and g in data.get("coming_groups", {}) and not any(data["files"].get(e["id"]) for e in data["editions"] if e.get("coming_group") == g):
            if g not in seen:
                seen.add(g)
                rows.append(("coming", data["coming_groups"][g], None))
        else:
            rows.append(("coming", {"id": ed["id"], "title": ed["title"], "variant": ed["variant"],
                                    "blurb": "Not published yet.", "desc": "Not published yet."}, None))
    return rows


def links(data, f):
    out = [("GitHub" if "github.com" in f["url"] else "download", f["url"])]
    for m in data.get("mirrors", []):
        out.append((m["label"], m["url"].replace("{file}", f["file"])))
    return out


def render_home_isos(data):
    li = []
    for kind, ed, f in split_editions(data):
        size = "" if kind == "coming" or not f else " &middot; " + mb(f["size"])
        name = '<div class="iso-name"><b>%s</b><span>%s%s</span></div>' % (esc(ed["title"]), esc(ed["variant"]).replace(" · ", " &middot; "), size)
        if kind == "file":
            primary = ed["id"] == data["editions"][0]["id"]
            li.append('    <li>\n      %s\n      <p>%s</p>\n      <a class="btn small%s" href="download/#%s">Get it</a>\n    </li>'
                      % (name, esc(ed["blurb"]), "" if primary else " ghost", esc(ed["id"])))
        else:
            li.append('    <li class="soon">\n      %s\n      <p>%s</p>\n      <span class="tag">coming</span>\n    </li>' % (name, esc(ed["blurb"])))
    return "\n" + "\n".join(li) + "\n  "


def render_files(data):
    li = []
    for kind, ed, f in split_editions(data):
        if kind == "file":
            ls = links(data, f)
            also = ""
            if len(ls) > 1:
                also = '\n        <span class="also">also from ' + ", ".join('<a href="%s">%s</a>' % (esc(u), esc(l)) for l, u in ls[1:]) + "</span>"
            primary = ed["id"] == data["editions"][0]["id"]
            li.append(
                '    <li id="%s">\n      <div class="iso-name"><b>%s</b><span>%s &middot; {size:,} bytes ({mb})</span></div>\n'
                '      <p>%s <code>%s</code><br>\n        <span class="sum">SHA-256 <code>%s</code></span>%s</p>\n'
                '      <a class="btn small%s" href="%s" download>Download</a>\n    </li>'
                .replace("{size:,}", format(f["size"], ","))
                .replace("{mb}", mb(f["size"]))
                % (esc(ed["id"]), esc(ed["title"]), esc(ed["variant"]).replace(" · ", " &middot; "), esc(ed["desc"]), esc(f["file"]), esc(f["sha256"]),
                   also, "" if primary else " ghost", esc(ls[0][1])))
        else:
            li.append('    <li class="soon" id="%s">\n      <div class="iso-name"><b>%s</b><span>%s</span></div>\n      <p>%s</p>\n      <span class="tag">coming</span>\n    </li>'
                      % (esc(ed["id"]), esc(ed["title"]), esc(ed["variant"]).replace(" · ", " &middot; "), esc(ed["desc"])))
    return "\n" + "\n".join(li) + "\n  "


def render_note(data):
    hosts = ["GitHub"] + [m["label"] for m in data.get("mirrors", [])]
    base = 'The checksums are also in <a href="https://github.com/%s/releases/download/%s/SHA256SUMS"><code>SHA256SUMS</code></a>.' % (data["repo"], data["tag"])
    if len(hosts) == 1:
        return ("These files are served from GitHub for now. When melon moves its downloads to other file hosts, this page lists them next to each file, "
                "and the checksums stay the same wherever you get a file. " + base)
    return "Every file is on %s; the checksum is the same wherever you get it. %s" % (" and ".join([", ".join(hosts[:-1]), hosts[-1]]) if len(hosts) > 2 else " and ".join(hosts), base)


def regions(data):
    first = data["files"].get(data["editions"][0]["id"])
    desktop_file = first["file"] if first else "melon-desktop.iso"
    title = "melon %s &ldquo;%s&rdquo;" % (esc(data["version"]), esc(data["name"]))
    return {
        "index": {
            "eyebrow": "%s &middot; %s &middot; x86_64" % (title, pretty_date(data["date"])),
            "btn": "Download melon %s" % esc(data["version"]),
            "btn2": "Download melon %s" % esc(data["version"]),
            "isos": render_home_isos(data),
        },
        "download": {
            "eyebrow": "%s &middot; %s" % (title, pretty_date(data["date"])),
            "files": render_files(data),
            "note": render_note(data),
            "file1": esc(desktop_file), "file2": esc(desktop_file), "file3": esc(desktop_file),
        },
    }


def apply(text, regs, page):
    for key, value in regs.items():
        pat = re.compile(r"(<!-- release:%s -->)(.*?)(<!-- /release:%s -->)" % (key, key), re.S)
        if not pat.search(text):
            sys.exit("%s: marker <!-- release:%s --> ... <!-- /release:%s --> not found" % (page, key, key))
        text = pat.sub(lambda m: m.group(1) + value + m.group(3), text)
    return text


def render(write):
    data = json.load(open(DATA))
    stale = []
    for page, regs in regions(data).items():
        old = open(PAGES[page]).read()
        new = apply(old, regs, PAGES[page])
        if new != old:
            stale.append(PAGES[page])
            if write:
                open(PAGES[page], "w").write(new)
    return stale


def fetch(url):
    req = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json", "User-Agent": "melon-site-release"})
    if os.environ.get("GITHUB_TOKEN"):
        req.add_header("Authorization", "Bearer " + os.environ["GITHUB_TOKEN"])
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def update(a):
    data = json.load(open(DATA))
    repo = a.repo or data["repo"]
    rel = json.loads(fetch("https://api.github.com/repos/%s/releases/tags/%s" % (repo, a.tag)))
    m = re.match(r"melon (\d+(?:\.\d+)*) .(.+).", rel.get("name") or "")
    data["repo"], data["tag"] = repo, a.tag
    data["version"] = a.version or (m.group(1) if m else sys.exit("can't read the version from the release name %r: pass --version, --name and --date" % rel.get("name")))
    data["name"] = a.name or (m.group(2) if m else sys.exit("pass --name"))
    data["date"] = a.date or rel["published_at"][:10]
    sums = {}
    for asset in rel["assets"]:
        if asset["name"] == "SHA256SUMS":
            for line in fetch(asset["browser_download_url"]).decode().splitlines():
                p = line.split()
                if len(p) == 2:
                    sums[p[1].lstrip("*")] = p[0]
    data["files"] = {}
    for ed in data["editions"]:
        hits = [x for x in rel["assets"] if fnmatch.fnmatch(x["name"], ed["match"])]
        if len(hits) > 1:
            sys.exit("%d assets match %s for %s: %s" % (len(hits), ed["match"], ed["id"], ", ".join(x["name"] for x in hits)))
        if hits:
            x = hits[0]
            digest = (x.get("digest") or "").removeprefix("sha256:") or sums.get(x["name"])
            if not digest:
                sys.exit("no SHA-256 for %s (no digest on the asset, no SHA256SUMS line)" % x["name"])
            if x["name"] in sums and sums[x["name"]] != digest:
                sys.exit("SHA256SUMS and GitHub's digest disagree for %s" % x["name"])
            data["files"][ed["id"]] = {"file": x["name"], "size": x["size"], "sha256": digest, "url": x["browser_download_url"]}
    json.dump(data, open(DATA, "w"), indent=2, ensure_ascii=False)
    open(DATA, "a").write("\n")
    print("releases.json: melon %s “%s” %s, published: %s; coming: %s" % (
        data["version"], data["name"], data["date"], ", ".join(data["files"]) or "none",
        ", ".join(e["id"] for e in data["editions"] if e["id"] not in data["files"]) or "none"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("render")
    sub.add_parser("check")
    u = sub.add_parser("update")
    u.add_argument("--tag", required=True)
    u.add_argument("--repo")
    u.add_argument("--version")
    u.add_argument("--name")
    u.add_argument("--date")
    a = ap.parse_args()
    if a.cmd == "update":
        update(a)
        a.cmd = "render"
    stale = render(write=a.cmd == "render")
    if a.cmd == "check" and stale:
        sys.exit("stale (run scripts/site-release.py render): " + ", ".join(os.path.relpath(p, ROOT) for p in stale))
    if a.cmd == "render":
        print("updated: " + (", ".join(os.path.relpath(p, ROOT) for p in stale) or "nothing, the pages already match"))


main()
