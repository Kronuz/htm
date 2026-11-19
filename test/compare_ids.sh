#!/bin/sh
# Ids of random points (off every edge) from two checkouts of htm must be identical.
# Run from a configured checkout (cmake -B build), whose build/_deps holds cartesian:
#
#   test/compare_ids.sh OTHER_CHECKOUT [points]
set -eu
other=$1
n=${2:-100000}
cart=build/_deps/cartesian-src
for side in other this; do
	if [ "$side" = other ]; then src=$other; else src=.; fi
	c++ -std=c++20 -O2 -I"$src" -I"$cart" test/ids_dump.cc "$src/htm.cc" "$src/constraint.cc" \
		"$cart/cartesian.cc" -o "build/ids_$side"
	"build/ids_$side" "$n" > "build/ids_$side.txt"
done
lines=$(wc -l < build/ids_this.txt)
cmp build/ids_other.txt build/ids_this.txt
echo "ids identical on $lines random points"
