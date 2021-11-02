#!/bin/sh
# store_c.sh - run the store.

I="$1"
if [ ! -f "store_${I}c.xml" ]; then :
  echo >&2 "Store_c.sh: Error: 'store_${I}c.xml' not exist."
  exit 1
fi

AFFINITY="$2"

rm -rf /home/tmp/sford/store_${I}c.log
rm -rf /home/tmp/sford/cache${I}c
rm -rf /home/tmp/sford/state${I}c
mkdir -p /home/tmp/sford/cache${I}c
mkdir -p /home/tmp/sford/state${I}c
onload umestored -a "$AFFINITY" store_${I}c.xml
egrep unrecov /home/tmp/sford/store_${I}c.log
