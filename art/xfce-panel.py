#!/usr/bin/env python3
"""melon's taskbar background for XFCE (docs/xfce-theme.md).
  xfce-panel.py <file.png>     (recipes/melon-xfce/files/usr/share/melon/xfce/panel.png)
The panel tiles it: a thin light line along the top over the panel colour. The taskbar is two panels (the apps centred in a
transparent one on top of the full-width one), so the line can't come from the theme's CSS: the centred panel would draw it
a second time over the first."""
import sys
from PIL import Image
BG = (16, 21, 18, 255)        # #101512, a step darker than the windows (#1b221d)
LINE = (37, 43, 39, 255)      # white at 8% over it
im = Image.new('RGBA', (8, 64), BG)
for x in range(8): im.putpixel((x, 0), LINE)
im.save(sys.argv[1], optimize=True)
