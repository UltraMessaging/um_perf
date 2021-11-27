#!/bin/sh
# bld.sh - build the programs on Linux.

. ./lbm.sh
LBM=$LBM_PLATFORM

# LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.

# LBM=$HOME/UMP_6.10.0.1/Linux-glibc-2.5-x86_64  # Modify according to your needs.

if [ "`uname`" = "Darwin" ]; then :
  LIBS="-l pthread -l m"
else : 
  LIBS="-l pthread -l m -l rt"
fi

gcc -Wall -g $LIBS \
    -o um_perf_jitter cprt.c um_perf_jitter.c
if [ $? -ne 0 ]; then echo um_perf_jitter.c; exit 1; fi

gcc -Wall -g -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l lbm $LIBS \
    -o um_perf_pub cprt.c um_perf_pub.c
if [ $? -ne 0 ]; then echo um_perf_pub.c; exit 1; fi

gcc -Wall -g -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l lbm $LIBS \
    -o um_perf_sub cprt.c um_perf_sub.c
if [ $? -ne 0 ]; then echo um_perf_sub.c; exit 1; fi

gcc -Wall -g $LIBS \
    -o sock_perf_pub cprt.c sock_perf_pub.c
if [ $? -ne 0 ]; then echo sock_perf_pub.c; exit 1; fi
