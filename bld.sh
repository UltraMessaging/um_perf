#!/bin/sh
# bld.sh - build the programs on Linux.

# LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.

LBM=$HOME/UMP_6.10.0.1/Linux-glibc-2.5-x86_64  # Modify according to your needs.

export LD_LIBRARY_PATH=$LBM/lib

gcc -Wall -I $LBM/include -I $LBM/include/lbm -l pthread -l m \
    -l rt -o um_perf_jitter um_perf_jitter.c
if [ $? -ne 0 ]; then exit 1; fi

gcc -Wall -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l pthread -l lbm -l m \
    -l rt -o um_perf_pub um_perf_pub.c
if [ $? -ne 0 ]; then exit 1; fi

gcc -Wall -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l pthread -l lbm -l m \
    -l rt -o um_perf_sub um_perf_sub.c
if [ $? -ne 0 ]; then exit 1; fi
