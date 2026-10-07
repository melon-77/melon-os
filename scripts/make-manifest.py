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
    'ca-certificates': ('ca-certificates', 'main'), 'ovmf-generic': ('edk2', 'main')}
GIT = {  # tarball name prefix -> repo, tag pattern
    'apk-tools': ('https://github.com/alpinelinux/apk-tools', 'v{v}'),
    'argp-standalone': ('https://github.com/ericonr/argp-standalone', '{v}'),
    'elogind': ('https://github.com/elogind/elogind', 'v{v}'),
    'eudev': ('https://github.com/eudev-project/eudev', 'v{v}'),
    'musl-fts': ('https://github.com/void-linux/musl-fts', 'v{v}'),
    'musl-obstack': ('https://github.com/void-linux/musl-obstack', 'v{v}'),
    'ripgrep': ('https://github.com/BurntSushi/ripgrep', '{v}'),
    'vulkan-headers': ('https://github.com/KhronosGroup/Vulkan-Headers', 'v{v}'),
    'spirv-llvm-translator': ('https://github.com/KhronosGroup/SPIRV-LLVM-Translator', 'v{v}'),
    'gh': ('https://github.com/cli/cli', 'v{v}'),
    'libslirp': ('https://gitlab.freedesktop.org/slirp/libslirp', 'v{v}'),
    # GNOME's official GitHub mirror and seatd's author's mirror: gitlab.gnome.org and git.sr.ht were out of reach
    'pango': ('https://github.com/GNOME/pango', '{v}'),
    'seatd': ('https://github.com/kennylevinsen/seatd', '{v}'),
    'labwc': ('https://github.com/labwc/labwc', '{v}')}
URL = {  # upstream downloads (Ubuntu 26.04 has NetHack 3.6.7 and Cataclysm: DDA 0.H, Rust 1.93 and its make-dfsg drops make's doc/; host-rust.sh wants 1.98.1)
    'cataclysm-dda-0.9.1.tar.gz': 'https://github.com/CleverRaven/Cataclysm-DDA/archive/refs/tags/0.I-1.tar.gz',
    'cargo-1.98.1-x86_64-unknown-linux-gnu.tar.xz': 'https://static.rust-lang.org/dist/cargo-1.98.1-x86_64-unknown-linux-gnu.tar.xz',
    'rust-std-1.98.1-x86_64-unknown-linux-gnu.tar.xz': 'https://static.rust-lang.org/dist/rust-std-1.98.1-x86_64-unknown-linux-gnu.tar.xz',
    'rust-std-1.98.1-x86_64-unknown-linux-musl.tar.xz': 'https://static.rust-lang.org/dist/rust-std-1.98.1-x86_64-unknown-linux-musl.tar.xz',
    'rustc-1.98.1-x86_64-unknown-linux-gnu.tar.xz': 'https://static.rust-lang.org/dist/rustc-1.98.1-x86_64-unknown-linux-gnu.tar.xz',
    'nethack-500-src.tgz': 'https://www.nethack.org/download/5.0.0/nethack-500-src.tgz',
    'rustc-1.98.1-src.tar.xz': 'https://static.rust-lang.org/dist/rustc-1.98.1-src.tar.xz',
    'rustc-1.98.1-x86_64-unknown-linux-musl.tar.xz': 'https://static.rust-lang.org/dist/rustc-1.98.1-x86_64-unknown-linux-musl.tar.xz',
    'cargo-1.98.1-x86_64-unknown-linux-musl.tar.xz': 'https://static.rust-lang.org/dist/cargo-1.98.1-x86_64-unknown-linux-musl.tar.xz',
    'make-4.4.1.tar.gz': 'https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz',
    # self-hosting build tools (GNU tarballs checked against gnu-keyring.gpg, file against its .asc)
    'm4-1.4.21.tar.xz': 'https://ftp.gnu.org/gnu/m4/m4-1.4.21.tar.xz',
    'bison-3.8.2.tar.xz': 'https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.xz',
    'gawk-5.4.1.tar.xz': 'https://ftp.gnu.org/gnu/gawk/gawk-5.4.1.tar.xz',
    'gperf-3.3.tar.gz': 'https://ftp.gnu.org/gnu/gperf/gperf-3.3.tar.gz',
    'bc-1.08.2.tar.gz': 'https://ftp.gnu.org/gnu/bc/bc-1.08.2.tar.gz',
    'texinfo-7.3.tar.xz': 'https://ftp.gnu.org/gnu/texinfo/texinfo-7.3.tar.xz',
    'autoconf-2.73.tar.xz': 'https://ftp.gnu.org/gnu/autoconf/autoconf-2.73.tar.xz',
    'automake-1.19.tar.xz': 'https://ftp.gnu.org/gnu/automake/automake-1.19.tar.xz',
    'autoconf-archive-2024.10.16.tar.xz': 'https://ftp.gnu.org/gnu/autoconf-archive/autoconf-archive-2024.10.16.tar.xz',
    'flex-2.6.4.tar.gz': 'https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz',
    'perl-5.44.0.tar.xz': 'https://www.cpan.org/src/5.0/perl-5.44.0.tar.xz',
    'file-5.48.tar.gz': 'https://astron.com/pub/file/file-5.48.tar.gz',
    'cmake-4.4.3.tar.gz': 'https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3.tar.gz',
    'meson-1.12.1.tar.gz': 'https://github.com/mesonbuild/meson/releases/download/1.12.1/meson-1.12.1.tar.gz',
    'ninja-1.13.2.tar.gz': 'https://github.com/ninja-build/ninja/archive/refs/tags/v1.13.2.tar.gz',
    'git-2.56.0.tar.xz': 'https://www.kernel.org/pub/software/scm/git/git-2.56.0.tar.xz',
    'git-manpages-2.56.0.tar.xz': 'https://www.kernel.org/pub/software/scm/git/git-manpages-2.56.0.tar.xz',
    'nasm-3.01.tar.xz': 'https://www.nasm.us/pub/nasm/releasebuilds/3.01/nasm-3.01.tar.xz',
    'tcl8.6.17-src.tar.gz': 'https://prdownloads.sourceforge.net/tcl/tcl8.6.17-src.tar.gz',
    'rsync-3.5.1.tar.gz': 'https://download.samba.org/pub/rsync/src/rsync-3.5.1.tar.gz',
    'lz4-1.10.0.tar.gz': 'https://github.com/lz4/lz4/releases/download/v1.10.0/lz4-1.10.0.tar.gz',
    'xorriso-1.5.8.pl02.tar.gz': 'https://ftp.gnu.org/gnu/xorriso/xorriso-1.5.8.pl02.tar.gz',
    'mtools-4.0.49.tar.bz2': 'https://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.bz2',
    'dtc-1.8.1.tar.xz': 'https://www.kernel.org/pub/software/utils/dtc/dtc-1.8.1.tar.xz',
    'dwarves-1.32.tar.xz': 'https://fedorapeople.org/~acme/dwarves/dwarves-1.32.tar.xz',
    'scdoc-1.11.5.tar.gz': 'https://git.sr.ht/~sircmpwn/scdoc/archive/1.11.5.tar.gz',
    'itstool-2.0.7.tar.gz': 'https://github.com/itstool/itstool/archive/2.0.7/itstool-2.0.7.tar.gz',
    'mako-1.4.3.tar.gz': 'https://files.pythonhosted.org/packages/5a/09/e07c4b5579a79f4b16f8d4f29f6c54514ac787c4ad506b8c4f28a0e6b0bf/mako-1.4.3.tar.gz',
    'markupsafe-3.0.3.tar.gz': 'https://files.pythonhosted.org/packages/7e/99/7690b6d4034fffd95959cbe0c02de8deb3098cc577c67bb6a24fe5d7caa7/markupsafe-3.0.3.tar.gz',
    # LXQt 2.4 and its libraries (melon's 32-bit desktop): checked against Alpine's sha512sums; cairo against Void's,
    # libfm and menu-cache against Arch's
    'cairo-1.18.4.tar.xz': 'https://cairographics.org/releases/cairo-1.18.4.tar.xz',
    'fribidi-1.0.16.tar.xz': 'https://github.com/fribidi/fribidi/releases/download/v1.0.16/fribidi-1.0.16.tar.xz',
    'libdbusmenu-lxqt-0.4.0.tar.xz': 'https://github.com/lxqt/libdbusmenu-lxqt/releases/download/0.4.0/libdbusmenu-lxqt-0.4.0.tar.xz',
    'libexif-0.6.26.tar.bz2': 'https://github.com/libexif/libexif/releases/download/v0.6.26/libexif-0.6.26.tar.bz2',
    'libfm-1.3.2.tar.xz': 'https://downloads.sourceforge.net/pcmanfm/libfm-1.3.2.tar.xz',
    'libfm-qt-2.4.0.tar.xz': 'https://github.com/lxqt/libfm-qt/releases/download/2.4.0/libfm-qt-2.4.0.tar.xz',
    'liblxqt-2.4.0.tar.xz': 'https://github.com/lxqt/liblxqt/releases/download/2.4.0/liblxqt-2.4.0.tar.xz',
    'libqtxdg-4.4.0.tar.xz': 'https://github.com/lxqt/libqtxdg/releases/download/4.4.0/libqtxdg-4.4.0.tar.xz',
    'libsysstat-1.1.0.tar.xz': 'https://github.com/lxqt/libsysstat/releases/download/1.1.0/libsysstat-1.1.0.tar.xz',
    'lximage-qt-2.4.0.tar.xz': 'https://github.com/lxqt/lximage-qt/releases/download/2.4.0/lximage-qt-2.4.0.tar.xz',
    'lxqt-build-tools-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-build-tools/releases/download/2.4.0/lxqt-build-tools-2.4.0.tar.xz',
    'lxqt-config-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-config/releases/download/2.4.0/lxqt-config-2.4.0.tar.xz',
    'lxqt-globalkeys-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-globalkeys/releases/download/2.4.0/lxqt-globalkeys-2.4.0.tar.xz',
    'lxqt-menu-data-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-menu-data/releases/download/2.4.0/lxqt-menu-data-2.4.0.tar.xz',
    'lxqt-notificationd-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-notificationd/releases/download/2.4.0/lxqt-notificationd-2.4.0.tar.xz',
    'lxqt-panel-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-panel/releases/download/2.4.0/lxqt-panel-2.4.0.tar.xz',
    'lxqt-policykit-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-policykit/releases/download/2.4.0/lxqt-policykit-2.4.0.tar.xz',
    'lxqt-powermanagement-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-powermanagement/releases/download/2.4.0/lxqt-powermanagement-2.4.0.tar.xz',
    'lxqt-qtplugin-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-qtplugin/releases/download/2.4.0/lxqt-qtplugin-2.4.0.tar.xz',
    'lxqt-runner-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-runner/releases/download/2.4.0/lxqt-runner-2.4.0.tar.xz',
    'lxqt-session-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-session/releases/download/2.4.0/lxqt-session-2.4.0.tar.xz',
    'lxqt-themes-2.4.0.tar.xz': 'https://github.com/lxqt/lxqt-themes/releases/download/2.4.0/lxqt-themes-2.4.0.tar.xz',
    'lxqt-wayland-session-0.4.0.tar.xz': 'https://github.com/lxqt/lxqt-wayland-session/releases/download/0.4.0/lxqt-wayland-session-0.4.0.tar.xz',
    'menu-cache-1.1.0.tar.xz': 'https://downloads.sourceforge.net/lxde/menu-cache-1.1.0.tar.xz',
    'pavucontrol-qt-2.4.0.tar.xz': 'https://github.com/lxqt/pavucontrol-qt/releases/download/2.4.0/pavucontrol-qt-2.4.0.tar.xz',
    'pcmanfm-qt-2.4.0.tar.xz': 'https://github.com/lxqt/pcmanfm-qt/releases/download/2.4.0/pcmanfm-qt-2.4.0.tar.xz',
    'qterminal-2.4.0.tar.xz': 'https://github.com/lxqt/qterminal/releases/download/2.4.0/qterminal-2.4.0.tar.xz',
    'qtermwidget-2.4.0.tar.xz': 'https://github.com/lxqt/qtermwidget/releases/download/2.4.0/qtermwidget-2.4.0.tar.xz',
    'qtxdg-tools-4.4.0.tar.xz': 'https://github.com/lxqt/qtxdg-tools/releases/download/4.4.0/qtxdg-tools-4.4.0.tar.xz',
    'pyparsing-3.3.3.tar.gz': 'https://files.pythonhosted.org/packages/e4/11/b213bebff182584360cb8d17c72c1677fec5c5c228de439e63bcf8ab1c8f/pyparsing-3.3.3.tar.gz',
    'jinja2-3.1.6.tar.gz': 'https://files.pythonhosted.org/packages/df/bf/f7da0350254c0ed7c72f3e33cef02e048281fec7ecec5f032d4aac52226b/jinja2-3.1.6.tar.gz',
    'pyyaml-6.0.3.tar.gz': 'https://files.pythonhosted.org/packages/05/8e/961c0007c59b8dd7729d542c61a4d537767a59645b82a0b521206e1e25c2/pyyaml-6.0.3.tar.gz',
    'packaging-26.3.tar.gz': 'https://files.pythonhosted.org/packages/7d/fa/3944b40b07da9ce895c0e6303a5ab7d53da063554f534556b134a54d6093/packaging-26.3.tar.gz',
    'pexpect-4.9.0.tar.gz': 'https://files.pythonhosted.org/packages/42/92/cc564bf6381ff43ce1f4d06852fc19a2f11d180f23dc32d9588bee2f149d/pexpect-4.9.0.tar.gz',
    'ptyprocess-0.7.0.tar.gz': 'https://files.pythonhosted.org/packages/20/e5/16ff212c1e452235a90aeb09066144d0c5a6a8c0834397e03f5224495c4e/ptyprocess-0.7.0.tar.gz',
    'publicsuffix-20260924.dat': 'https://raw.githubusercontent.com/publicsuffix/list/a179a48c465e818cfd8d626691cb317985da87fb/public_suffix_list.dat',
    # step 2 (self-hosting): Go and its bootstrap (checked against go.dev), QEMU (signed by Michael Roth)
    'go1.27.1.src.tar.gz': 'https://go.dev/dl/go1.27.1.src.tar.gz',
    'go1.27.1.linux-amd64.tar.gz': 'https://go.dev/dl/go1.27.1.linux-amd64.tar.gz',
    'qemu-11.1.1.tar.xz': 'https://download.qemu.org/qemu-11.1.1.tar.xz',
    # QEMU's configure venv on a melon build machine (recipes/qemu): PyPI's wheels (PyPI's sha256, files identical to the
    # sdists that Gentoo's Manifest and Debian's .dsc list; wheel's PyPI attestation from pypa/wheel)
    'setuptools-84.0.0-py3-none-any.whl': 'https://files.pythonhosted.org/packages/95/9c/c510029fc6ef33a6275cd2c5d3cecd6613dfd6aa401d57c54f1c18852ccf/setuptools-84.0.0-py3-none-any.whl',
    'wheel-0.45.1-py3-none-any.whl': 'https://files.pythonhosted.org/packages/0b/2c/87f3254fd8ffd29e4c02732eee68a83a1d3c346ae39bc6822dcbcb697f2b/wheel-0.45.1-py3-none-any.whl',
    # NVIDIA: linux-firmware (signed by its maintainer), NVIDIA's module source and driver (NVIDIA's sha256, Arch's sha512)
    'linux-firmware-20260916.tar.xz': 'https://cdn.kernel.org/pub/linux/kernel/firmware/linux-firmware-20260916.tar.xz',
    'NVIDIA-kernel-module-source-615.71.09.tar.xz': 'https://download.nvidia.com/XFree86/NVIDIA-kernel-module-source/NVIDIA-kernel-module-source-615.71.09.tar.xz',
    'NVIDIA-Linux-x86_64-615.71.09.run': 'https://download.nvidia.com/XFree86/Linux-x86_64/615.71.09/NVIDIA-Linux-x86_64-615.71.09.run',
    # rpcgen (open-vm-tools' build runs it); upstream publishes no checksum: Alpine's sha512 and Debian's sha256 match
    'rpcsvc-proto-1.4.4.tar.xz': 'https://github.com/thkukuk/rpcsvc-proto/releases/download/v1.4.4/rpcsvc-proto-1.4.4.tar.xz'}

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
