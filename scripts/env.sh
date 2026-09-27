# melon build environment
export M=/home/claude/melon
export TARGET=x86_64-melon-linux-musl
export TOOLS=$M/tools
export SYSROOT=$M/sysroot
export SRC=$M/sources
export WORK=$M/work
export REPO=$M/repo
export PATH=$TOOLS/bin:$PATH
export JOBS=$(nproc)
