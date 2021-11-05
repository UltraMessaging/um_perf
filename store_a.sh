#!/bin/sh
# store_a.sh - run the store.

I="$1"
if [ ! -f "store_${I}a.xml" ]; then :
  echo >&2 "Store_a.sh: Error: 'store_${I}a.xml' not exist."
  exit 1
fi

AFFINITY="$2"

rm -rf /home/tmp/sford/store_${I}a.log
rm -rf /home/tmp/sford/cache${I}a
rm -rf /home/tmp/sford/state${I}a
if [ "`hostname`" = "zeus" ]; then :
  rm -rf /mnt/sdc/data/sford/cache${I}a
  mkdir /mnt/sdc/data/sford/cache${I}a
  ln -s /mnt/sdc/data/sford/cache${I}a /home/tmp/sford/cache${I}a
else :
  mkdir -p /home/tmp/sford/cache${I}a
fi
mkdir -p /home/tmp/sford/state${I}a
onload umestored -a "$AFFINITY" store_${I}a.xml
egrep unrecov /home/tmp/sford/store_${I}a.log
