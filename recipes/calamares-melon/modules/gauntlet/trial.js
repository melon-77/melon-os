.pragma library
// the final trial: answers are salted md5 hashes (Qt.md5(s + answer) === h)
var questions = [
 {
  "q": "Which command changes a file's permissions?",
  "o": [
   "chmod",
   "chown",
   "umask",
   "chgrp"
  ],
  "s": "09ff0892d02c",
  "h": "333577e404328fb016f073357e476f4e"
 },
 {
  "q": "After chmod 755 file, what can \"others\" do with it?",
  "o": [
   "read and execute",
   "read only",
   "nothing",
   "read, write and execute"
  ],
  "s": "04757425d32f",
  "h": "4e52d716b70471eade822236b5255d8b"
 },
 {
  "q": "What does 2>&1 do in a shell command?",
  "o": [
   "sends error output to the same place as normal output",
   "runs the command twice",
   "redirects input from file 1",
   "runs it in the background"
  ],
  "s": "57b5d49fef73",
  "h": "d2664c493e9d06c2cd99523a4f69873d"
 },
 {
  "q": "Which signal does kill send when you don't name one?",
  "o": [
   "SIGTERM",
   "SIGKILL",
   "SIGHUP",
   "SIGINT"
  ],
  "s": "4d6ba532dceb",
  "h": "ba66e24e41f93bb350780bfd026dc7a7"
 },
 {
  "q": "What extra files does ls -a show?",
  "o": [
   "hidden files, whose names start with a dot",
   "system files",
   "deleted files",
   "files owned by root"
  ],
  "s": "64c91d138baa",
  "h": "fc118b108e5d05419d5eca491935efcc"
 },
 {
  "q": "Which file lists the filesystems to mount at boot?",
  "o": [
   "/etc/fstab",
   "/etc/mtab",
   "/boot/mounts",
   "/etc/disks"
  ],
  "s": "18b92a20ed88",
  "h": "e6c1b370f2d729c05b29b93ded901fcd"
 },
 {
  "q": "What is process ID 1 on a running Linux system?",
  "o": [
   "the init system",
   "the kernel",
   "the shell",
   "the display server"
  ],
  "s": "b4c0b6fa341b",
  "h": "6bcc6351f4a1c00994adfb62621faac6"
 },
 {
  "q": "Which command keeps printing a log file as it grows?",
  "o": [
   "tail -f",
   "cat -f",
   "less -g",
   "head -n"
  ],
  "s": "015c2cac7364",
  "h": "e6a93786fd2eee587194ec194cbc7f6c"
 },
 {
  "q": "What does | do between two commands?",
  "o": [
   "passes the first one's output as the second one's input",
   "runs them at the same time, unrelated",
   "runs the second only if the first fails",
   "saves output to a file"
  ],
  "s": "2d0ee5927336",
  "h": "c3a878f1ad843ea0a8dd01ea96da1c55"
 },
 {
  "q": "Where do most system-wide configuration files live?",
  "o": [
   "/etc",
   "/usr/conf",
   "/var",
   "/opt"
  ],
  "s": "29adcc1ae491",
  "h": "56ac7ef3a7368c87246aeb3c96c37b88"
 },
 {
  "q": "Which directory holds device files like disks and terminals?",
  "o": [
   "/dev",
   "/sys/devices only",
   "/mnt",
   "/hw"
  ],
  "s": "1af37e7e6af6",
  "h": "2eb7ac02bf5533e5f01008e881594876"
 },
 {
  "q": "In a path, what does .. mean?",
  "o": [
   "the parent directory",
   "the current directory",
   "the home directory",
   "the root directory"
  ],
  "s": "b82605ac2c00",
  "h": "9360d15574909c7146e058e6ca4bff36"
 },
 {
  "q": "Which command searches for text inside files?",
  "o": [
   "grep",
   "find",
   "locate",
   "which"
  ],
  "s": "18241f1df606",
  "h": "ab1904ca196b35a25e6fca5e891bf91b"
 },
 {
  "q": "What exit status means a command succeeded?",
  "o": [
   "0",
   "1",
   "255",
   "-1"
  ],
  "s": "c2d61e04926f",
  "h": "518b2d53d5059f281c588b7e3568e7fd"
 },
 {
  "q": "What does chmod +x script.sh let you do?",
  "o": [
   "run it as a program",
   "edit it",
   "hide it",
   "delete it"
  ],
  "s": "2eec5474b733",
  "h": "419061e4b5db9b913b2c404cbdcf06ef"
 },
 {
  "q": "Which port does HTTPS use by default?",
  "o": [
   "443",
   "80",
   "22",
   "8080"
  ],
  "s": "20c23da1d618",
  "h": "477385d71e457d25920b19a0b77c0e00"
 },
 {
  "q": "What does DNS do?",
  "o": [
   "turns names like example.com into IP addresses",
   "encrypts web traffic",
   "assigns IP addresses",
   "blocks ads"
  ],
  "s": "e26a6c213404",
  "h": "1fdf3fe5009f908573170b8965f0e533"
 },
 {
  "q": "Which of these is a private (home network) IPv4 address?",
  "o": [
   "192.168.1.10",
   "8.8.8.8",
   "1.1.1.1",
   "142.250.1.1"
  ],
  "s": "e235b51d4549",
  "h": "98d83b96ff15de38d5b0089001b3775c"
 },
 {
  "q": "How many bits are in an IPv4 address?",
  "o": [
   "32",
   "64",
   "128",
   "16"
  ],
  "s": "0854c3a6e5d7",
  "h": "795c1d37d54be932a72701c16226bfc7"
 },
 {
  "q": "What does DHCP give your computer when it joins a network?",
  "o": [
   "an IP address and network settings, automatically",
   "a faster connection",
   "a firewall",
   "a domain name"
  ],
  "s": "bd7961559031",
  "h": "3f4d9a32fe308a7b9cb8535b458832cb"
 },
 {
  "q": "Which of these is the fastest storage?",
  "o": [
   "CPU cache",
   "RAM",
   "an NVMe SSD",
   "a hard disk"
  ],
  "s": "bbfae619cabd",
  "h": "ecdd89a510a5e81156a345115fb3b6a2"
 },
 {
  "q": "What does UEFI replace?",
  "o": [
   "the BIOS",
   "the bootloader",
   "the kernel",
   "the CPU microcode"
  ],
  "s": "bbece6969752",
  "h": "0bd23374b657286e1fc3687ec70846b0"
 },
 {
  "q": "How many bits are in a byte?",
  "o": [
   "8",
   "4",
   "16",
   "10"
  ],
  "s": "d411a9e86b9e",
  "h": "731f0c1e001dcc2e7993ee386b215600"
 },
 {
  "q": "What happens to what's in RAM when the power is cut?",
  "o": [
   "it's lost",
   "it's saved to disk automatically",
   "it stays for a day",
   "it moves to the GPU"
  ],
  "s": "f99a681a7a96",
  "h": "a5618df376615ce9e8f7a1a191164e9f"
 },
 {
  "q": "Which partition table can handle disks larger than 2 TB?",
  "o": [
   "GPT",
   "MBR",
   "FAT",
   "ext4"
  ],
  "s": "f26eccb5e39f",
  "h": "61635b6681e065f9620287a1b0ce6e4f"
 },
 {
  "q": "What is a GPU mainly built for?",
  "o": [
   "lots of calculations in parallel, like drawing graphics",
   "storing files",
   "networking",
   "keeping time"
  ],
  "s": "fe7068766712",
  "h": "3538a7194a69d81db102a4b9e4a4dd02"
 },
 {
  "q": "Who started the Linux kernel?",
  "o": [
   "Linus Torvalds",
   "Richard Stallman",
   "Dennis Ritchie",
   "Ken Thompson"
  ],
  "s": "0591767978df",
  "h": "e95df8d05a88adc41507098ca23aefe2"
 },
 {
  "q": "In what year was Linux first released?",
  "o": [
   "1991",
   "1985",
   "1996",
   "2001"
  ],
  "s": "db8992b10c99",
  "h": "4e07cfe0cc00830a0a86c48781e75507"
 },
 {
  "q": "What does GNU stand for?",
  "o": [
   "GNU's Not Unix",
   "General Network Utilities",
   "GNU New Unix",
   "Global Native Userland"
  ],
  "s": "c0de4973e46e",
  "h": "21b74551b6e6e94a11d33f99960c9b56"
 },
 {
  "q": "Which operating system family did Linux set out to be like?",
  "o": [
   "Unix",
   "Windows",
   "MS-DOS",
   "macOS"
  ],
  "s": "dc143a830ea3",
  "h": "db4397271ebb9c3d2007392d92ba0122"
 },
 {
  "q": "Where was the C programming language created?",
  "o": [
   "Bell Labs",
   "MIT",
   "Microsoft",
   "Xerox PARC"
  ],
  "s": "fea24a89b69d",
  "h": "3e311293d9bbf095a5722a0ab32817f4"
 },
 {
  "q": "What is the Linux mascot?",
  "o": [
   "a penguin",
   "a gnu",
   "a fox",
   "a daemon"
  ],
  "s": "5b8bbeb694eb",
  "h": "899f3539cc82348152a3755965bdd1f3"
 },
 {
  "q": "Which of these should you never share with anyone?",
  "o": [
   "your private key",
   "your public key",
   "your username",
   "your IP address on a home network"
  ],
  "s": "4768da98269e",
  "h": "d5d8ec5a7e87c13f2c1093a6711cabcb"
 },
 {
  "q": "What is phishing?",
  "o": [
   "tricking you into giving away secrets with fake messages or sites",
   "scanning a network",
   "overheating a CPU",
   "a type of backup"
  ],
  "s": "61ee2d498336",
  "h": "9f5c25d6bc692cf6e69257b492511142"
 },
 {
  "q": "What makes a password strong?",
  "o": [
   "long, unique and not reused anywhere",
   "short with one symbol",
   "your birthday backwards",
   "the same one everywhere so you remember it"
  ],
  "s": "fd11ec4355af",
  "h": "a77e1b3800afed5f8049cc770788aa32"
 },
 {
  "q": "What is 2 to the power of 10?",
  "o": [
   "1024",
   "1000",
   "512",
   "2048"
  ],
  "s": "39c7fe2ddc5a",
  "h": "629f68c028801240541985fd22495e1b"
 },
 {
  "q": "What is binary 1010 in decimal?",
  "o": [
   "10",
   "12",
   "5",
   "1010"
  ],
  "s": "6a0c0c42488d",
  "h": "07f75a1df97e2348b4b6b0130bbfe7b7"
 },
 {
  "q": "What is hexadecimal FF in decimal?",
  "o": [
   "255",
   "256",
   "15",
   "100"
  ],
  "s": "b99e2ab891d6",
  "h": "7573841e42e8df622d5c1c9748add48e"
 },
 {
  "q": "What comes next: 1, 1, 2, 3, 5, 8, …?",
  "o": [
   "13",
   "11",
   "12",
   "16"
  ],
  "s": "3afc290040f5",
  "h": "f4eb15db8298479b8c0b2e10cbcf381e"
 },
 {
  "q": "A 100 MB file downloads at 10 MB per second. How long does it take?",
  "o": [
   "10 seconds",
   "1 second",
   "100 seconds",
   "1000 seconds"
  ],
  "s": "7139cf4e862e",
  "h": "9d6ecf648be8cb2b45684611bd4d6121"
 },
 {
  "q": "Which C library does melon use?",
  "o": [
   "musl",
   "glibc",
   "uClibc",
   "bionic"
  ],
  "s": "050485b3e1e2",
  "h": "4076d8dde4d38f5ef7f0c3471bf3e85f"
 },
 {
  "q": "Which init system does melon use?",
  "o": [
   "runit",
   "systemd",
   "OpenRC",
   "sysvinit"
  ],
  "s": "2501925aba3f",
  "h": "c896dee967ac65e5a18dd377497db944"
 },
 {
  "q": "What is melon's package manager?",
  "o": [
   "apk",
   "pacman",
   "apt",
   "dnf"
  ],
  "s": "1c1802accc8d",
  "h": "6a718e2a3ade6fef6ac3e241a2fe2ed6"
 },
 {
  "q": "Which filesystem is melon's root on?",
  "o": [
   "XFS",
   "ext4",
   "btrfs",
   "ZFS"
  ],
  "s": "62971b120c87",
  "h": "b4f422febe9a0ea3d785599583d3f644"
 },
 {
  "q": "What is /bin/sh on melon?",
  "o": [
   "BusyBox ash",
   "bash",
   "dash",
   "zsh"
  ],
  "s": "6b48cb4bc8e4",
  "h": "3dbfc7e8a1603116a4cbb83b953f7e7c"
 },
 {
  "q": "Which desktop does melon ship?",
  "o": [
   "KDE Plasma",
   "GNOME",
   "Xfce",
   "Cinnamon"
  ],
  "s": "801ebe3c92c1",
  "h": "2264a4bb5b99f5a7d9da4bb42b9f23fa"
 },
 {
  "q": "How does melon run Steam?",
  "o": [
   "through Flatpak, from Flathub",
   "natively",
   "through Wine",
   "it can't"
  ],
  "s": "4cb75baa3e54",
  "h": "734b679f0b82f4d734fdc10832769d48"
 },
 {
  "q": "Why can't Steam run natively on melon?",
  "o": [
   "it needs glibc, and melon uses musl",
   "melon has no graphics drivers",
   "Steam doesn't support Linux",
   "melon is 32-bit"
  ],
  "s": "6128916ba570",
  "h": "5839c209413a4022c90f6bd5767dcd90"
 },
 {
  "q": "Which bootloader does melon use?",
  "o": [
   "GRUB",
   "systemd-boot",
   "LILO",
   "rEFInd"
  ],
  "s": "985385106087",
  "h": "0e671ba24a85ac270ecb69e154f51551"
 },
 {
  "q": "In the install gauntlet, what did a wrong answer do to you?",
  "o": [
   "sent you back 10 questions",
   "ended the install",
   "nothing",
   "sent you back to the start"
  ],
  "s": "207b32f11745",
  "h": "426883780e67521943302f2923867f93"
 }
];
