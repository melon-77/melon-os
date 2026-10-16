#!/bin/sh
# Builds melon-run.tns from melon-run.lua with Luna (github.com/ndless-nspire/Luna, `make` needs zlib),
# after the desktop tests. LUNA=/path/to/luna if it isn't on PATH.
set -e
cd "$(dirname "$0")"
LUNA=${LUNA:-luna}
LUA=${LUA:-lua5.1}
luac5.1 -p melon-run.lua 2>/dev/null || $LUA -e 'assert(loadfile("melon-run.lua"))'
$LUA tools/sim.lua bot
$LUA tools/sim.lua stress
$LUNA melon-run.lua melon-run.tns
ls -l melon-run.tns
