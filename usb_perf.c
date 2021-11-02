/* usb_perf.c - Compare smart src execution time between USB and non-USB. */
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

#include "lbm/lbm.h"
#include "um_perf.h"


/* Command-line options and their defaults */
static int o_affinity_cpu = 1;
static char *o_config = NULL;
static int o_msg_len = 25;
static int o_num_msgs = 10000000;
static int o_warmup_loops = 10000;


char usage_str[] = "Usage: usb_perf [-h] [-a affinity_cpu] [-c config] [-m msg_len] [-n num_msgs] [-w warmup_loops]";

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
      "  -c config : configuration file; can be repeated [%s]\n"
      "  -m msg_len : message length [%d]\n"
      "  -n num_msgs : number of messages to send [%d]\n"
      "  -w warmup_loops : messages to send before measurement loop [%d]\n"
      , o_affinity_cpu, (o_config == NULL) ? "" : o_config
      , o_msg_len, o_num_msgs
      , o_warmup_loops
  );
  exit(0);
}


void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  while ((opt = getopt(argc, argv, "ha:c:l:m:n:u:w:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': SAFE_ATOI(optarg, o_affinity_cpu); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c': if (o_config != NULL) { free(o_config); }
                o_config = strdup(optarg);
                E(lbm_config(o_config));
                break;
      case 'm': SAFE_ATOI(optarg, o_msg_len); break;
      case 'n': SAFE_ATOI(optarg, o_num_msgs); break;
      case 'w': SAFE_ATOI(optarg, o_warmup_loops); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  if (optind != argc) { usage("Extra parameter(s)"); }
}  /* get_my_opts */


uint64_t msg_buf[2*1024];  /* 16K bytes, aligned to 64-bit boundary. */

void um_ss_tight_loop(lbm_ssrc_t *ssrc, int use_usb, int num_loops)
{
  perf_msg_t *perf_msg;
  static lbm_ssrc_send_ex_info_t ssrc_exinfo;
  int i;

  lbm_set_lbtrm_src_loss_rate(100);

  /* Get buffer from from smart source pool. */
  E(lbm_ssrc_buff_get(ssrc, (char **)&perf_msg, 0));
  memset(&ssrc_exinfo, 0, sizeof(ssrc_exinfo));
  if (use_usb) {
    ssrc_exinfo.flags = LBM_SSRC_SEND_EX_FLAG_USER_SUPPLIED_BUFFER;
    ssrc_exinfo.usr_supplied_buffer = (char *)msg_buf;

    for (i = 0; i < num_loops; i++) {
      E(lbm_ssrc_send_ex(ssrc, (char *)perf_msg, o_msg_len, 0, &ssrc_exinfo));
    }
  }
  else {
    for (i = 0; i < num_loops; i++) {
      memcpy((char *)perf_msg, (char *)msg_buf, o_msg_len);
      E(lbm_ssrc_send_ex(ssrc, (char *)perf_msg, o_msg_len, 0, &ssrc_exinfo));
    }
  }

  E(lbm_ssrc_buff_put(ssrc, (char *)perf_msg));

  lbm_set_lbtrm_src_loss_rate(0);
}  /* um_ss_tight_loop */


int main(int argc, char **argv)
{
  cpu_set_t cpuset;
  lbm_context_t *ctx;
  lbm_topic_t *topic_obj;
  lbm_ssrc_t *ssrc = NULL;
  struct timespec start_ts;  /* struct timespec is used by clock_gettime(). */
  struct timespec end_ts;

  get_my_opts(argc, argv);

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_cpu=%d, o_config=%s, o_msg_len=%d, o_num_msgs=%d, o_warmup_loops=%d, \n",
      o_affinity_cpu, o_config, o_msg_len, o_num_msgs, o_warmup_loops);

  /* Publisher's UM objects. */
  E(lbm_context_create(&ctx, NULL, NULL, NULL));
  E(lbm_src_topic_alloc(&topic_obj, ctx, "usb_perf", NULL));
  E(lbm_ssrc_create(&ssrc, ctx, topic_obj, NULL, NULL, NULL));

  uint64_t usb_duration_ns, no_usb_duration_ns;
  CPU_ZERO(&cpuset);
  CPU_SET(o_affinity_cpu, &cpuset);
  errno = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  if (errno != 0) { PERRNO("pthread_setaffinity_np"); }

  um_ss_tight_loop(ssrc, 0, o_warmup_loops);

  clock_gettime(CLOCK_MONOTONIC, &start_ts);
  um_ss_tight_loop(ssrc, 0, o_num_msgs);
  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  DIFF_TS(no_usb_duration_ns, end_ts, start_ts);

  um_ss_tight_loop(ssrc, 1, o_warmup_loops);

  clock_gettime(CLOCK_MONOTONIC, &start_ts);
  um_ss_tight_loop(ssrc, 1, o_num_msgs);
  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  DIFF_TS(usb_duration_ns, end_ts, start_ts);

  printf("no_usb_duration_ns=%"PRIu64", usb_duration_ns=%"PRIu64"\n",
      no_usb_duration_ns, usb_duration_ns);

  E(lbm_ssrc_delete(ssrc));
  E(lbm_context_delete(ctx));

  return 0;
}  /* main */
