#!/usr/bin/env python3
"""Mockups of melon's XFCE look (docs/xfce-theme.md): layout A (top bar + dock) and B (one bottom taskbar).

Writes mock-a.html and mock-b.html into OUTDIR; render them at 1920x1080 with headless Chromium, e.g.
  chromium --headless --hide-scrollbars --allow-file-access-from-files --window-size=1920,1080 --screenshot=a.png mock-a.html
usage: xfce-mockup.py ICONS_DIR INTER_TTF MELON_SVG OUTDIR
  ICONS_DIR holds Papirus and Papirus-Dark (e.g. /usr/share/icons), INTER_TTF is InterVariable.ttf."""
import os, sys
if len(sys.argv) != 5: sys.exit(__doc__)
P, FONT, MELON, OUT = (os.path.abspath(a) for a in sys.argv[1:])
def ic(theme, size, cat, name):
    for t in (theme, 'Papirus'):
        f = f'{P}/{t}/{size}/{cat}/{name}.svg'
        if os.path.exists(f): return 'file://' + f
    print('missing', theme, size, cat, name, file=sys.stderr); return ''
app = lambda n, s='64x64': ic('Papirus-Dark', s, 'apps', n)
sym = lambda cat, n: ic('Papirus-Dark', 'symbolic', cat, n + '-symbolic')
folder = lambda n: ic('Papirus-Dark', '64x64', 'places', n)

CSS = f'''
@font-face {{ font-family: Inter; src: url(file://{FONT}); }}
:root {{ --win:#1b221d; --view:#141916; --head:#202823; --side:#18201b; --line:rgba(255,255,255,.07);
  --text:#e6ece3; --dim:#97a395; --accent:#6fbf4a; --acc-text:#0b150c; }}
* {{ box-sizing:border-box; margin:0; padding:0; }}
body {{ width:1920px; height:1080px; overflow:hidden; font:15px/1.35 Inter, sans-serif; color:var(--text);
  background:radial-gradient(1200px 800px at 18% 92%, rgba(111,191,74,.30), transparent 70%), radial-gradient(1100px 700px at 88% 8%, rgba(46,120,98,.32), transparent 70%), radial-gradient(900px 600px at 60% 55%, rgba(30,60,40,.35), transparent 70%), #0b100d; position:relative; -webkit-font-smoothing:antialiased; }}
.glass {{ background:rgba(16,21,18,.70); backdrop-filter:blur(24px) saturate(1.2); border:1px solid rgba(255,255,255,.08); }}
/* top bar */
.top {{ position:absolute; left:0; right:0; top:0; height:32px; display:flex; align-items:center; padding:0 10px;
  background:rgba(12,16,13,.78); backdrop-filter:blur(20px); font-size:14px; font-weight:500; }}
.top .menu {{ display:flex; align-items:center; gap:8px; padding:3px 10px; border-radius:7px; }}
.top .menu img {{ width:18px; height:18px; }}
.top .title {{ margin-left:14px; color:var(--dim); font-weight:500; }}
.top .clock {{ position:absolute; left:50%; transform:translateX(-50%); font-weight:600; }}
.tray {{ margin-left:auto; display:flex; gap:16px; align-items:center; }}
.tray img {{ width:16px; height:16px; opacity:.92; }}
/* dock */
.dock {{ position:absolute; bottom:12px; left:50%; transform:translateX(-50%); display:flex; gap:10px; padding:9px 12px;
  border-radius:22px; align-items:flex-end; }}
.dock .it {{ position:relative; width:56px; height:56px; display:flex; justify-content:center; }}
.dock img {{ width:52px; height:52px; }}
.dock .it.run::after {{ content:''; position:absolute; bottom:-6px; left:50%; width:5px; height:5px; margin-left:-2.5px;
  border-radius:50%; background:#dfe7dc; }}
.dock .it.act::after {{ background:var(--accent); width:16px; margin-left:-8px; border-radius:3px; }}
.dock .sep {{ width:1px; height:44px; background:rgba(255,255,255,.12); margin:0 4px 6px; }}
/* taskbar (layout B) */
.bar {{ position:absolute; left:0; right:0; bottom:0; height:52px; display:flex; align-items:center; justify-content:center;
  background:rgba(12,16,13,.80); backdrop-filter:blur(24px); border-top:1px solid rgba(255,255,255,.07); }}
.bar .apps {{ display:flex; gap:4px; }}
.bar .it {{ position:relative; width:46px; height:44px; border-radius:8px; display:flex; align-items:center; justify-content:center; }}
.bar .it img {{ width:30px; height:30px; }}
.bar .it.act {{ background:rgba(255,255,255,.08); }}
.bar .it.run::after {{ content:''; position:absolute; bottom:3px; width:6px; height:3px; border-radius:2px; background:#aab5a7; }}
.bar .it.act::after {{ width:16px; background:var(--accent); }}
.bar .right {{ position:absolute; right:14px; display:flex; gap:16px; align-items:center; font-size:13px; text-align:right; }}
.bar .right img {{ width:16px; height:16px; }}
/* windows (xfwm4 frame + GTK app) */
.win {{ position:absolute; border-radius:11px 11px 8px 8px; overflow:hidden; background:var(--win);
  box-shadow:0 22px 60px rgba(0,0,0,.55), 0 0 0 1px rgba(255,255,255,.07); }}
.win.inactive .tb {{ color:var(--dim); }}
.tb {{ height:40px; display:flex; align-items:center; justify-content:center; position:relative; background:var(--head);
  font-weight:600; font-size:14px; }}
.btns {{ position:absolute; right:10px; display:flex; gap:9px; }}
.btns span {{ width:22px; height:22px; border-radius:50%; background:rgba(255,255,255,.08); display:flex; align-items:center;
  justify-content:center; color:#cfd8cc; font-size:12px; line-height:1; }}
.win.inactive .btns span {{ background:rgba(255,255,255,.05); color:#7f8a7d; }}
.toolbar {{ height:46px; display:flex; align-items:center; gap:8px; padding:0 10px; background:var(--head);
  border-bottom:1px solid var(--line); }}
.tbtn {{ width:34px; height:32px; border-radius:7px; display:flex; align-items:center; justify-content:center; }}
.tbtn img {{ width:16px; height:16px; }}
.path {{ display:flex; gap:4px; margin-left:6px; }}
.crumb {{ padding:5px 12px; border-radius:7px; background:rgba(255,255,255,.06); font-weight:500; }}
.crumb.on {{ background:rgba(255,255,255,.12); }}
.search {{ margin-left:auto; width:220px; height:32px; border-radius:8px; background:rgba(0,0,0,.25); border:1px solid var(--line);
  display:flex; align-items:center; padding:0 10px; gap:8px; color:var(--dim); }}
.search img {{ width:14px; height:14px; opacity:.7; }}
.body {{ display:flex; height:calc(100% - 86px); }}
.side {{ width:210px; background:var(--side); padding:10px 8px; border-right:1px solid var(--line); }}
.side h4 {{ font-size:12px; color:var(--dim); font-weight:600; margin:10px 10px 6px; }}
.side div.r {{ display:flex; align-items:center; gap:10px; padding:7px 10px; border-radius:7px; }}
.side div.r img {{ width:16px; height:16px; }}
.side div.r.on {{ background:var(--accent); color:var(--acc-text); font-weight:600; }}
.side div.r.on img {{ filter:brightness(0) saturate(100%); opacity:.8; }}
.grid {{ flex:1; background:var(--view); padding:18px; display:grid; grid-template-columns:repeat(auto-fill,108px);
  grid-auto-rows:118px; gap:6px; align-content:start; }}
.f {{ display:flex; flex-direction:column; align-items:center; gap:6px; padding:8px 4px; border-radius:9px; font-size:13.5px; }}
.f img {{ width:64px; height:64px; }}
.f.sel {{ background:rgba(111,191,74,.18); box-shadow:inset 0 0 0 1px rgba(111,191,74,.55); }}
.status {{ position:absolute; bottom:0; left:210px; right:0; height:28px; font-size:12.5px; color:var(--dim); padding:5px 16px;
  background:var(--view); border-top:1px solid var(--line); }}
.term {{ background:#0f1411; height:calc(100% - 40px); padding:14px 16px; font:14.5px/1.5 'DejaVu Sans Mono', monospace; color:#d8e0d5; }}
.g {{ color:var(--accent); font-weight:bold; }} .d {{ color:#8b978a; }}
/* whisker menu (layout B) */
.menu-pop {{ position:absolute; bottom:64px; left:50%; transform:translateX(-50%); width:640px; height:600px; border-radius:16px;
  padding:22px; box-shadow:0 24px 70px rgba(0,0,0,.55); }}
.menu-pop .s {{ height:40px; border-radius:10px; background:rgba(0,0,0,.30); border:1px solid rgba(255,255,255,.09); display:flex;
  align-items:center; gap:10px; padding:0 14px; color:var(--dim); }}
.menu-pop .s img {{ width:16px; }}
.menu-pop h5 {{ margin:22px 6px 10px; font-size:13.5px; font-weight:600; }}
.apps6 {{ display:grid; grid-template-columns:repeat(6,1fr); gap:6px; }}
.apps6 div {{ display:flex; flex-direction:column; align-items:center; gap:7px; padding:10px 2px; border-radius:10px; font-size:12.5px;
  text-align:center; }}
.apps6 div:first-child {{ background:rgba(255,255,255,.07); }}
.apps6 img {{ width:46px; height:46px; }}
.recent div {{ display:flex; gap:12px; align-items:center; padding:8px 10px; border-radius:8px; font-size:13.5px; }}
.recent img {{ width:30px; }} .recent small {{ color:var(--dim); display:block; font-size:12px; }}
.menu-pop .foot {{ position:absolute; left:0; right:0; bottom:0; height:58px; border-top:1px solid rgba(255,255,255,.07);
  display:flex; align-items:center; padding:0 22px; gap:12px; }}
.avatar {{ width:30px; height:30px; border-radius:50%; background:#3a4a3e; display:flex; align-items:center; justify-content:center;
  font-weight:700; font-size:13px; }}
.foot .pw {{ margin-left:auto; display:flex; gap:8px; }} .foot .pw span {{ width:34px; height:34px; border-radius:8px;
  display:flex; align-items:center; justify-content:center; background:rgba(255,255,255,.06); }} .foot .pw img {{ width:16px; }}
/* notification (layout A) */
.note {{ position:absolute; right:14px; top:44px; width:380px; border-radius:14px; padding:14px 16px; display:flex; gap:14px;
  box-shadow:0 14px 40px rgba(0,0,0,.45); }}
.note img {{ width:40px; height:40px; }} .note b {{ display:block; font-size:14px; margin-bottom:2px; }}
.note span {{ color:#b7c2b4; font-size:13.5px; }}
'''

def thunar(x, y, w, h, inactive=False):
    places = [('user-home', 'Home', True), ('user-desktop', 'Desktop', False), ('folder-documents', 'Documents', False),
              ('folder-download', 'Downloads', False), ('folder-music', 'Music', False), ('folder-pictures', 'Pictures', False),
              ('folder-videos', 'Videos', False), ('user-trash', 'Trash', False)]
    side = ''.join(f'<div class="r{" on" if on else ""}"><img src="{sym("places", n)}">{t}</div>' for n, t, on in places)
    side += f'<h4>Devices</h4><div class="r"><img src="{sym("devices", "drive-removable-media")}">USB stick</div>'
    side += f'<div class="r"><img src="{sym("places", "network-workgroup")}">Network</div>'
    files = [('folder-grey-desktop', 'Desktop'), ('folder-grey-documents', 'Documents'), ('folder-grey-download', 'Downloads'),
             ('folder-grey-music', 'Music'), ('folder-grey-pictures', 'Pictures'), ('folder-grey-videos', 'Videos'),
             ('folder-grey-games', 'Games'), ('folder-grey-projects', 'Projects')]
    grid = ''.join(f'<div class="f{" sel" if i == 1 else ""}"><img src="{folder(n)}">{t}</div>' for i, (n, t) in enumerate(files))
    return f'''<div class="win{" inactive" if inactive else ""}" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px">
  <div class="tb">Home<div class="btns"><span>&#8211;</span><span>&#9633;</span><span>&#10005;</span></div></div>
  <div class="toolbar"><div class="tbtn"><img src="{sym("actions", "go-previous")}"></div><div class="tbtn"><img src="{sym("actions", "go-next")}"></div>
   <div class="path"><div class="crumb on">Home</div></div>
   <div class="search"><img src="{sym("actions", "system-search")}">Search Home</div></div>
  <div class="body"><div class="side">{side}</div><div class="grid">{grid}</div></div>
  <div class="status">8 folders &middot; &ldquo;Documents&rdquo; selected &middot; 212 GB free</div></div>'''

def terminal(x, y, w, h, inactive=True):
    return f'''<div class="win{" inactive" if inactive else ""}" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px">
  <div class="tb">Terminal<div class="btns"><span>&#8211;</span><span>&#9633;</span><span>&#10005;</span></div></div>
  <div class="term"><span class="g">melon@melon</span>:<span class="d">~</span>$ melonfetch<br>
  <span class="g">os</span>      melon 0.6 &ldquo;Fukui&rdquo;<br><span class="g">kernel</span>  7.0.0-melon<br>
  <span class="g">init</span>    runit<br><span class="g">desktop</span> Xfce 4.20<br><span class="g">theme</span>   melon (GTK + Qt)<br>
  <span class="g">icons</span>   Papirus-Dark<br><span class="g">font</span>    Inter 10<br><br>
  <span class="g">melon@melon</span>:<span class="d">~</span>$ <span style="background:#d8e0d5">&nbsp;</span></div></div>'''

tray = lambda: ''.join(f'<img src="{sym("status", n)}">' for n in
                       ('bluetooth-active', 'network-wireless-signal-excellent', 'audio-volume-high', 'battery-full'))
melon = 'file://' + MELON

def layout_a():
    dock_apps = [('org.xfce.thunar', 'run act'), ('firefox', 'run'), ('org.xfce.terminal', 'run'), ('org.xfce.mousepad', ''),
                 ('ristretto', ''), ('vlc', ''), ('steam', ''), ('org.xfce.settings.manager', '')]
    dock = ''.join(f'<div class="it {c}"><img src="{app(n)}"></div>' for n, c in dock_apps)
    dock += f'<div class="sep"></div><div class="it"><img src="{ic("Papirus-Dark", "64x64", "places", "user-trash")}"></div>'
    return f'''<div class="top"><div class="menu"><img src="{melon}">Applications</div><div class="title">Thunar</div>
  <div class="clock">Fri 16 Oct&nbsp;&nbsp;14:32</div><div class="tray">{tray()}</div></div>
  {terminal(1180, 130, 640, 500)}{thunar(150, 210, 1000, 660, inactive=False)}
  <div class="note glass"><img src="{ic("Papirus-Dark", "64x64", "devices", "drive-removable-media-usb")}"><div><b>USB stick connected</b><span>Opened in Files. Eject it from the sidebar.</span></div></div>
  <div class="dock glass">{dock}</div>'''

def layout_b():
    bar = [('__melon', ''), ('org.xfce.thunar', 'run'), ('firefox', 'run act'), ('org.xfce.terminal', 'run'),
           ('org.xfce.mousepad', ''), ('vlc', ''), ('steam', ''), ('org.xfce.settings.manager', '')]
    items = ''.join(f'<div class="it {c}"><img src="{melon if n == "__melon" else app(n, "48x48")}"></div>' for n, c in bar)
    fav = [('firefox', 'Firefox'), ('org.xfce.thunar', 'Files'), ('org.xfce.terminal', 'Terminal'), ('org.xfce.mousepad', 'Text Editor'),
           ('vlc', 'VLC'), ('steam', 'Steam'), ('ristretto', 'Image Viewer'), ('org.xfce.settings.manager', 'Settings'),
           ('system-software-install', 'Software'), ('calendar', 'Calendar'), ('org.xfce.taskmanager', 'Task Manager'),
           ('accessories-calculator', 'Calculator')]
    favs = ''.join(f'<div><img src="{app(n, "48x48")}">{t}</div>' for n, t in fav)
    rec = ''.join(f'<div><img src="{ic("Papirus-Dark", "48x48", "mimetypes", m)}"><span>{t}<small>{w}</small></span></div>'
                  for m, t, w in (('application-pdf', 'tax-return-2026.pdf', 'Documents &middot; yesterday'),
                                  ('image-jpeg', 'beach.jpg', 'Pictures &middot; Monday')))
    return f'''{thunar(330, 90, 1040, 640, inactive=True)}{terminal(1100, 240, 700, 470)}
  <div class="menu-pop glass"><div class="s"><img src="{sym("actions", "system-search")}">Type to search apps, files and settings</div>
   <h5>Pinned</h5><div class="apps6">{favs}</div><h5>Recent</h5><div class="recent">{rec}</div>
   <div class="foot"><div class="avatar">M</div>melon<div class="pw"><span><img src="{sym("actions", "system-lock-screen")}"></span>
   <span><img src="{sym("actions", "system-shutdown")}"></span></div></div></div>
  <div class="bar"><div class="apps">{items}</div><div class="right">{tray()}<div>14:32<br><span style="color:#97a395">16/10/2026</span></div></div></div>'''

for name, body in (('mock-a', layout_a()), ('mock-b', layout_b())):
    open(f'{OUT}/{name}.html', 'w').write(f'<!doctype html><html><head><meta charset="utf-8"><style>{CSS}</style></head><body>{body}</body></html>')
