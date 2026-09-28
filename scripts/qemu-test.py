#!/usr/bin/env python3
"""Boot-test melon in QEMU over the serial console.

  qemu-test.py live  <iso> <disk.img> [--uefi]   boot the ISO, run the quick installer onto disk.img
  qemu-test.py disk  <disk.img> [--uefi]         boot the installed disk and check the system
  qemu-test.py toram <iso> <disk.img>            boot with "copy to RAM", eject the CD, then use and install
  qemu-test.py desktop <desktop-iso>             boot the desktop ISO with graphics: services up, Plasma running,
                                                 a USB stick mounts through UDisks2, screenshot in logs/qemu-desktop.png
  qemu-test.py desktop-install <desktop-iso> <disk.img>   install the desktop profile, boot the installed disk
                                                 with graphics, log in through SDDM: greeter stays up, Plasma starts
  --luks   install with an encrypted root (and type the passphrase when the installed disk boots)
  --vmware VMware-style virtual hardware: PVSCSI disk, VMXNET3 network, VMware SVGA
"""
import sys, time, pexpect, os
M = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

mode = sys.argv[1]
uefi = '--uefi' in sys.argv
i686 = '--i686' in sys.argv          # 32-bit ISO / disk: run in qemu-system-i386
luks = '--luks' in sys.argv
vmware = '--vmware' in sys.argv
DISKNAME = 'sda' if vmware else 'vda'
args = [a for a in sys.argv[2:] if not a.startswith('--')]
log = open(f'{M}/logs/qemu-{mode}{"-uefi" if uefi else ""}{"-i686" if "--i686" in sys.argv else ""}{"-vmware" if vmware else ""}.log', 'w')

cmd = ['qemu-system-i386' if i686 else 'qemu-system-x86_64', '-m', '1024' if i686 else '3072', '-smp', '2', '-nographic', '-no-reboot',
       '-audiodev', f'wav,id=snd0,path={M}/logs/audio-capture.wav',
       '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0',
       '-netdev', 'user,id=n0', '-device', ('vmxnet3' if vmware else 'virtio-net-pci') + ',netdev=n0',
       # QEMU guest agent channel (the host side is a socket the test talks to)
       '-device', 'virtio-serial', '-chardev', 'socket,path=/tmp/melon-qga.sock,server=on,wait=off,id=qga0',
       '-device', 'virtserialport,chardev=qga0,name=org.qemu.guest_agent.0',
       '-monitor', 'unix:/tmp/melon-qmon.sock,server,nowait']
# hardware acceleration when the build host has it (WSL2 and most PCs do; the original build container didn't)
if os.access('/dev/kvm', os.R_OK | os.W_OK) and not i686:
    cmd += ['-enable-kvm', '-cpu', 'host']
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
    base = cmd + ['-m', '4096']
    cmd = base + ['-cdrom', iso] + disk_args(disk) + ['-boot', 'd']
elif mode in ('live', 'toram'):
    iso, disk = args
    cmd += ['-cdrom', iso] + disk_args(disk) + ['-boot', 'd']
else:
    disk, = args
    cmd += disk_args(disk) + ['-boot', 'c']

t0 = time.time()
def step(s): print(f'[{time.time()-t0:7.1f}s] {s}', flush=True)
p = pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace', timeout=600)
p.logfile_read = log
PROMPT = r'@melon[\w-]*.*[#$] '

def sh(c, timeout=300, expect=PROMPT):
    p.sendline(c)
    p.expect(expect, timeout=timeout)
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
    for svc in ['dbus', 'elogind', 'polkitd', 'udevd', 'NetworkManager', 'sddm']:
        for _ in range(30):                        # services settle in the first seconds after boot
            out = sh(f'doas sv check /var/service/{svc} >/dev/null && echo SVC-READY || echo SVC-WAIT')
            if 'SVC-READY' in out: break
            time.sleep(2)
        up = 'SVC-READY' in out; ok &= up; step(f'service {svc}: ' + ('ready' if up else 'NOT READY'))
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
    # nmcli: the text-mode way to reach the network when the desktop won't start
    out = sh('nmcli -c no -t -f STATE general; nmcli -c no -t -f DEVICE,STATE device')
    online = _re.search(r'(^|[\r\n])connected', out) is not None; ok &= online   # (bash's bracketed-paste codes end in \r)
    step('nmcli: ' + ('NetworkManager connected' if online else 'NOT CONNECTED'))
    if not online: print(repr(out[-600:]))
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
    shell = "ps -o user,comm | awk '$1==\"jcole\" && $2==\"plasmashell\"' | wc -l"
    for _ in range(60):
        if count(shell): break
        time.sleep(3)
    time.sleep(20)                                  # let the desktop finish drawing
    sess = count("loginctl list-sessions --no-legend | awk '$3==\"jcole\" && $6==\"active\"' | wc -l")
    up = count(shell) == 1 and sess == 1; ok &= up
    step(f'Plasma for jcole after 20 s: plasmashell={count(shell)} active sessions={sess}')
    mime = count('[ -s /usr/share/mime/mime.cache ] && echo 1 || echo 0') == 1; ok &= mime
    step('MIME cache: ' + ('present' if mime else 'MISSING'))
    mon(f'screendump {M}/logs/qemu-desktop-installed.ppm'); time.sleep(3)
    step('screenshots: logs/qemu-desktop-greeter.ppm, logs/qemu-desktop-installed.ppm')
    p.sendline('poweroff'); p.expect(pexpect.EOF, timeout=300)
    step('installed desktop test: ' + ('OK' if ok else 'FAILED'))
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
