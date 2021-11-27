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

#include "cprt.h"
#include <stdio.h>
#include <string.h>
#if ! defined(_WIN32)
  #include <stdlib.h>
  #include <unistd.h>
#endif

#include "um_perf.h"


/* Command-line options and their defaults */
static int o_affinity_cpu = -1;
static char *o_histogram = NULL;
static int o_jitter_loops = 10000000;
static int o_malloc_size = 0;
static int o_spin_cnt = 0;

/* Parameters parsed out from command-line options. */
int histo_num_buckets;
int histo_ns_per_bucket;

char usage_str[] = "Usage: um_perf_jitter [-h] [-a affinity_cpu] [-H histo_num_buckets,histo_ns_per_bucket] [-j jitter_loops] [-m malloc_size] [-s spin_cnt]";

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
      "  -H histo_num_buckets,histo_ns_per_bucket : send time histogram [%s]\n"
      "  -j jitter_loops : jitter measurement loops [%d]\n"
      "  -m malloc_size : do mallocs (size) [%d]\n"
      "  -s spin_cnt : spin loops inside one jitter loop [%d]\n"
      , o_affinity_cpu, o_histogram, o_jitter_loops, o_malloc_size, o_spin_cnt
  );
  exit(0);
}


void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  /* Set defaults for string options. */
  o_histogram = CPRT_STRDUP("0,0");

  while ((opt = getopt(argc, argv, "ha:H:j:m:s:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': CPRT_ATOI(optarg, o_affinity_cpu); break;
      case 'H': free(o_histogram); o_histogram = CPRT_STRDUP(optarg); break;
      case 'j': CPRT_ATOI(optarg, o_jitter_loops); break;
      case 'm': CPRT_ATOI(optarg, o_malloc_size); break;
      case 's': CPRT_ATOI(optarg, o_spin_cnt); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  if (optind != argc) { usage("Extra parameter(s)"); }

  char *strtok_context;

  char *work_str = CPRT_STRDUP(o_histogram);
  char *histo_num_buckets_str = CPRT_STRTOK(work_str, ",", &strtok_context);
  ASSRT(histo_num_buckets_str != NULL);
  CPRT_ATOI(histo_num_buckets_str, histo_num_buckets);

  char *histo_ns_per_bucket_str = CPRT_STRTOK(NULL, ",", &strtok_context);
  ASSRT(histo_ns_per_bucket_str != NULL);
  CPRT_ATOI(histo_ns_per_bucket_str, histo_ns_per_bucket);

  ASSRT(CPRT_STRTOK(NULL, ",", &strtok_context) == NULL);
  free(work_str);
}  /* get_my_opts */


int *histo_buckets = NULL;
int histo_min_sample = 999999999;
int histo_max_sample = 0;
int histo_overflows = 0;  /* Number of values above the last bucket. */
int histo_num_samples = 0;
uint64_t histo_sample_sum = 0;

void histo_init()
{
  /* Re-initialize the data. */
  histo_min_sample = 999999999;
  histo_max_sample = 0;
  histo_overflows = 0;  /* Number of values above the last bucket. */
  histo_num_samples = 0;
  histo_sample_sum = 0;

  int i;
  for (i = 0; i < histo_num_buckets; i++) {
    histo_buckets[i] = 0;
  }
}  /* histo_init */

void histo_create()
{
  histo_buckets = (int *)malloc(histo_num_buckets * sizeof(int));

  histo_init();
}  /* histo_create */

void histo_input(int in_sample)
{
  ASSRT(histo_buckets != NULL);

  histo_num_samples++;
  histo_sample_sum += in_sample;

  if (in_sample > histo_max_sample) {
    histo_max_sample = in_sample;
  }
  if (in_sample < histo_min_sample) {
    histo_min_sample = in_sample;
  }

  int bucket = in_sample / histo_ns_per_bucket;
  if (bucket >= histo_num_buckets) {
    histo_overflows++;
  }
  else {
    histo_buckets[bucket]++;
  }
}  /* histo_input */

void histo_print()
{
  int i;
  for (i = 0; i < histo_num_buckets; i++) {
    printf("%d\n", histo_buckets[i]);
  }
  printf("histo_overflows=%d, histo_min_sample=%d, histo_max_sample=%d,\n",
      histo_overflows, histo_min_sample, histo_max_sample);
  uint64_t average_sample = histo_sample_sum / (uint64_t)histo_num_samples;
  printf("histo_num_samples=%d, average_sample=%d,\n",
      histo_num_samples, (int)average_sample);
}  /* histo_print */


void histo_test()
{
  histo_input(1);
  histo_input(histo_num_buckets * histo_ns_per_bucket - 1);
  histo_print();
  histo_input(histo_num_buckets * histo_ns_per_bucket);
  histo_print();
}  /* histo_test */


int global_cnt;  /* This is global so that compiler doesn't optimize it out. */

/* Measure the minimum and maximum duration of a timestamp. */
void jitter_loop()
{
  uint64_t ts_min_ns = 999999999;
  uint64_t ts_max_ns = 0;
  struct timespec ts1;
  struct timespec ts2;
#define NUM_MALLOCS 10000
  char *mallocs[NUM_MALLOCS];
  int i;

  int do_histogram = 0;
  if (histo_buckets != NULL) {
      do_histogram = 1;
  }

  int malloc_size = o_malloc_size;  /* Local var for speed. */
  int spin_cnt = o_spin_cnt;  /* Local var for speed. */

  if (malloc_size > 0) {
    for (i = 0; i < NUM_MALLOCS; i++) {
      mallocs[i] = NULL;
    }
  }

  /* Warm up the cache. */
  CPRT_GETTIME(&ts1);
  CPRT_GETTIME(&ts2);
  CPRT_GETTIME(&ts1);
  CPRT_GETTIME(&ts2);

  for (i = 0; i < o_jitter_loops; i++) {
    uint64_t ts_this_ns;

    /* Two timestamps in a row measures the duration of the timestamp. */
    CPRT_GETTIME(&ts1);

    if (malloc_size > 0) {
      if (mallocs[i % NUM_MALLOCS] != NULL) {
        free(mallocs[i % NUM_MALLOCS]);
      }
      mallocs[i % NUM_MALLOCS] = malloc(o_malloc_size);
      *mallocs[i % NUM_MALLOCS] = 1;
    }
    if (spin_cnt > 0) {
      for (global_cnt = 0; global_cnt < spin_cnt; global_cnt++) {
      }
    }

    CPRT_GETTIME(&ts2);

    CPRT_DIFF_TS(ts_this_ns, ts2, ts1);
    if (do_histogram) {
      histo_input((int)ts_this_ns);
    }
    /* Track maximum and minimum. */
    if (ts_this_ns < ts_min_ns) ts_min_ns = ts_this_ns;
    if (ts_this_ns > ts_max_ns) ts_max_ns = ts_this_ns;
  }  /* for i */

  if (do_histogram) {
    histo_print();
  }
  printf("ts_min_ns=%"PRIu64", ts_max_ns=%"PRIu64", \n",
      ts_min_ns, ts_max_ns);
}  /* jitter_loop */


int main(int argc, char **argv)
{
  uint64_t cpuset;
  CPRT_NET_START;

  get_my_opts(argc, argv);

  if (histo_num_buckets > 0) {
    histo_create();
  }

  if (o_affinity_cpu > -1) {
    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(o_affinity_cpu, &cpuset);
    cprt_set_affinity(cpuset);
  }

  jitter_loop();

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_cpu=%d, o_histogram=%s, o_jitter_loops=%d, o_malloc_size=%d, o_spin_cnt=%d, \n",
      o_affinity_cpu, o_histogram, o_jitter_loops, o_malloc_size, o_spin_cnt);

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
