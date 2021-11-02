/* um_perf_sub.c - measure one-way latency under load (subscriber). */
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
static int o_affinity_cpu = 0;
static char *o_config = NULL;  /* Default set in "get_my_opts()". */
static char *o_persist = NULL;
static int o_spin_cnt = 0;
static char *o_topics = NULL;  /* Default set in "get_my_opts()". */
static char *o_xml_config = NULL;  /* Default set in "get_my_opts()". */

/* Globals. The code depends on the loader initializing them to all zeros. */
char *app_name;

char usage_str[] = "Usage: um_perf_sub [-h] [-a affinity_cpu] [-c config] [-s spin_cnt] [-t topic] [-x xml_config]";

void usage(char *msg) {
  if (msg) fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "%s\n", usage_str);
  exit(1);
}

void help() {
  fprintf(stderr, "%s\n", usage_str);
  fprintf(stderr, "where:\n"
      "  -h : print help\n"
      "  -a affinity_cpu : CPU number (0..N-1) for receive thread [%d]\n"
      "  -c config : configuration file; can be repeated [%s]\n"
      "  -p ''|r|s : persist mode (empty=streaming, r=RPP, s=SPP) [%s]\n"
      "  -s spin_cnt : empty loop inside receiver callback [%d]\n"
      "  -t topics : comma-separated topic strings [%s]\n"
      "  -x xml_config : configuration file [%s]\n"
      , o_affinity_cpu, o_config, o_persist, o_spin_cnt , o_topics, o_xml_config
  );
  exit(0);
}


void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  /* Set defaults for string options. */
  o_config = strdup("");
  o_persist = strdup("");
  o_topics = strdup("");
  o_xml_config = strdup("");

  while ((opt = getopt(argc, argv, "ha:c:p:s:t:x:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': SAFE_ATOI(optarg, o_affinity_cpu); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c': free(o_config);
                o_config = strdup(optarg);
                E(lbm_config(o_config));  /* Allow multiple calls. */
                break;
      case 'p': free(o_persist); o_persist = strdup(optarg); break;
      case 's': SAFE_ATOI(optarg, o_spin_cnt); break;
      case 't': free(o_topics); o_topics = strdup(optarg); break;
      case 'x': free(o_xml_config); o_xml_config = strdup(optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  if (strlen(o_persist) == 0) {
    app_name = "um_perf";
  }
  else if (strcmp(o_persist, "r") == 0) {
    app_name = "um_perf_rpp";
  }
  else if (strcmp(o_persist, "s") == 0) {
    app_name = "um_perf_spp";
  }
  else {
    usage("Error, -p value must be '', 'r', or 's'\n");
  }

  if (strlen(o_xml_config) > 0) {
    /* Unlike lbm_config(), you can't load more than one XML file.
     * If user supplied -x more than once, only load last one. */
    E(lbm_config_xml_file(o_xml_config, app_name));
  }

  if (optind != argc) { usage("Extra parameter(s)"); }
}  /* get_my_opts */


/* This "counter" is made global to force the optimizer to update it. */
int global_counter;
int rcv_callback(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  static uint64_t num_rcv_msgs;
  static uint64_t num_rx_msgs;
  static uint64_t num_unrec_loss;
  static uint64_t min_latency;
  static uint64_t max_latency;
  static uint64_t sum_latencies;  /* For calculating average latencies. */
  static uint64_t num_timestamps; /* For calculating average latencies. */
  cpu_set_t cpuset;

  switch (msg->type) {
  case LBM_MSG_BOS:
    /* Assume receive thread is calling this; pin the time-critical thread
     * to the requested CPU. */
    CPU_ZERO(&cpuset);
    CPU_SET(o_affinity_cpu, &cpuset);
    errno = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (errno != 0) { PERRNO("pthread_setaffinity_np"); }

    num_rcv_msgs = 0;
    num_rx_msgs = 0;
    num_unrec_loss = 0;
    min_latency = (uint64_t)-1;  /* max int */
    max_latency = 0;
    sum_latencies = 0;
    num_timestamps = 0;
    printf("rcv event BOS, topic_name='%s', source=%s, \n",
      msg->topic_name, msg->source);
    fflush(stdout);
    break;

  case LBM_MSG_EOS:
    if (num_timestamps > 0) {
      printf("rcv event EOS, '%s', %s, num_rcv_msgs=%"PRIu64", num_rx_msgs=%"PRIu64", num_unrec_loss=%"PRIu64", min_latency=%"PRIu64", max_latency=%"PRIu64", average latency=%"PRIu64", \n",
          msg->topic_name, msg->source, num_rcv_msgs, num_rx_msgs, num_unrec_loss,
          min_latency, max_latency, sum_latencies / num_timestamps);
    } else {
      printf("rcv event EOS, '%s', %s, num_rcv_msgs=%"PRIu64", num_rx_msgs=%"PRIu64", num_unrec_loss=%"PRIu64",\n",
          msg->topic_name, msg->source, num_rcv_msgs, num_rx_msgs, num_unrec_loss);
    }
    fflush(stdout);
    break;

  case LBM_MSG_UME_REGISTRATION_ERROR:
  {
    printf("rcv event LBM_MSG_UME_REGISTRATION_ERROR, '%s', %s, msg='%s'\n",
        msg->topic_name, msg->source, (char *)msg->data);
    break;
  }

  case LBM_MSG_UNRECOVERABLE_LOSS:
  {
    num_unrec_loss++;
    break;
  }

  case LBM_MSG_DATA:
  {
    perf_msg_t *perf_msg = (perf_msg_t *)msg->data;

    if ((perf_msg->flags & FLAGS_TIMESTAMP) == FLAGS_TIMESTAMP) {
      struct timespec cur_ts;
      uint64_t diff_ns;
      /* Calculate one-way latency for this message. */
      clock_gettime(CLOCK_MONOTONIC, &cur_ts);
      DIFF_TS(diff_ns, cur_ts, perf_msg->send_ts);

      if (diff_ns < min_latency) min_latency = diff_ns;
      if (diff_ns > max_latency) max_latency = diff_ns;
      sum_latencies += diff_ns;
      num_timestamps++;
    }

    num_rcv_msgs++;
    if ((msg->flags & LBM_MSG_FLAG_RETRANSMIT) == LBM_MSG_FLAG_RETRANSMIT) {
      num_rx_msgs++;
    }
 
    /* This "counter" loop is to introduce short delays into the receiver. */
    if (o_spin_cnt > 0) {
      for (global_counter = 0; global_counter < o_spin_cnt; global_counter++) {
      }
    }
    break;
  }

  default:
    printf("rcv event %d, topic_name='%s', source=%s, \n", msg->type, msg->topic_name, msg->source); fflush(stdout);
  }  /* switch msg->type */

  return 0;
}  /* rcv_callback */


int main(int argc, char **argv)
{
  lbm_context_t *ctx;
  lbm_rcv_topic_attr_t *rcv_attr;
  lbm_topic_t *topic_obj;
#define MAX_RCVS 16
  lbm_rcv_t *rcvs[MAX_RCVS];
  int num_rcvs = 0;

  get_my_opts(argc, argv);

  printf("o_affinity_cpu=%d, o_config=%s, o_persist='%s', o_spin_cnt=%d, o_topics='%s', o_xml_config=%s, \n",
      o_affinity_cpu, o_config, o_persist, o_spin_cnt, o_topics, o_xml_config);

  /* Create UM context. */
  E(lbm_context_create(&ctx, NULL, NULL, NULL));

  /* Set some options in code. */
  E(lbm_rcv_topic_attr_create(&rcv_attr));

  E(lbm_rcv_topic_attr_str_setopt(rcv_attr, "ume_session_id", "0x6"));

  char *work_string = strdup(o_topics);
  char *cur_topic = strtok(work_string, ",");
  while (cur_topic != NULL) {
    ASSRT(strlen(cur_topic) > 0);
    ASSRT(num_rcvs < MAX_RCVS);
    E(lbm_rcv_topic_lookup(&topic_obj, ctx, cur_topic, rcv_attr));
    E(lbm_rcv_create(&rcvs[num_rcvs], ctx, topic_obj, rcv_callback, NULL, NULL));

    num_rcvs++;
    cur_topic = strtok(NULL, ",");
  }

  /* The subscriber must be "kill"ed externally. */
  sleep(2000000000);  /* 23+ centuries. */

  /* Should delete receivers and context. */

  return 0;
}  /* main */
