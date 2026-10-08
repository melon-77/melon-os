#!/usr/bin/env python3
"""Leave out firmware that no melon kernel can ever load (AGENTS.md, "Smaller images").

usage: prune-firmware.py <firmware dir>...     (the usr/lib/firmware trees of linux-firmware's sub-packages)

Three kinds of blobs go:
  * firmware for drivers that recipes/linux-melon/config-melon leaves out, and for chips that only exist on other
    architectures (Arm system-on-chips: NXP DPAA2, Samsung MFC, Amlogic, Rockchip, the Wi-Fi of router chips, ...);
  * old iwlwifi API versions. The driver asks for the newest version it knows and falls back to older ones down to its
    own minimum, so only the newest few files of each chip family are ever read;
  * links that pointed at something removed here.
Nothing a PC, a laptop or the usual add-in cards can use is touched. When melon's kernel gains one of these drivers,
take its name out of DROP.
"""
import os, re, shutil, sys

# files and directories (relative to the firmware root) that go
DROP = [
    # drivers config-melon leaves out: server NICs, pro audio, DSL modems, embedded Wi-Fi
    'cxgb4', 'cxgb3', 'bnx2x', 'liquidio', 'asihpi', 'vxge', 'ueagle-atm', 'ath6k', 'wfx', 'rsi', 'rsi_91x.fw.zst',
    # system-on-chip video, display and GPU blocks (no such driver on x86)
    'dpaa2', 'amphion', 'cnm', 'amlogic', 'meson', 'arm', 'airoha', 'imx', 'rockchip', 'sun', 'powervr', 'qcom',
    # Wi-Fi on a system-on-chip's own bus: Qualcomm router and phone chips, MediaTek Chromebook and router SoCs
    'ath11k/IPQ8074', 'ath11k/IPQ6018', 'ath11k/IPQ5018', 'ath11k/WCN6750', 'ath10k/WCN3990', 'ath10k/QCA4019',
    'mediatek/mt8183', 'mediatek/mt8186', 'mediatek/mt8188', 'mediatek/mt8192', 'mediatek/mt8195',
]
DROP_FILES = re.compile(r'^(s5p-mfc[^/]*|mediatek/mt(7622|7981|7986|7988)[^/]*)$')

KEEP_NEWEST = 3   # iwlwifi versions kept per chip family; the c-numbered ones ('c101') are counted on their own
IWL = re.compile(r'^(iwlwifi-.+?)-(c?)(\d+)\.ucode(\.zst)?$')


def size(path):
    if os.path.isdir(path) and not os.path.islink(path):
        return sum(os.lstat(os.path.join(d, f)).st_size for d, _, fs in os.walk(path) for f in fs)
    return os.lstat(path).st_size


def remove(path):
    n = size(path)
    shutil.rmtree(path) if os.path.isdir(path) and not os.path.islink(path) else os.unlink(path)
    return n


def main(roots):
    removed = 0
    for root in roots:
        for rel in DROP:
            p = os.path.join(root, rel)
            if os.path.lexists(p):
                removed += remove(p)
        for d, _, fs in os.walk(root):
            for f in fs:
                p = os.path.join(d, f)
                if DROP_FILES.match(os.path.relpath(p, root)):
                    removed += remove(p)
        groups = {}
        for d, _, fs in os.walk(root):
            for f in fs:
                m = IWL.match(f)
                if m and not os.path.islink(os.path.join(d, f)):
                    groups.setdefault((d, m.group(1), m.group(2)), []).append((int(m.group(3)), f))
        for (d, _, _), files in groups.items():
            for _, f in sorted(files)[:-KEEP_NEWEST]:
                removed += remove(os.path.join(d, f))
    # links that lost their target; a link may point into another sub-package's tree, so look in all of them
    for root in roots:
        for d, _, fs in os.walk(root):
            for f in fs:
                p = os.path.join(d, f)
                if not os.path.islink(p):
                    continue
                t = os.path.normpath(os.path.join(os.path.relpath(d, root), os.readlink(p)))
                if not any(os.path.lexists(os.path.join(r, t)) for r in roots):
                    removed += remove(p)
    print('prune-firmware: left out %.1f MB' % (removed / 1e6))


if __name__ == '__main__':
    main(sys.argv[1:])
