#!/usr/bin/env python3
"""Record the console installer in QEMU as a video (the one on the website).

  record-installer.py record <console-iso> <outdir>      boot the ISO, start the installer by its path, answer its
                                                        questions through the virtual keyboard, save screenshots
  record-installer.py build  <outdir> <out.mp4>           join the screenshots into an MP4 with the sound QEMU captured

Needs qemu-system-x86_64, ffmpeg, tesseract-ocr and python3-pil (apt). Without KVM (WSL2 on Windows 10 has none) QEMU runs
in software: about 4 minutes. The installer's title is drawn in big letters on the intro, so `build` blacks it out in
every frame: never publish a video that wasn't built this way (AGENTS.md, "Website"). ffmpeg runs with -nostdin, so
this can run from a script that reads its commands from stdin.
"""
import os, pickle, re, socket, subprocess, sys, threading, time, hashlib
from PIL import Image, ImageChops

KEYS = {' ': 'spc', '\n': 'ret', '/': 'slash', '.': 'dot', '-': 'minus', '_': 'shift-minus'}
ANSWERS = [(r'[Dd]isk to', 'vda'), (r'What to', 'base'), (r'ostname', 'melon'), (r'[Rr]oot pass', 'melonroot'), (r'again', None),
           (r'user name', 'melon'), (r'Passw\w* for', 'melonuser'), (r'[Aa]lpin', 'n'), (r'Rust', 'n'), (r'ncrypt', 'n'), (r'YES', 'YES')]


def kvm_works():
    """/dev/kvm can exist and still fail ("No such device" in WSL2 on Windows 10): start a bare QEMU to find out"""
    try:
        p = subprocess.Popen(['qemu-system-x86_64', '-accel', 'kvm', '-machine', 'none', '-display', 'none', '-S', '-nodefaults', '-monitor', 'none'],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(2); ok = p.poll() is None; p.kill(); return ok
    except OSError: return False


class VM:
    def __init__(self, iso, out):
        self.out, self.sock = out, f'{out}/mon.sock'
        os.makedirs(out, exist_ok=True)
        disk = f'{out}/disk.img'
        subprocess.run(['qemu-img', 'create', '-f', 'raw', disk, '12G'], check=True, stdout=subprocess.DEVNULL)
        accel = ['-enable-kvm', '-cpu', 'host'] if kvm_works() else ['-accel', 'tcg,thread=multi', '-cpu', 'max']
        cmd = ['qemu-system-x86_64'] + accel + ['-m', '3072', '-smp', '4', '-display', 'none', '-vga', 'std', '-no-reboot',
               '-audiodev', f'wav,id=snd0,path={out}/audio.wav', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0',
               '-netdev', 'user,id=n0', '-device', 'virtio-net-pci,netdev=n0', '-monitor', f'unix:{self.sock},server,nowait',
               '-cdrom', iso, '-boot', 'd', '-drive', f'file={disk},if=virtio,format=raw']
        if os.path.exists(self.sock): os.unlink(self.sock)
        self.proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            try:
                self.mon = socket.socket(socket.AF_UNIX); self.mon.connect(self.sock); self.mon.settimeout(3); break
            except OSError: time.sleep(0.2)
        self.lock = threading.Lock()

    def hmp(self, c):
        with self.lock:
            self.mon.sendall((c + '\n').encode()); time.sleep(0.02)
            try: self.mon.recv(65536)
            except OSError: pass

    def shot(self, path): self.hmp(f'screendump {path} -f png')

    def type(self, text, delay=0.09):
        for c in text:
            self.hmp('sendkey ' + (KEYS.get(c) or ('shift-' + c.lower() if c.isupper() else c))); time.sleep(delay)


def ocr(path): return subprocess.run(['tesseract', path, '-', '--psm', '6'], capture_output=True, text=True).stdout


class Cap(threading.Thread):
    """a screenshot every 0.3 s; a frame equal to one of the last two is dropped (that also drops the blinking cursor)"""
    def __init__(self, vm, d):
        super().__init__(daemon=True); self.vm, self.d, self.stop, self.frames = vm, d, False, []; os.makedirs(d, exist_ok=True)

    def run(self):
        n, last, prev = 0, None, None
        while not self.stop:
            t = time.time(); p = f'{self.d}/{n:06d}.png'; self.vm.shot(p); time.sleep(0.15)
            if os.path.exists(p):
                h = hashlib.md5(open(p, 'rb').read()).hexdigest()
                if h in (last, prev): os.unlink(p)
                else: prev, last = last, h; self.frames.append((t, p)); n += 1
            time.sleep(max(0, 0.3 - (time.time() - t)))


def record(iso, out):
    vm = VM(iso, out); t0 = time.time(); probe = f'{out}/probe.png'
    while True:                                              # the live shell's welcome text is on screen
        time.sleep(8)
        if os.path.exists(probe): os.unlink(probe)
        vm.shot(probe); time.sleep(1)
        if re.search(r'[mn]elon.?wifi', ocr(probe)): break
        if time.time() - t0 > 1500: sys.exit('the live system did not come up')
    time.sleep(4)
    vm.type('/usr/libexec/melon/.cold\n', 0.08); start = time.time()   # typed before the recording starts: the video never shows it
    cap = Cap(vm, f'{out}/frames'); cap.start()
    lastpw, handled, yes = None, -1, False
    while time.time() < start + 5400:
        time.sleep(1)
        if not cap.frames: continue
        ft, fp = cap.frames[-1]
        if time.time() - ft < 3 or len(cap.frames) == handled: continue   # the screen is still moving
        txt = ocr(fp)
        if yes:
            if re.search(r'is instal|install media|reboot', txt): break
            continue
        hits = [(m.start(), pat, ans) for pat, ans in ANSWERS for m in re.finditer(pat, txt)]
        if not hits: time.sleep(4); continue
        _, pat, ans = max(hits)                                           # the prompt furthest down the screen
        if pat == r'again': ans = lastpw
        if pat in (r'[Rr]oot pass', r'Passw\w* for'): lastpw = ans
        yes = pat == r'YES'
        handled = len(cap.frames); vm.type(ans + chr(10), 0.1); time.sleep(1.5)
    time.sleep(8); cap.stop = True; time.sleep(1)
    pickle.dump((start, cap.frames), open(f'{out}/frames.pkl', 'wb'))
    vm.proc.terminate()


def title_band(im):
    """the rows of the big title letters (white or yellow, drawn in blocks; they scroll up with the console), or None"""
    r, g, _ = im.split()
    bw = ImageChops.darker(r, g).crop((380, 0, 900, 800)).point(lambda v: 255 if v > 200 else 0)
    rows = [bw.crop((0, y, 520, y + 1)).histogram()[255] for y in range(800)]
    best = max(range(0, 744), key=lambda y: sum(rows[y:y + 56]))
    if sum(rows[best:best + 56]) < 1500: return None
    ys = [y for y in range(best, best + 56) if rows[y] >= 90]
    return (max(0, ys[0] - 3), ys[-1] + 8)


def build(out, mp4):
    _, fr = pickle.load(open(f'{out}/frames.pkl', 'rb'))
    ims = [(t, Image.open(p).convert('RGB')) for t, p in fr]

    ims = ims[next(i for i, (t, im) in enumerate(ims) if title_band(im)):]   # starts at the intro, not at the typed command
    os.makedirs(f'{out}/web', exist_ok=True)
    lines, total = [], 6
    for k, (t, im) in enumerate(ims):
        band = title_band(im)
        if band: im.paste((0, 0, 0), (380, band[0], 900, band[1]))
        p = f'{out}/web/{k:05d}.png'; im.save(p)
        d = min(max(ims[k + 1][0] - t, 0.12), 2.5) if k + 1 < len(ims) else 0.3
        lines.append((p, d)); total += d
    with open(f'{out}/web.ffconcat', 'w') as f:
        f.write('ffconcat version 1.0\n')
        for p, d in lines: f.write(f"file '{p}'\nduration {d:.3f}\n")
        f.write(f"file '{lines[-1][0]}'\n")
    subprocess.run(['ffmpeg', '-nostdin', '-loglevel', 'error', '-y', '-f', 'concat', '-safe', '0', '-i', f'{out}/web.ffconcat', '-i', f'{out}/audio.wav',
                    '-t', f'{total:.1f}', '-r', '25', '-vf', 'format=yuv420p,tpad=stop_mode=clone:stop_duration=6', '-c:v', 'libx264', '-crf', '30',
                    '-preset', 'slow', '-af', f'afade=t=out:st={max(0, total - 3):.1f}:d=3', '-c:a', 'aac', '-b:a', '96k', '-movflags', '+faststart', mp4], check=True)
    # the poster for the page: a frame from the middle of the intro (already blacked out)
    Image.open(f'{out}/web/{min(12, len(ims) - 1):05d}.png').save(os.path.splitext(mp4)[0] + '-poster.webp', 'WEBP', quality=80, method=6)


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == 'record': record(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 4 and sys.argv[1] == 'build': build(sys.argv[2], sys.argv[3])
    else: sys.exit(__doc__)
