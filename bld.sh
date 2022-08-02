#!/bin/sh
# bld.sh - build the programs on Linux.

. $HOME/lbm.sh

if [ "`uname`" = "Darwin" ]; then :
  LIBS="-L $LBM/lib -l lbm -l pthread -l m"
else : 
  LIBS="-L $LBM/lib -l lbm -pthread -l m -l rt"
fi

gcc -Wall -g -o um_perf_jitter cprt.c um_perf_jitter.c $LIBS 
if [ $? -ne 0 ]; then echo error in um_perf_jitter.c; exit 1; fi

gcc -Wall -g -I $LBM/include -I $LBM/include/lbm -o um_perf_pub cprt.c um_perf_pub.c $LIBS
if [ $? -ne 0 ]; then echo error in um_perf_pub.c; exit 1; fi

gcc -Wall -g -I $LBM/include -I $LBM/include/lbm -o um_perf_sub cprt.c um_perf_sub.c $LIBS
if [ $? -ne 0 ]; then echo error in um_perf_sub.c; exit 1; fi

gcc -Wall -g -o sock_perf_pub cprt.c sock_perf_pub.c $LIBS
if [ $? -ne 0 ]; then echo error in sock_perf_pub.c; exit 1; fi

gcc -Wall -g -o sock_perf_pub2 cprt.c sock_perf_pub2.c $LIBS
if [ $? -ne 0 ]; then echo error in sock_perf_pub2.c; exit 1; fi
