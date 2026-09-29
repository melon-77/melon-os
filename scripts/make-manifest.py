#!/usr/bin/env python3
"""Write sources/MANIFEST.tsv: every source file the recipes use, with its sha256 and where it comes from,
so another build machine can download exactly the same files (scripts/fetch-sources.sh).

Columns: path (relative to sources/), sha256, method, argument
  apt    <srcpkg>=<version>     from `apt-get source --download-only` (Ubuntu source archive)
  pool   <url>                  a .deb from the Ubuntu archive pool
  url    <url>                  a tarball from the upstream site (versions Ubuntu doesn't have)
  git    <repo-url> <tag>       `git archive` of a tag (gzip output can differ, so only the tree is checked)
  inner  <archive> <member>     a file inside another archive (the upstream GCC tarball in Debian's gcc orig)
  link   <target>               a symlink to another entry
"""
import hashlib, os, re, subprocess, sys
M = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = f'{M}/sources'

def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()

# which Debian source package (and version) ships each file in sources/deb
owner = {}
for d in sorted(os.listdir(f'{S}/deb')):
    if not d.endswith('.dsc'): continue
    txt = open(f'{S}/deb/{d}', errors='replace').read()
    src = re.search(r'^Source: (\S+)', txt, re.M).group(1)
    ver = re.search(r'^Version: (\S+)', txt, re.M).group(1)
    files = re.search(r'^Files:\n((?: .*\n)+)', txt, re.M)
    for line in (files.group(1).splitlines() if files else []):
        owner[line.split()[-1]] = f'{src}={ver}'

POOL = {  # binary .debs: source package, component
    'linux-firmware': ('linux-firmware', 'main'), 'fonts-hack-ttf': ('fonts-hack', 'universe'),
    'fonts-noto-color-emoji': ('fonts-noto-color-emoji', 'main'), 'fonts-noto-core': ('fonts-noto', 'main'),
    'ca-certificates': ('ca-certificates', 'main')}
GIT = {  # tarball name prefix -> repo, tag pattern
    'apk-tools': ('https://github.com/alpinelinux/apk-tools', 'v{v}'),
    'argp-standalone': ('https://github.com/ericonr/argp-standalone', '{v}'),
    'elogind': ('https://github.com/elogind/elogind', 'v{v}'),
    'eudev': ('https://github.com/eudev-project/eudev', 'v{v}'),
    'musl-fts': ('https://github.com/void-linux/musl-fts', 'v{v}'),
    'musl-obstack': ('https://github.com/void-linux/musl-obstack', 'v{v}'),
    'vulkan-headers': ('https://github.com/KhronosGroup/Vulkan-Headers', 'v{v}')}
URL = {  # upstream downloads (Ubuntu 26.04 has NetHack 3.6.7; its make-dfsg drops make's doc/)
    'nethack-500-src.tgz': 'https://www.nethack.org/download/5.0.0/nethack-500-src.tgz',
    'make-4.4.1.tar.gz': 'https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz'}

rows, missing = [], []
def add(path, method, arg):
    full = f'{S}/{path}'
    rows.append((path, sha(full) if method != 'link' else '-', method, arg))

for sub in ('deb', 'firmware', 'fonts'):
    for f in sorted(os.listdir(f'{S}/{sub}')):
        p = f'{sub}/{f}'
        if sub == 'deb':
            if f.endswith(('.dsc', '.debian.tar.xz', '.debian.tar.gz', '.diff.gz', '.asc', '.deb')): continue
            if f in owner: add(p, 'apt', owner[f])
            else: missing.append(p)
        elif f.endswith('.deb'):
            pkg = f.split('_')[0]
            key = next((k for k in POOL if pkg == k or pkg.startswith(k + '-') or pkg.startswith('linux-firmware')), None)
            # linux-firmware's binary packages each have their own pool directory
            src, comp = (pkg, 'main') if pkg.startswith('linux-firmware') else POOL[key]
            pre = src[:4] if src.startswith('lib') else src[0]
            add(p, 'pool', f'http://archive.ubuntu.com/ubuntu/pool/{comp}/{pre}/{src}/{f}')

for f in sorted(os.listdir(S)):
    full = f'{S}/{f}'
    if os.path.isdir(full) and not os.path.islink(full): continue
    if f in ('MANIFEST.tsv',): continue
    if os.path.islink(full):
        t = os.readlink(full)
        if t.startswith(S + '/'): t = t[len(S) + 1:]
        rows.append((f, '-', 'link', t)); continue
    m = re.match(r'(.+?)-(\d[\w.]*)\.tar\.(gz|xz|bz2)$', f)
    if f in URL:
        add(f, 'url', URL[f])
    elif f.startswith('gcc-') and f.endswith('.tar.xz'):
        add(f, 'inner', f'deb/gcc-15_15.2.0.orig.tar.gz gcc-15-15.2.0/{f}')
    elif m and m.group(1) in GIT:
        repo, tag = GIT[m.group(1)]
        add(f, 'git', f'{repo} {tag.format(v=m.group(2))}')
    else:
        missing.append(f)

with open(f'{S}/MANIFEST.tsv', 'w') as out:
    out.write('# path\tsha256\tmethod\targument   (see scripts/make-manifest.py)\n')
    for r in rows: out.write('\t'.join(r) + '\n')
print(f'{len(rows)} entries written to sources/MANIFEST.tsv')
if missing: print('no known origin for:', ' '.join(missing)); sys.exit(1)
