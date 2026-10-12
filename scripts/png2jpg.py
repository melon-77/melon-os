#!/usr/bin/env python3
"""png2jpg.py FILE.png...: replace big photographic PNGs with high-quality JPEGs (quality 95, no chroma subsampling).

Used by recipes that ship wallpapers (breeze). A PNG stays as it is when Pillow is missing or the conversion fails,
so a build machine without Pillow still gets a working (just larger) package."""
import os, sys
try:
    from PIL import Image
except ImportError:
    print('png2jpg: Pillow is not installed, keeping the PNGs', file=sys.stderr)
    sys.exit(0)
for png in sys.argv[1:]:
    if not os.path.isfile(png):
        continue
    jpg = os.path.splitext(png)[0] + '.jpg'
    try:
        with Image.open(png) as im:
            im.convert('RGB').save(jpg, 'JPEG', quality=95, subsampling=0, optimize=True)
        os.remove(png)
    except Exception as e:
        print('png2jpg: %s: %s, keeping the PNG' % (png, e), file=sys.stderr)
        if os.path.exists(jpg):
            os.remove(jpg)
