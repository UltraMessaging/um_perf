/* um_perf_jitter.c */
/*
  Copyright (c) 2021 Informatica Corporation
  Permission is granted to licensees to use or alter this software for any
  purpose, including commercial applications, according to the terms laid
  out in the Software License Agreement.

  This source code example is provided by Informatica for educational
  and evaluation purposes only.

  THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES 
  EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF 
  NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR 
  PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE 
  UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES,
  BE LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR 
  INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE 
  TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF 
  THE LIKELIHOOD OF SUCH DAMAGES.
*/

/* This is needed for affinity setting. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "um_perf.h"


/* Command-line options and their defaults */
static int o_affinity_cpu = 1;
static int o_jitter_loops = 10000000;
static int o_spin_cnt = 0;


char usage_str[] = "Usage: um_perf_jitter [-h] [-a affinity_cpu] [-j jitter_loops] [-s spin_cnt]";

void usage(char *msg) {
  if (msg) fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "%s\n", usage_str);
  exit(1);
}

void help() {
  fprintf(stderr, "%s\n", usage_str);
  fprintf(stderr, "where:\n"
      "  -h : print help\n"
      "  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]\n"
      "  -j jitter_loops : jitter measurement loops [%d]\n"
      "  -s spin_cnt : spin loops inside one jitter loop [%d]\n"
      , o_affinity_cpu , o_jitter_loops, o_spin_cnt
  );
  exit(0);
}


void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  while ((opt = getopt(argc, argv, "ha:j:s:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': SAFE_ATOI(optarg, o_affinity_cpu); break;
      case 'j': SAFE_ATOI(optarg, o_jitter_loops); break;
      case 's': SAFE_ATOI(optarg, o_spin_cnt); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  if (optind != argc) { usage("Extra parameter(s)"); }
}  /* get_my_opts */


int global_cnt;  /* This is global so that compiler doesn't optimize it out. */

/* Measure the minimum and maximum duration of a timestamp. */
void jitter_loop()
{
  uint64_t ts_min_ns = 999999999;
  uint64_t ts_max_ns = 0;
  struct timespec ts1;
  struct timespec ts2;
  int i, spinner;

  clock_gettime(CLOCK_MONOTONIC, &ts1);
  clock_gettime(CLOCK_MONOTONIC, &ts2);
  clock_gettime(CLOCK_MONOTONIC, &ts1);
  clock_gettime(CLOCK_MONOTONIC, &ts2);

  for (i = 0; i < o_jitter_loops; i++) {
    uint64_t ts_this_ns;

    /* Two timestamps in a row measures the duration of the timestamp. */
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    for (spinner = 0; spinner < o_spin_cnt; spinner++) {
      global_cnt++;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts2);

    DIFF_TS(ts_this_ns, ts2, ts1);
    /* Track maximum and minimum (average not needed). */
    if (ts_this_ns < ts_min_ns) ts_min_ns = ts_this_ns;
    if (ts_this_ns > ts_max_ns) ts_max_ns = ts_this_ns;
  }  /* for i */

  printf("ts_min_ns=%"PRIu64", ts_max_ns=%"PRIu64", \n",
      ts_min_ns, ts_max_ns);
}  /* jitter_loop */


int main(int argc, char **argv)
{
  cpu_set_t cpuset;

  get_my_opts(argc, argv);

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_cpu=%d, o_jitter_loops=%d, o_spin_cnt=%d, \n",
      o_affinity_cpu, o_jitter_loops, o_spin_cnt);

  CPU_ZERO(&cpuset);
  CPU_SET(o_affinity_cpu, &cpuset);
  errno = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  if (errno != 0) { PERRNO("pthread_setaffinity_np"); }

  jitter_loop();

  exit(0);
}  /* main */
