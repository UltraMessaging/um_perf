#!/bin/sh
# store_b.sh - run the store.

I="$1"
if [ ! -f "store_${I}b.xml" ]; then :
  echo >&2 "Store_b.sh: Error: 'store_${I}b.xml' not exist."
  exit 1
fi

AFFINITY="$2"

rm -rf /home/tmp/sford/store_${I}b.log
rm -rf /home/tmp/sford/cache${I}b
rm -rf /home/tmp/sford/state${I}b
mkdir -p /home/tmp/sford/cache${I}b
mkdir -p /home/tmp/sford/state${I}b
onload umestored -a "$AFFINITY" store_${I}b.xml
egrep unrecov /home/tmp/sford/store_${I}b.log
