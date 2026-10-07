#!/usr/bin/env python3
"""Boot-test melon in QEMU over the serial console.

  qemu-test.py live  <iso> <disk.img> [--uefi]   boot the ISO, run the quick installer onto disk.img
  qemu-test.py disk  <disk.img> [--uefi]         boot the installed disk and check the system
  qemu-test.py toram <iso> <disk.img>            boot with "copy to RAM", eject the CD, then use and install
  qemu-test.py desktop <desktop-iso>             boot the desktop ISO with graphics: services up, Plasma running,
                                                 a USB stick mounts through UDisks2, screenshot in logs/qemu-desktop.png
  qemu-test.py desktop-install <desktop-iso> <disk.img>   install the desktop profile, boot the installed disk
                                                 with graphics, log in through SDDM: greeter stays up, Plasma starts
  qemu-test.py dualboot <iso> <disk.img>         a fake Windows disk (EFI partition, MSR, NTFS C:, free space): install
                                                 alongside on UEFI, boot melon from the firmware's boot menu, Windows in
                                                 GRUB, and nothing of Windows' changed
  --luks   install with an encrypted root (and type the passphrase when the installed disk boots)
  --vmware VMware-style virtual hardware: PVSCSI disk, VMXNET3 network, VMware SVGA
"""
import sys, time, pexpect, os
import re as _re
import types, pexpect.expect, pexpect.pty_spawn, pexpect.utils
# pexpect times its timeouts with the wall clock, and WSL's clock jumps (hours at a time after the laptop sleeps,
# or back and forth while Windows and NTP disagree) would end them at once: give it the monotonic clock
_mono = types.SimpleNamespace(**{k: getattr(time, k) for k in dir(time) if not k.startswith('_')})
_mono.time = time.monotonic
for _m in (pexpect.expect, pexpect.pty_spawn, pexpect.utils): _m.time = _mono
M = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

mode = sys.argv[1]
uefi = '--uefi' in sys.argv
i686 = '--i686' in sys.argv          # 32-bit ISO / disk: run in qemu-system-i386
luks = '--luks' in sys.argv
vmware = '--vmware' in sys.argv
offline = '--offline' in sys.argv  # the VM keeps its network card but can't reach anything outside (no internet)
DISKNAME = 'sda' if vmware else 'vda'
args = [a for a in sys.argv[2:] if not a.startswith('--')]
log = open(f'{M}/logs/qemu-{mode}{"-uefi" if uefi else ""}{"-i686" if "--i686" in sys.argv else ""}{"-vmware" if vmware else ""}{"-offline" if offline else ""}.log', 'w')

cmd = ['qemu-system-i386' if i686 else 'qemu-system-x86_64', '-m', '1024' if i686 else '3072', '-smp', '2', '-nographic', '-no-reboot',
       '-audiodev', f'wav,id=snd0,path={M}/logs/audio-capture.wav',
       '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0',
       '-netdev', 'user,id=n0' + (',restrict=on' if offline else ''), '-device', ('vmxnet3' if vmware else 'virtio-net-pci') + ',netdev=n0',
       # QEMU guest agent channel (the host side is a socket the test talks to)
       '-device', 'virtio-serial', '-chardev', 'socket,path=/tmp/melon-qga.sock,server=on,wait=off,id=qga0',
       '-device', 'virtserialport,chardev=qga0,name=org.qemu.guest_agent.0',
       '-monitor', 'unix:/tmp/melon-qmon.sock,server,nowait']
# hardware acceleration when the build host has it (WSL2 and most PCs do; the original build container didn't)
if os.access('/dev/kvm', os.R_OK | os.W_OK) and not i686:
    cmd += ['-enable-kvm', '-cpu', 'host']
elif i686:
    cmd += ['-cpu', 'n270']   # the Intel Atom N270 of the netbooks the 32-bit edition is for (MSI Wind U100): SSE2/SSSE3, no 64-bit
if uefi:
    cmd += ['-bios', '/usr/share/ovmf/OVMF.fd']
if vmware:
    cmd += ['-vga', 'vmware', '-device', 'pvscsi,id=scsi0']
def disk_args(img):
    if vmware:
        return ['-drive', f'file={img},if=none,format=raw,id=d0', '-device', 'scsi-hd,drive=d0,bus=scsi0.0']
    return ['-drive', f'file={img},if=virtio,format=raw']
if mode == 'desktop':
    iso, = args
    # a USB stick like a real one: MBR partition table, one FAT32 partition labelled MELONUSB with a file on it
    import subprocess
    usb = '/tmp/melon-usb.img'
    open(usb, 'wb').truncate(64 << 20)
    subprocess.run(['sfdisk', '-q', usb], input=b'label: dos\nstart=2048, type=c\n', check=True)
    subprocess.run(['mkfs.vfat', '-F', '32', '-n', 'MELONUSB', '--offset', '2048', usb, str((64 << 20) // 1024 - 1024)],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run(['mcopy', '-i', f'{usb}@@1M', '-', '::hello.txt'], input=b'hello from the stick\n', check=True)
    # a real graphics card for KWin; the serial port stays the test's console
    cmd = [c for c in cmd if c != '-nographic'] + ['-display', 'none', '-serial', 'stdio', '-device', 'virtio-vga', '-m', '4096',
                                                 '-cdrom', iso, '-boot', 'd', '-device', 'qemu-xhci,id=xhci',
                                                 '-drive', f'if=none,id=stick,format=raw,file={usb}', '-device', 'usb-storage,bus=xhci.0,drive=stick']
elif mode == 'desktop-install':
    iso, disk = args
    base = cmd + ['-m', '2048' if i686 else '4096']   # 32-bit: what the netbooks it is for can hold
    cmd = base + ['-cdrom', iso] + disk_args(disk) + ['-boot', 'd']
elif mode == 'dualboot':
    import subprocess, hashlib
    iso, disk = args
    def run(*c, **k): return subprocess.run(list(c), check=True, capture_output=True, text=True, **k).stdout
    # the fake Windows disk: 40 GiB GPT, 260 MiB EFI (Windows Boot Manager + fallback loader), 16 MiB MSR,
    # 15 GiB NTFS "C:", the rest unallocated (what Windows' "Shrink Volume" leaves)
    open(disk, 'wb').truncate(40 << 30)
    run('sfdisk', '-q', disk, input='label: gpt\nsize=260MiB, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, name="EFI system partition"\n'
        'size=16MiB, type=E3C9E316-0B5C-4DB8-817D-F92DF00215AE, name="Microsoft reserved partition"\n'
        'size=15GiB, type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7, name="Basic data partition"\n')
    loop = run('losetup', '-P', '-f', '--show', disk).strip()
    try:
        run('mkfs.vfat', '-F', '32', '-n', 'SYSTEM', loop + 'p1'); run('mkntfs', '-Q', '-q', '-L', 'Windows', loop + 'p3')
        os.makedirs('/tmp/melon-esp', exist_ok=True); run('mount', loop + 'p1', '/tmp/melon-esp')
        for path, blob in (('EFI/Microsoft/Boot/bootmgfw.efi', b'fake windows boot manager\n' * 999),
                           ('EFI/Boot/bootx64.efi', b'fake windows fallback loader\n' * 999)):
            os.makedirs(os.path.dirname(f'/tmp/melon-esp/{path}'), exist_ok=True); open(f'/tmp/melon-esp/{path}', 'wb').write(blob)
        run('umount', '/tmp/melon-esp')
    finally:
        run('losetup', '-d', loop)
    def windows_state():
        """checksums of everything Windows owns: its partition table entries, MSR, NTFS C:, its files on the EFI partition"""
        loop = run('losetup', '-P', '-f', '--show', disk).strip()
        try:
            st = {'table': ''.join(l for l in run('sfdisk', '-d', disk).splitlines(True) if 'melon' not in l and not l.startswith(('last-lba', 'first-lba')))}
            for n in ('p2', 'p3'):
                st[n] = hashlib.sha256(open(loop + n, 'rb').read(64 << 20)).hexdigest()
            run('mount', '-o', 'ro', loop + 'p1', '/tmp/melon-esp')
            for path in ('EFI/Microsoft/Boot/bootmgfw.efi', 'EFI/Boot/bootx64.efi'):
                st[path] = hashlib.sha256(open(f'/tmp/melon-esp/{path}', 'rb').read()).hexdigest()
            run('umount', '/tmp/melon-esp')
        finally:
            run('losetup', '-d', loop)
        return st
    before = windows_state()
    # UEFI with a firmware boot menu that survives reboots (writable OVMF variables)
    run('cp', '/usr/share/OVMF/OVMF_VARS_4M.fd', '/tmp/melon-ovmf-vars.fd')
    uefi_args = ['-drive', 'if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd',
                 '-drive', 'if=pflash,format=raw,file=/tmp/melon-ovmf-vars.fd']
    base = cmd + uefi_args
    cmd = base + ['-cdrom', iso] + disk_args(disk) + ['-boot', 'd']
elif mode in ('live', 'toram'):
    iso, disk = args
    cmd += ['-cdrom', iso] + disk_args(disk) + ['-boot', 'd']
else:
    disk, = args
    cmd += disk_args(disk) + ['-boot', 'c']

t0 = time.monotonic()
def step(s): print(f'[{time.monotonic()-t0:7.1f}s] {s}', flush=True)
p = pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace', timeout=600)
p.logfile_read = log
PROMPT = r'@melon[\w-]*.*[#$] '

def sh(c, timeout=300, expect=PROMPT):
    p.sendline(c)
    try:
        p.expect(expect, timeout=timeout)
    except pexpect.TIMEOUT:
        # a busy guest can drop characters typed at the serial console; a lost quote leaves bash waiting for more
        # input. Short commands get one more go (never long ones like the installer: they would run twice)
        if timeout > 300 or expect is not PROMPT: raise
        step(f'no prompt after {c[:50]!r}: interrupting and typing it again')
        p.sendcontrol('c'); p.expect(PROMPT, timeout=30)
        p.sendline(c); p.expect(expect, timeout=timeout)
    # without the terminal's echo of the command line, which contains every marker the command would print
    return p.before.split('\n', 1)[-1]

if mode == 'desktop':
    import socket
    step('waiting for GRUB')
    p.expect('melon live', timeout=300); time.sleep(1)
    for _ in range(3):
        p.send('\x1b[B'); time.sleep(0.3)          # down to "serial console" (a typed GRUB command line loses keys)
    p.send('\r')
    p.expect(PROMPT, timeout=900); step('logged in on the serial console')
    ok = True
    for svc in ['dbus', 'elogind', 'polkitd', 'udevd', 'NetworkManager', 'sddm', 'avahi-daemon', 'cupsd']:
        for _ in range(30):                        # services settle in the first seconds after boot
            out = sh(f'doas sv check /var/service/{svc} >/dev/null && echo SVC-READY || echo SVC-WAIT')
            if 'SVC-READY' in out: break
            time.sleep(2)
        up = 'SVC-READY' in out; ok &= up; step(f'service {svc}: ' + ('ready' if up else 'NOT READY'))
    flapping = sh('sleep 5; doas sv status /var/service/* 2>&1 | grep "want up" | cut -d: -f2 | xargs echo FLAP=')
    flap = _re.search(r'FLAP=([^\r\n]*)', flapping); flap = flap.group(1).strip() if flap else '?'
    ok &= flap == ''; step('services: ' + ('none restarting in a loop' if flap == '' else f'RESTARTING: {flap}'))
    found = False
    for _ in range(40):                            # KWin and plasmashell after SDDM's autologin
        out = sh('ps -o comm | grep -qx kwin_wayland && ps -o comm | grep -qx plasmashell && echo PLASMA-UP || echo waiting')
        if 'PLASMA-UP' in out: found = True; break
        time.sleep(5)
    time.sleep(20)                                  # let the desktop finish drawing
    # still there after settling, and inside a real logind session (a crash loop can look alive for a moment)
    out = sh('echo SESSIONS=$(loginctl list-sessions --no-legend 2>/dev/null | grep -c seat0) '
             'KWIN=$(ps -o comm | grep -cx kwin_wayland) SHELL=$(ps -o comm | grep -cx plasmashell)')
    import re as _re
    v = dict(_re.findall(r'(SESSIONS|KWIN|SHELL)=(\d+)', out))
    stable = found and all(int(v.get(k, '0')) > 0 for k in ('SESSIONS', 'KWIN', 'SHELL')); ok &= stable
    step(f"Plasma after 20 s: sessions={v.get('SESSIONS')} kwin={v.get('KWIN')} plasmashell={v.get('SHELL')}")
    # Plasma's device list asks UDisks2 over D-Bus, which starts udisksd; then mount the stick the same way Dolphin does
    out = sh('echo UDISKSD=$(ps -o comm | grep -cx udisksd)')
    started = 'UDISKSD=1' in out; ok &= started; step('udisksd started by D-Bus activation: ' + ('yes' if started else 'NO'))
    out = sh('dev=$(doas blkid -L MELONUSB); echo "stick: $dev"; doas udisksctl mount -b $dev && '
             'cat /run/media/root/MELONUSB/hello.txt && doas udisksctl unmount -b $dev', timeout=120)
    mounted = 'hello from the stick' in out and 'Mounted' in out; ok &= mounted
    step('USB stick through UDisks2: ' + ('mounted, file read, unmounted' if mounted else 'FAILED'))
    if not mounted: print(out[-2000:])
    # codecs: a second of video through each encoder Spectacle's screen recorder (KPipeWire) asks ffmpeg for,
    # Opus audio, and PipeWire's Bluetooth codecs
    out = sh('cd /tmp; for e in libvpx-vp9:webm libx264:mp4 libwebp_anim:webp; do ffmpeg -v error -f lavfi '
             '-i testsrc=duration=1:size=320x240:rate=10 -c:v ${e%:*} -y rec.${e#*:} && [ -s rec.${e#*:} ] && echo ENC-OK ${e%:*}; done; '
             'ffmpeg -v error -f lavfi -i sine=duration=1 -c:a libopus -y rec.opus && echo ENC-OK libopus; cd; '
             'ls /usr/lib/spa-0.2/bluez5/ | grep -oE "codec-bluez5-[a-z0-9]+" | xargs', timeout=180)
    have = set(_re.findall(r'ENC-OK ([a-z0-9_-]+)', out)) | set(_re.findall(r'codec-bluez5-[a-z0-9]+', out))
    missing = sorted({'libvpx-vp9', 'libx264', 'libwebp_anim', 'libopus',
                      'codec-bluez5-sbc', 'codec-bluez5-opus', 'codec-bluez5-aptx', 'codec-bluez5-lc3'} - have); ok &= not missing
    step('codecs: ' + ('VP9, H.264, WebP and Opus encode; Bluetooth codecs present' if not missing else f'MISSING {missing}'))
    if missing: print(out[-2000:])
    # printing: CUPS' virtual IPP Everywhere printer that only takes PWG raster, a driverless queue for it, and a text
    # file through the whole filter chain (texttopdf, pdftopdf with QPDF, pdftoraster with Poppler, the ipp backend)
    out = sh('rm -rf /tmp/ippspool; mkdir -p /tmp/ippspool; (ippeveprinter -r off -p 8631 -f image/pwg-raster -k -d /tmp/ippspool '
             'MelonTest >/tmp/ippeve.log 2>&1 &); sleep 3; lpadmin -p melontest -E -v ipp://localhost:8631/ipp/print -m everywhere '
             '&& lp -d melontest /etc/os-release; for i in $(seq 60); do [ -z "$(lpstat -o melontest)" ] && break; sleep 1; done; '
             'lpstat -W completed -o melontest | wc -l | sed "s/^/COMPLETED=/"; ls /tmp/ippspool | grep -c "pwg$" | sed "s/^/RASTER=/"', timeout=180)
    v = dict(_re.findall(r'(COMPLETED|RASTER)=(\d+)', out))
    printed = int(v.get('COMPLETED', '0')) >= 1 and int(v.get('RASTER', '0')) >= 1; ok &= printed
    step('printing: ' + ('a text file reached the IPP Everywhere printer as PWG raster' if printed else f'FAILED {v}'))
    if not printed: print(out[-1500:]); print(sh('tail -20 /var/log/cups/error_log; tail -5 /tmp/ippeve.log'))
    # nmcli: the text-mode way to reach the network when the desktop won't start
    out = sh('nmcli -c no -t -f STATE general; nmcli -c no -t -f DEVICE,STATE device')
    online = _re.search(r'(^|[\r\n])connected', out) is not None; ok &= online   # (bash's bracketed-paste codes end in \r)
    step('nmcli: ' + ('NetworkManager connected' if online else 'NOT CONNECTED'))
    if not online: print(repr(out[-600:]))
    # Wi-Fi the way Plasma's network applet does it (NetworkManager, which starts wpa_supplicant over D-Bus) on two
    # virtual radios: wlan1 is a WPA2 access point run by a separate wpa_supplicant, NetworkManager joins it with wlan0
    ap = ("printf '%s\\n' 'network={' 'ssid=\"melontest\"' 'mode=2' 'frequency=2437' 'key_mgmt=WPA-PSK' 'proto=RSN' "
          "'pairwise=CCMP' 'psk=\"melonwifi\"' '}' > /tmp/ap.conf")
    # The radios appear while udevd is stopped, like a real card whose interface registers after its firmware loads,
    # in the gap between stage 1's udevd and the udevd service: nothing processes them and NetworkManager leaves them
    # unmanaged. Starting udevd must replay those events (recipes/eudev/udevd.run).
    out = sh('sv down /var/service/udevd; modprobe mac80211_hwsim radios=2; sleep 3; '
             'nmcli -c no -t -f DEVICE,STATE device | grep "^wlan0:" | sed "s/^/GAP=/"; sv up /var/service/udevd', timeout=60)
    gap = _re.search(r'GAP=wlan0:(\S+)', out); gap = gap.group(1) if gap else '?'
    out = sh('for i in $(seq 20); do nmcli -c no -t -f DEVICE,STATE device | grep -q "^wlan1:unmanaged" || break; sleep 1; done; '
             'nmcli -c no device set wlan1 managed no && ' + ap + ' && '
             'wpa_supplicant -B -i wlan1 -c /tmp/ap.conf >/tmp/ap.log 2>&1 && sleep 3 && '
             'nmcli -c no connection add type wifi ifname wlan0 con-name melontest ssid melontest wifi-sec.key-mgmt wpa-psk '
             'wifi-sec.psk melonwifi ipv4.method disabled ipv6.method link-local >/dev/null && '
             'nmcli -c no --wait 60 connection up melontest >/dev/null 2>&1; '
             'nmcli -c no -t -f GENERAL.STATE device show wlan0 | sed "s/^GENERAL.STATE:/WIFI=/"', timeout=180)
    wifi = 'WIFI=100' in out; ok &= wifi
    step(f'Wi-Fi: radios found while udevd was down were {gap}; ' +
         ('after the replay NetworkManager joined a WPA2 network on one' if wifi else 'FAILED'))
    if not wifi: print(out[-1500:]); print(sh('cat /tmp/ap.log; nmcli -c no device; tail -20 /var/log/NetworkManager/current'))
    m = socket.socket(socket.AF_UNIX); m.connect('/tmp/melon-qmon.sock'); time.sleep(0.5)
    m.sendall(f'screendump {M}/logs/qemu-desktop.ppm\n'.encode()); time.sleep(3); m.close()
    step('screenshot: logs/qemu-desktop.ppm')
    sh('doas poweroff', expect=pexpect.EOF, timeout=300)
    step('desktop test: ' + ('OK' if ok else 'FAILED'))
    sys.exit(0 if ok else 1)

if mode == 'desktop-install':
    import socket, re as _re
    def mon(line):
        m = socket.socket(socket.AF_UNIX); m.connect('/tmp/melon-qmon.sock'); time.sleep(0.3)
        m.sendall(line.encode() + b'\n'); time.sleep(0.4); m.close()
    def count(c):                                   # run c, return the number it echoes as N=<n>
        m = _re.search(r'N=(\d+)', sh(f'echo N=$({c})')); return int(m.group(1)) if m else 0
    step('waiting for GRUB')
    p.expect('melon live', timeout=300); time.sleep(1)
    for _ in range(3):
        p.send('\x1b[B'); time.sleep(0.3)          # down to "serial console"
    p.send('\r')
    p.expect(PROMPT, timeout=900); step('logged in on the live desktop ISO')
    sh('stty cols 160 rows 50; export TERM=vt100')
    sh(f'export MELON_DISK={DISKNAME} MELON_HOSTNAME=melondesk MELON_ROOTPW=melonroot MELON_USER=jcole '
       'MELON_USERPW=melonuser MELON_PROFILE=desktop MELON_YES=1 MELON_SERIAL=1 MELON_ENCRYPT=n')
    out = sh('/usr/libexec/melon/.cold', timeout=3600)
    ok = 'melon is installed' in out
    step(f'desktop install: {"OK" if ok else "FAILED"}')
    if not ok: print(out[-3000:])
    p.sendline('poweroff'); p.expect(pexpect.EOF, timeout=300)
    if not ok: sys.exit(1)
    # second boot: the installed disk, with a graphics card for SDDM and KWin; the serial port stays the test's console
    cmd = [c for c in base if c != '-nographic'] + ['-display', 'none', '-serial', 'stdio', '-device', 'virtio-vga'] \
          + disk_args(disk) + ['-boot', 'c']
    p = pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace', timeout=600)
    p.logfile_read = log
    p.expect('login:', timeout=900); p.sendline('root'); p.expect('assword:'); p.sendline('melonroot')
    p.expect(PROMPT, timeout=120); step('logged in on the installed desktop (serial console)')
    time.sleep(15)                                  # SDDM and KWin are starting: typing now can lose characters
    sh('stty cols 160 rows 50; export TERM=vt100')
    # (comm is cut to 15 characters: sddm-greeter-qt6 shows as sddm-greeter-qt)
    greeter = "ps -o pid,comm | awk '$2 ~ /^sddm-greeter/{print $1}' | head -1"
    for _ in range(60):
        pid = count(greeter)
        if pid: break
        time.sleep(2)
    step(f'greeter: {"running" if pid else "NOT RUNNING"}')
    time.sleep(20)                                  # a crash-looping greeter comes back with a new pid
    stable = pid and count(greeter) == pid and count("loginctl list-sessions --no-legend | awk '$3==\"sddm\" && $6==\"active\"' | wc -l") == 1
    ok &= bool(stable); step('greeter after 20 s: ' + ('same process, active session' if stable else 'RESTARTED OR NO SESSION'))
    mon(f'screendump {M}/logs/qemu-desktop-greeter.ppm'); time.sleep(3)
    for ch in 'melonuser':
        mon(f'sendkey {ch}'); time.sleep(0.15)
    mon('sendkey ret'); step('typed the password into the greeter')
    time.sleep(3); mon(f'screendump {M}/logs/qemu-desktop-splash.ppm')   # Plasma's loading screen
    # the desktop's shell: Plasma's plasmashell, or on the 32-bit edition LXQt's panel (on labwc)
    desk = 'lxqt-panel' if i686 else 'plasmashell'
    shell = f"ps -o user,comm | awk '$1==\"jcole\" && $2==\"{desk}\"' | wc -l"
    for _ in range(60):
        if count(shell): break
        time.sleep(3)
    time.sleep(20)                                  # let the desktop finish drawing
    sess = count("loginctl list-sessions --no-legend | awk '$3==\"jcole\" && $6==\"active\"' | wc -l")
    up = count(shell) == 1 and sess == 1; ok &= up
    if i686:
        lab = count("ps -o user,comm | awk '$1==\"jcole\" && $2==\"labwc\"' | wc -l"); up &= lab == 1; ok &= lab == 1
        step(f'LXQt for jcole after 20 s: lxqt-panel={count(shell)} labwc={lab} active sessions={sess}')
    else:
        step(f'Plasma for jcole after 20 s: plasmashell={count(shell)} active sessions={sess}')
    mime = count('[ -s /usr/share/mime/mime.cache ] && echo 1 || echo 0') == 1; ok &= mime
    # a service runit keeps restarting shows as "down: 1s, normally up, want up" (polkitd did, raced by D-Bus activation)
    flapping = sh('sv status /var/service/* 2>&1 | grep "want up" | cut -d: -f2 | xargs echo FLAP=')
    flap = _re.search(r'FLAP=([^\r\n]*)', flapping); flap = flap.group(1).strip() if flap else '?'
    ok &= flap == ''; step('services: ' + ('none restarting in a loop' if flap == '' else f'RESTARTING: {flap}'))
    step('MIME cache: ' + ('present' if mime else 'MISSING'))
    # Flatpak must hand X11 apps (Steam, VLC's interface) an Xauthority cookie, or they never open a window
    if not i686:                                    # no Flatpak on the 32-bit edition
        xau = count('ldd /usr/bin/flatpak | grep -c libXau')
        ok &= xau == 1; step('flatpak: ' + ('X11 authorization for sandboxed apps' if xau == 1 else 'BUILT WITHOUT libXau (X11 apps cannot open windows)'))
    # packaged files owned by the build machine's account arrive as nobody's (rule 45)
    nob = count('find /usr /etc -xdev \\( -user 65534 -o -group 65534 \\) | wc -l')
    ok &= nob == 0; step('system files owned by nobody: ' + ('none' if nob == 0 else f'{nob} FOUND'))
    # the desktop profile's services, and the user may manage printers
    svcs = count('n=0; for s in avahi-daemon cupsd; do sv check /var/service/$s >/dev/null 2>&1 && n=$((n+1)); done; echo $n')
    lpadm = count('id -Gn jcole | tr " " "\\n" | grep -cx lpadmin')
    ok &= svcs == 2 and lpadm == 1; step(f'printing on the installed system: {svcs}/2 services up, jcole in lpadmin: {lpadm == 1}')
    # the gauntlet rewards through the unlock script (called by path; the command that starts it stays unnamed)
    out = sh('/usr/libexec/melon/.gold jcole; echo REW=$([ -s /etc/melon/gauntlet-survivor ] && echo badge)'
             '$(ls -d /usr/share/wallpapers/melon-survivor-* | wc -l)$([ -s /home/jcole/Pictures/melon-gauntlet-certificate.svg ] && echo cert)'
             '$(grep -q "set theme=" /boot/grub/grub.cfg && grep -q "survivor edition" /boot/grub/themes/melon/theme.txt && echo gold)'
             '$(grep -qx "Current=melon-gold" /etc/sddm.conf.d/20-survivor.conf && [ -s /usr/share/sddm/themes/melon-gold/Main.qml ] && echo sddm)')
    rew = 'REW=badge3certgoldsddm' in out; ok &= rew
    step('gauntlet rewards: ' + ('badge, 3 survivor wallpapers, certificate, gold boot menu, gold login screen' if rew else f'MISSING {out[-300:]!r}'))
    # the gold look in jcole's running session (what a survivor's first login runs), read back from jcole's config;
    # it is Plasma's look (colours, Plasma style, Konsole), so not on the 32-bit LXQt edition
    if not i686:
        sh("pid=$(ps -o pid,user,comm | awk '$2==\"jcole\" && $3==\"plasmashell\"{print $1}' | head -1)")
        sh("e=$(tr '\\0' '\\n' < /proc/$pid/environ | grep -E '^(DBUS_SESSION_BUS_ADDRESS|WAYLAND_DISPLAY|XDG_RUNTIME_DIR)=' | tr '\\n' ' ')")
        look = sh('su -s /bin/sh jcole -c "env $e /usr/libexec/melon/melon-survivor-look"; echo LOOK=$?; '
                  'su -s /bin/sh jcole -c \'kreadconfig6 --file kdeglobals --group General --key ColorScheme; '
                  'kreadconfig6 --file plasmarc --group Theme --key name; '
                  'kreadconfig6 --file konsolerc --group "Desktop Entry" --key DefaultProfile; '
                  'kreadconfig6 --file kscreenlockerrc --group Greeter --group Wallpaper --group org.kde.image --group General --key Image\' '
                  '| tr "\\n" " " | sed "s/^/GOLD=/"')
        gold = 'LOOK=0' in look and 'GOLD=MelonGold melon-gold MelonGold.profile /usr/share/wallpapers/melon-survivor-gold/' in look
        ok &= gold; step('gold look in the session: ' + ('colours, Plasma style, Konsole, lock screen, wallpaper' if gold else f'MISSING {look[-300:]!r}'))
    time.sleep(10); mon(f'screendump {M}/logs/qemu-desktop-installed.ppm'); time.sleep(3)
    # the gold login screen: back to the greeter (this ends jcole's session)
    sh('sv restart sddm')
    for _ in range(60):
        time.sleep(2); gpid = count(greeter)
        if gpid and gpid != pid: break
    time.sleep(15); gup = gpid and count(greeter) == gpid; ok &= bool(gup)
    step('gold greeter after restarting SDDM: ' + ('running' if gup else 'NOT RUNNING'))
    mon(f'screendump {M}/logs/qemu-desktop-greeter-gold.ppm'); time.sleep(3)
    step('screenshots: logs/qemu-desktop-greeter.ppm, -splash.ppm, -installed.ppm, -greeter-gold.ppm')
    p.sendline('poweroff'); p.expect(pexpect.EOF, timeout=300)
    step('installed desktop test: ' + ('OK' if ok else 'FAILED'))
    sys.exit(0 if ok else 1)

if mode == 'dualboot':
    step('waiting for GRUB (UEFI)')
    p.expect('melon live', timeout=300); time.sleep(1)
    for _ in range(3):
        p.send('\x1b[B'); time.sleep(0.3)
    p.send('\r')
    p.expect(PROMPT, timeout=900); step('logged in on the live system')
    sh('stty cols 160 rows 50; export TERM=vt100')
    sh(f'export MELON_DISK={DISKNAME} MELON_HOSTNAME=melondual MELON_ROOTPW=melonroot MELON_USER=jcole MELON_USERPW=melonuser '
       'MELON_PROFILE=base MELON_YES=1 MELON_SERIAL=1 MELON_ENCRYPT=n MELON_MODE=alongside')
    out = sh('/usr/libexec/melon/.cold', timeout=3600)
    ok = 'melon is installed' in out and 'next to the other systems' in out
    step('install alongside: ' + ('OK' if ok else 'FAILED'))
    if not ok: print(out[-3000:])
    p.sendline('poweroff'); p.expect(pexpect.EOF, timeout=300)
    if not ok: sys.exit(1)
    # boot the disk with no boot order given: the firmware must pick melon from its own boot menu
    cmd = base + disk_args(disk)
    p = pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace', timeout=600); p.logfile_read = log
    i = p.expect(['melon Linux', 'fake windows', pexpect.TIMEOUT], timeout=300)
    menu = p.before + (p.after if isinstance(p.after, str) else '')
    ok &= i == 0; step('firmware boot menu: ' + ('started melon\'s GRUB' if i == 0 else 'DID NOT START MELON'))
    try: p.expect('Windows', timeout=5); win_menu = True
    except pexpect.TIMEOUT: win_menu = 'Windows' in menu
    p.expect('login:', timeout=900); p.sendline('root'); p.expect('assword:'); p.sendline('melonroot'); p.expect(PROMPT, timeout=120)
    sh('stty cols 160 rows 50; export TERM=vt100')
    out = sh("echo WIN=$(grep -c \"menuentry 'Windows'\" /boot/grub/grub.cfg) EFIMNT=$(awk '$2==\"/boot/efi\"{print $2}' /proc/mounts) "
             "NVRAM=$(efibootmgr | grep -c -i melon); ls /boot/efi/EFI | xargs echo ESP:")
    v = dict(_re.findall(r'(WIN|EFIMNT|NVRAM)=(\S*)', out))
    good = v.get('WIN') == '1' and v.get('EFIMNT') == '/boot/efi' and v.get('NVRAM', '0') != '0'; ok &= good
    step(f"installed system: Windows in GRUB {v.get('WIN')} (menu showed it: {win_menu}), EFI partition at {v.get('EFIMNT')}, "
         f"firmware entries for melon {v.get('NVRAM')}")
    print(sh('ls /boot/efi/EFI; efibootmgr'))
    p.sendline('poweroff'); p.expect(pexpect.EOF, timeout=300)
    after = windows_state()
    same = after == before; ok &= same
    step('Windows untouched: ' + ('partition table entries, MSR, NTFS and its EFI files identical' if same else
         f'CHANGED: {[k for k in before if before[k] != after.get(k)]}'))
    step('dual boot test: ' + ('OK' if ok else 'FAILED'))
    sys.exit(0 if ok else 1)

if mode == 'live':
    step('waiting for GRUB')
    p.expect('melon live', timeout=300)
    time.sleep(1)
    for _ in range(3):
        p.send('\x1b[B'); time.sleep(0.3)          # down to "serial console"
    p.send('\r')
    step('booting kernel')
    p.expect('melon live: looking for boot medium', timeout=600)
    step('initramfs running')
    p.expect(PROMPT, timeout=900)
    step('logged in on live system')
    sh('stty cols 160 rows 50; export TERM=vt100')
    print(sh('melonfetch'))
    print(sh('cat /proc/asound/cards; ls /dev/snd'))
    step('running the hidden installer')
    sh(f'export MELON_DISK={DISKNAME} MELON_HOSTNAME=melontest MELON_ROOTPW=melonroot MELON_USER=jcole '
       'MELON_USERPW=melonuser MELON_PROFILE=base MELON_YES=1 MELON_SERIAL=1'
       + (' MELON_ENCRYPT=y MELON_LUKSPW=melonluks' if luks else ' MELON_ENCRYPT=n'))
    out = sh('/usr/libexec/melon/.cold', timeout=3600)
    print(out[-3000:])
    ok = 'melon is installed' in out
    step(f'installer finished: {"OK" if ok else "FAILED"}')
    print(sh('ls /usr/bin | wc -l'))
    p.sendline('poweroff')
    p.expect(pexpect.EOF, timeout=300)
    sys.exit(0 if ok else 1)
elif mode == 'toram':
    step('waiting for GRUB')
    p.expect('melon live', timeout=300); time.sleep(1)
    p.send('c'); p.expect('grub>', timeout=60)
    for line in ['linux /boot/vmlinuz toram console=tty0 console=ttyS0,115200', 'initrd /boot/initramfs.img', 'boot']:
        for ch in line: p.send(ch); time.sleep(0.08)   # GRUB drops keys that arrive too fast
        p.send('\r'); time.sleep(1)
    p.expect('copying .* MiB to RAM', timeout=600); step('copying to RAM')
    p.expect('you can remove the boot medium now', timeout=900); step('copy finished')
    p.expect(PROMPT, timeout=900); step('logged in')
    # pull the "USB stick": eject the CD through the QEMU monitor
    import socket
    m = socket.socket(socket.AF_UNIX); m.connect('/tmp/melon-qmon.sock'); time.sleep(0.5)
    m.sendall(b'eject -f ide1-cd0\n'); time.sleep(1); m.sendall(b'info block\n'); time.sleep(1)
    print(m.recv(65536).decode(errors='replace')); m.close()
    step('boot medium ejected')
    sh('stty cols 160 rows 50; export TERM=vt100')
    for c in ['grep -E "media|sr0" /proc/mounts', 'ls -la /media/melon/melon', 'dd if=/dev/sr0 of=/dev/null bs=2048 count=1', 'free -m',
              'melonfetch | tail -3', 'mpg123 --version | head -1']:
        print(f'$ {c}'); print(sh(c))
    sh(f'export MELON_DISK={DISKNAME} MELON_HOSTNAME=ramtest MELON_ROOTPW=melonroot MELON_USER=jcole '
       'MELON_USERPW=melonuser MELON_PROFILE=base MELON_YES=1 MELON_SERIAL=1')
    out = sh('/usr/libexec/melon/.cold', timeout=3600)
    ok = 'melon is installed' in out
    step(f'install with the medium removed: {"OK" if ok else "FAILED"}')
    if not ok: print(out[-3000:])
    p.sendline('poweroff'); p.expect(pexpect.EOF, timeout=300)
    sys.exit(0 if ok else 1)
else:
    step('waiting for GRUB on the installed disk')
    p.expect("melon Linux", timeout=300)
    step('kernel booting')
    if luks:
        p.expect('unlocking the encrypted disk', timeout=900); step('initramfs asks for the passphrase')
        p.expect('assphrase', timeout=120); p.sendline('melonluks'); step('passphrase sent')
    p.expect('login:', timeout=900)
    step('login prompt')
    p.sendline('root'); p.expect('assword:'); p.sendline('melonroot')
    p.expect(PROMPT, timeout=120)
    step('logged in')
    sh('stty cols 160 rows 50; export TERM=vt100')
    for c in ['melonfetch', 'cat /etc/os-release', 'findmnt / /boot', 'melon-svc status',
              'apk list --installed | wc -l', 'apk update && apk search -q | wc -l',
              'ls /sys/firmware/efi >/dev/null 2>&1 && echo booted-UEFI || echo booted-BIOS',
              'id jcole', 'ip -4 addr show dev eth0 | grep inet; ping -c1 -W3 10.0.2.2 >/dev/null && echo net-ok',
              'ls /usr/libexec/melon', 'free -m; df -h / /boot', 'wc -l < /proc/modules; ls /usr/lib/modules',
              'lspci 2>/dev/null | head; cat /sys/bus/pci/devices/*/modalias | head -20',
              'modprobe -v snd_hda_intel; ls /dev/snd; dmesg | tail -5',
              'melon-detect-virt; lsmod | grep -E "vmw|vmxnet|pvscsi|virtio|hv_|vbox" ; ls /var/service',
              'sleep 3; sv status /var/service/qemu-ga /var/service/vmtoolsd /var/service/hv_kvp_daemon 2>&1']:
        print(f'$ {c}'); print(sh(c))
    # sudo as the normal user: asks for the password once, then remembers it
    p.sendline('su - jcole'); p.expect(r'@melon[\w-]*.*\$ ', timeout=60)
    p.sendline('sudo id -u'); p.expect('assword', timeout=60); p.sendline('melonuser')
    p.expect(r'\n\r*0\r*\n', timeout=60); step('sudo as jcole: OK (got uid 0)')
    p.sendline('sudo -i whoami'); i = p.expect([r'[\r\n]root\r', 'assword'], timeout=60)
    step('sudo -i without asking again: ' + ('OK' if i == 0 else 'asked again'))
    if i == 1: p.sendline('melonuser'); p.expect('root', timeout=60)
    p.sendline('exit'); p.expect(PROMPT, timeout=60)
    print(sh('melon-svc status; ls /var/log'))
    # NetHack: setgid games (never root), and the shared scores belong to group games
    # (waits for the answer itself: after the su session above, output and prompts can arrive one command late)
    p.sendline("echo NH=$(stat -c '%a %U:%G' /usr/lib/nethack/nethack /var/games/nethack /var/games/nethack/record | xargs)")
    try: p.expect(r'NH=(\d+ \S+ \d+ \S+ \d+ \S+)\r', timeout=60); nh = p.match.group(1)
    except pexpect.TIMEOUT: nh = 'no answer'
    nh_ok = nh == '2755 root:games 775 root:games 664 root:games'
    step('nethack: ' + ('setgid games, shared scores' if nh_ok else f'WRONG {nh[-200:]!r}'))
    # the host side of the guest agent: ask the VM for its OS info, then shut it down from the host
    import socket, json
    try:
        q = socket.socket(socket.AF_UNIX); q.connect('/tmp/melon-qga.sock'); q.settimeout(20)
        q.sendall(b'{"execute":"guest-sync","arguments":{"id":1234}}\n'); print('qga sync:', q.recv(4096).decode().strip())
        q.sendall(b'{"execute":"guest-get-osinfo"}\n'); print('qga osinfo:', q.recv(4096).decode().strip())
        q.sendall(b'{"execute":"guest-shutdown"}\n'); step('asked the guest agent to shut down')
        p.expect(pexpect.EOF, timeout=300); step('powered off by the host (guest agent)')
    except Exception as e:
        step(f'guest agent not answering ({e}), powering off from inside')
        p.sendline('poweroff'); p.expect(pexpect.EOF, timeout=300); step('powered off')
    if not nh_ok: sys.exit(1)
