# melon games launcher icon

A gamepad grown from cantaloupe rind (the same netting as the melon logo) with the melon's stem, tendril and vine leaf
on top. The d-pad is cut into the orange flesh and the four face buttons are melon seeds in a round of flesh.

| File | Use |
|---|---|
| `melon-games.svg` | master, green (use for 48 px and up) |
| `melon-games-small.svg` | simplified, thicker lines and no netting (the 16, 24 and 32 px PNGs come from it) |
| `melon-games-gold.svg`, `melon-games-gold-512.png` | the gold edition, for the survivor look (optional) |
| `melon-games-{16,24,32,48,64,128,256,512}.png` | transparent exports |
| `preview.png` | both editions on dark and light, plus the small sizes |

Regenerate: `python3 art/games.py out.svg [full|small] [green|gold]` (numpy only), then render with any SVG renderer.
Colours are the melon logo's (`#8bca5e` / `#5a9d45` / `#2f6431` rind, `#f7c47a` / `#f29a4c` flesh, `#0f2513` outline).
Install as `melon-games.svg` in `usr/share/icons/hicolor/scalable/apps/` and the PNGs under `hicolor/<size>x<size>/apps/`.
