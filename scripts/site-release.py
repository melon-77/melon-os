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
(GitHub records both); set GITHUB_TOKEN if the API rate limit bites. To list other file hosts next to the files, put them in
"mirrors" in releases.json as {"label": "SourceForge", "url": "https://.../{file}"} ({tag} works too), or, for a host that only
has some releases, {"label": "SourceForge", "tags": {"i686-20261007": "https://.../i686-20261007/{file}/download"}}.
Add "primary": true to make a mirror the download button and drop GitHub's link for the files it carries (the page then also links that
host's SHA256SUMS: {file} is replaced by SHA256SUMS).

A second release (the 32-bit pre-release) goes in the same file; name the editions it carries:

    scripts/site-release.py update --tag i686-20261007 --editions x86-desktop,x86-console

That fills only those editions (each remembers its tag, and a pre-release is shown as a "test build") and leaves the release's
name, version and date, and the other editions, alone. A plain `update --tag <tag>` leaves editions that have their own tag alone too.

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


def mirror_url(m, f):
    """-> the mirror's link for file f, or None when the mirror doesn't carry f's release."""
    tpl = m["tags"].get(f.get("tag")) if "tags" in m else m.get("url")
    return tpl.replace("{file}", f["file"]).replace("{tag}", f.get("tag", "")) if tpl else None


def links(data, f):
    """-> [(label, url)], the first one being the download button. A mirror marked "primary" that carries the file comes first and
    takes the place of GitHub, which is then not offered for that file; files the primary host doesn't carry (yet) keep GitHub."""
    gh = [("GitHub" if "github.com" in f["url"] else "download", f["url"])]
    mirrors = [(m, mirror_url(m, f)) for m in data.get("mirrors", [])]
    mirrors = [(m, u) for m, u in mirrors if u]
    prim = [(m["label"], u) for m, u in mirrors if m.get("primary")]
    rest = [(m["label"], u) for m, u in mirrors if not m.get("primary")]
    return prim + rest if prim else gh + rest


def variant(ed):
    return esc(ed["variant"]).replace(" · ", " &middot; ") + (" &middot; test build" if ed.get("prerelease") else "")


def render_home_isos(data):
    li = []
    for kind, ed, f in split_editions(data):
        size = "" if kind == "coming" or not f else " &middot; " + mb(f["size"])
        name = '<div class="iso-name"><b>%s</b><span>%s%s</span></div>' % (esc(ed["title"]), variant(ed) if kind == "file" else esc(ed["variant"]).replace(" · ", " &middot; "), size)
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
                % (esc(ed["id"]), esc(ed["title"]), variant(ed), esc(ed["desc"]), esc(f["file"]), esc(f["sha256"]),
                   also, "" if primary else " ghost", esc(ls[0][1])))
        else:
            li.append('    <li class="soon" id="%s">\n      <div class="iso-name"><b>%s</b><span>%s</span></div>\n      <p>%s</p>\n      <span class="tag">coming</span>\n    </li>'
                      % (esc(ed["id"]), esc(ed["title"]), esc(ed["variant"]).replace(" · ", " &middot; "), esc(ed["desc"])))
    return "\n" + "\n".join(li) + "\n  "


def sums_link(data, tag, files):
    """The SHA256SUMS link for a release: the primary host's copy when it carries that release, else GitHub's."""
    for m in data.get("mirrors", []):
        if m.get("primary"):
            f = next((x for x in files if x.get("tag", data["tag"]) == tag), None)
            u = mirror_url(m, dict(f, file="SHA256SUMS")) if f else None
            if u:
                return u
    return "https://github.com/%s/releases/download/%s/SHA256SUMS" % (data["repo"], tag)


def render_note(data):
    files = list(data["files"].values())
    tags = []
    for f in files:
        tag = f.get("tag", data["tag"])
        if tag not in tags:
            tags.append(tag)
    sums = ", ".join('<a href="%s"><code>SHA256SUMS</code></a>%s'
                     % (esc(sums_link(data, tg, files)), " (melon %s)" % esc(data["version"]) if tg == data["tag"] else " (%s)" % esc(tg)) for tg in tags or [data["tag"]])
    base = "The checksums are also in %s." % sums
    prim = [m["label"] for m in data.get("mirrors", []) if m.get("primary") and any(mirror_url(m, f) for f in files)]
    others = [m["label"] for m in data.get("mirrors", []) if not m.get("primary") and any(mirror_url(m, f) for f in files)]
    if prim:
        everywhere = all(any(mirror_url(m, f) for m in data["mirrors"] if m.get("primary")) for f in files)
        head = "These files are downloaded from %s." % prim[0] if everywhere else "Most of these files are downloaded from %s; the rest are still on GitHub for now." % prim[0]
        tail = " Files are also on %s." % " and ".join(others) if others else ""
        return head + tail + " The checksum is the same wherever you get a file. " + base
    if not others:
        return ("These files are served from GitHub for now. When melon moves its downloads to other file hosts, this page lists them next to each file, "
                "and the checksums stay the same wherever you get a file. " + base)
    return "Every file is on GitHub; some are also on %s. The checksum is the same wherever you get a file. %s" % (" and ".join([", ".join(others[:-1]), others[-1]]) if len(others) > 1 else others[0], base)


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
    only = a.editions.split(",") if a.editions else None
    known = [e["id"] for e in data["editions"]]
    for i in only or []:
        if i not in known:
            sys.exit("no edition %r in releases.json (editions: %s)" % (i, ", ".join(known)))
    if only is None:
        m = re.match(r"melon (\d+(?:\.\d+)*) .(.+).", rel.get("name") or "")
        data["repo"], data["tag"] = repo, a.tag
        data["version"] = a.version or (m.group(1) if m else sys.exit("can't read the version from the release name %r: pass --version, --name and --date" % rel.get("name")))
        data["name"] = a.name or (m.group(2) if m else sys.exit("pass --name"))
        data["date"] = a.date or rel["published_at"][:10]
        # editions with a release of their own (the 32-bit pre-release) are filled by `--editions`, not by this
        take = [e for e in data["editions"] if "tag" not in e]
    else:
        take = [e for e in data["editions"] if e["id"] in only]
    sums = {}
    for asset in rel["assets"]:
        if asset["name"] == "SHA256SUMS":
            for line in fetch(asset["browser_download_url"]).decode().splitlines():
                p = line.split()
                if len(p) == 2:
                    sums[p[1].lstrip("*")] = p[0]
    for ed in take:
        data["files"].pop(ed["id"], None)
        hits = [x for x in rel["assets"] if fnmatch.fnmatch(x["name"], ed["match"])]
        if len(hits) > 1:
            sys.exit("%d assets match %s for %s: %s" % (len(hits), ed["match"], ed["id"], ", ".join(x["name"] for x in hits)))
        if only is not None:
            ed["tag"] = a.tag
            if rel.get("prerelease"):
                ed["prerelease"] = True
            else:
                ed.pop("prerelease", None)
        if hits:
            x = hits[0]
            digest = (x.get("digest") or "").removeprefix("sha256:") or sums.get(x["name"])
            if not digest:
                sys.exit("no SHA-256 for %s (no digest on the asset, no SHA256SUMS line)" % x["name"])
            if x["name"] in sums and sums[x["name"]] != digest:
                sys.exit("SHA256SUMS and GitHub's digest disagree for %s" % x["name"])
            data["files"][ed["id"]] = {"file": x["name"], "size": x["size"], "sha256": digest, "tag": a.tag, "url": x["browser_download_url"]}
    # keep the files in edition order
    data["files"] = {e["id"]: data["files"][e["id"]] for e in data["editions"] if e["id"] in data["files"]}
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
    u.add_argument("--editions", help="comma-separated edition ids this release carries (a second release, e.g. the 32-bit pre-release)")
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
