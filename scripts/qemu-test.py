#!/usr/bin/env python3
"""Boot-test melon in QEMU over the serial console.

  qemu-test.py live  <iso> <disk.img> [--uefi]   boot the ISO, run the quick installer onto disk.img
  qemu-test.py disk  <disk.img> [--uefi]         boot the installed disk and check the system
  qemu-test.py toram <iso> <disk.img>            boot with "copy to RAM", eject the CD, then use and install
"""
import sys, time, pexpect, os

mode = sys.argv[1]
uefi = '--uefi' in sys.argv
args = [a for a in sys.argv[2:] if not a.startswith('--')]
log = open(f'/home/claude/melon/logs/qemu-{mode}{"-uefi" if uefi else ""}.log', 'w')

cmd = ['qemu-system-x86_64', '-m', '3072', '-smp', '2', '-nographic', '-no-reboot',
       '-audiodev', 'wav,id=snd0,path=/home/claude/melon/logs/audio-capture.wav',
       '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0',
       '-netdev', 'user,id=n0', '-device', 'virtio-net-pci,netdev=n0',
       '-monitor', 'unix:/tmp/melon-qmon.sock,server,nowait']
if uefi:
    cmd += ['-bios', '/usr/share/ovmf/OVMF.fd']
if mode in ('live', 'toram'):
    iso, disk = args
    cmd += ['-cdrom', iso, '-drive', f'file={disk},if=virtio,format=raw', '-boot', 'd']
else:
    disk, = args
    cmd += ['-drive', f'file={disk},if=virtio,format=raw', '-boot', 'c']

t0 = time.time()
def step(s): print(f'[{time.time()-t0:7.1f}s] {s}', flush=True)
p = pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace', timeout=600)
p.logfile_read = log
PROMPT = r'@melon[\w-]*.*[#$] '

def sh(c, timeout=300, expect=PROMPT):
    p.sendline(c)
    p.expect(expect, timeout=timeout)
    return p.before

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
    sh('export MELON_DISK=vda MELON_HOSTNAME=melontest MELON_ROOTPW=melonroot MELON_USER=jcole '
       'MELON_USERPW=melonuser MELON_PROFILE=base MELON_YES=1 MELON_SERIAL=1')
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
    sh('export MELON_DISK=vda MELON_HOSTNAME=ramtest MELON_ROOTPW=melonroot MELON_USER=jcole '
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
              'modprobe -v snd_hda_intel; ls /dev/snd; dmesg | tail -5']:
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
    p.sendline('poweroff')
    p.expect(pexpect.EOF, timeout=300)
    step('powered off')
