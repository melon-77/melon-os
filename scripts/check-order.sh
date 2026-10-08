#!/bin/bash
# check-order.sh: every makedepends of every recipe must come from a recipe that build-everything.sh builds
# earlier. A package built before something it needs either fails (the next pass retries it) or, worse, quietly
# leaves features out (libxkbcommon without libxml2 has no xkbregistry).
M=$(cd "$(dirname "$0")/.." && pwd)
cd $M
# (MELON_ARCH=x86: the 32-bit list at the end of that block)
ALL=$(M=$M bash -c 'eval "$(sed -n "/^BASE=/,/^# recipes nobody listed/p" "$M/scripts/build-everything.sh")"; printf "%s\n" $ALL')
declare -A pos origin
i=0; for r in $ALL; do pos[$r]=$i; i=$((i+1)); done
# which recipe makes which package: the recipe itself and its subpackages
for r in $ALL; do
  f=recipes/$r/MELONBUILD; [ -f $f ] || continue
  origin[$r]=$r
  for sp in $(bash -c ". $f >/dev/null 2>&1; echo \${subpackages[@]}"); do origin[$sp]=$r; done
done
bad=0
for r in $ALL; do
  f=recipes/$r/MELONBUILD; [ -f $f ] || continue
  for d in $(bash -c ". $f >/dev/null 2>&1; echo \${makedepends[@]}"); do
    o=${origin[$d]:-}
    if [ -z "$o" ]; then echo "check-order: $r needs $d, which no recipe in the list makes"; bad=1
    elif [ ${pos[$o]} -ge ${pos[$r]} ]; then echo "check-order: $r (#${pos[$r]}) needs $d from $o (#${pos[$o]}), built later"; bad=1; fi
  done
done
[ $bad = 0 ] && echo "check-order: $(echo $ALL | wc -w) recipes, every makedepends built first"
exit $bad
