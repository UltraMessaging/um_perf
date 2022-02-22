/* um_perf_pub.c - performance measurement tool. */
/*
  Copyright (c) 2021-2022 Informatica Corporation
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

#include "lbm/lbm.h"
#include "um_perf.h"

#if defined(PRINT4)
void histo_print4();
#endif


/* Command-line options and their defaults. String defaults are set
 * in "get_my_opts()".
 */
static int o_affinity_cpu = -1;
static char *o_config = NULL;
static int o_generic_src = 0;
static char *o_histogram = NULL;  /* -H */
static int o_linger_ms = 1000;
static int o_loss_percent = 0;  /* -L */
static int o_msg_len = 0;
static int o_num_msgs = 0;
static char *o_persist = NULL;
static int o_rate = 0;
static char *o_topics = NULL;
static char *o_warmup = NULL;
static char *o_xml_config = NULL;

/* Parameters parsed out from command-line options. */
char *app_name;
int hist_num_buckets;
int hist_ns_per_bucket;
int warmup_loops;
int warmup_rate;

/* Globals. The code depends on the loader initializing them to all zeros. */
char *msg_buf;
perf_msg_t *perf_msg;
int global_max_tight_sends;
int registration_complete;
int cur_flight_size;
int max_flight_size;


char usage_str[] = "Usage: um_perf_pub [-h] [-a affinity_cpu] [-c config] [-g] [-H hist_num_buckets,hist_ns_per_bucket] [-l linger_ms] [-L loss_percent] [-m msg_len] [-n num_msgs] [-p persist_mode] [-r rate] [-t topics] [-w warmup_loops,warmup_rate] [-x xml_config]";

void usage(char *msg) {
  if (msg) fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "%s\n", usage_str);
  CPRT_NET_CLEANUP;
  exit(1);
}

void help() {
  fprintf(stderr, "%s\n", usage_str);
  fprintf(stderr, "where:\n"
      "  -h : print help\n"
      "  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]\n"
      "  -c config : configuration file; can be repeated [%s]\n"
      "  -g : generic source [%d]\n"
      "  -H hist_num_buckets,hist_ns_per_bucket : send time histogram [%s]\n"
      "  -l linger_ms : linger time before source delete [%d]\n"
      "  -L loss_percent : Source-side artificial packet loss (after warmup) [%d]\n"
      "  -m msg_len : message length [%d]\n"
      "  -n num_msgs : number of messages to send [%d]\n"
      "  -p ''|r|s : persist mode (empty=streaming, r=RPP, s=SPP) [%s]\n"
      "  -r rate : messages per second to send [%d]\n"
      "  -t topics : comma-separated topic strings [\"%s\"]\n"
      "  -w warmup_loops,warmup_rate : messages to send before measurement [%s]\n"
      "  -x xml_config : XML configuration file [%s]\n"
      , o_affinity_cpu, o_config, o_generic_src, o_histogram, o_linger_ms
      , o_loss_percent, o_msg_len, o_num_msgs, o_persist, o_rate, o_topics
      , o_warmup, o_xml_config
  );
  CPRT_NET_CLEANUP;
  exit(0);
}


/* Process command-line options. */
void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  /* Set defaults for string options. */
  o_config = CPRT_STRDUP("");
  o_histogram = CPRT_STRDUP("0,0");
  o_persist = CPRT_STRDUP("");
  o_topics = CPRT_STRDUP("");
  o_warmup = CPRT_STRDUP("0,0");
  o_xml_config = CPRT_STRDUP("");

  while ((opt = getopt(argc, argv, "ha:c:gH:l:L:m:n:p:r:t:w:x:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': CPRT_ATOI(optarg, o_affinity_cpu); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c': free(o_config);
                o_config = CPRT_STRDUP(optarg);
                E(lbm_config(o_config));
                break;
      case 'g': o_generic_src = 1; break;
      case 'H': free(o_histogram); o_histogram = CPRT_STRDUP(optarg); break;
      case 'l': CPRT_ATOI(optarg, o_linger_ms); break;
      case 'L': CPRT_ATOI(optarg, o_loss_percent); break;
      case 'm': CPRT_ATOI(optarg, o_msg_len); break;
      case 'n': CPRT_ATOI(optarg, o_num_msgs); break;
      case 'p': free(o_persist); o_persist = CPRT_STRDUP(optarg); break;
      case 'r': CPRT_ATOI(optarg, o_rate); break;
      case 't': free(o_topics); o_topics = CPRT_STRDUP(optarg); break;
      case 'w': free(o_warmup); o_warmup = CPRT_STRDUP(optarg); break;
      case 'x': free(o_xml_config); o_xml_config = CPRT_STRDUP(optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  /* Must supply certain required "options". */
  ASSRT(o_rate > 0);
  ASSRT(o_num_msgs > 0);
  ASSRT(o_msg_len > 0);
  ASSRT(strlen(o_topics) > 0);  /* o_topics is parsed in create_sources(). */

  char *strtok_context;

  /* Parse the histogram option: "hist_num_buckets,hist_ns_per_bucket". */
  char *work_str = CPRT_STRDUP(o_histogram);
  char *hist_num_buckets_str = CPRT_STRTOK(work_str, ",", &strtok_context);
  ASSRT(hist_num_buckets_str != NULL);
  CPRT_ATOI(hist_num_buckets_str, hist_num_buckets);

  char *hist_ns_per_bucket_str = CPRT_STRTOK(NULL, ",", &strtok_context);
  ASSRT(hist_ns_per_bucket_str != NULL);

  CPRT_ATOI(hist_ns_per_bucket_str, hist_ns_per_bucket);

  ASSRT(CPRT_STRTOK(NULL, ",", &strtok_context) == NULL);
  free(work_str);
  if (hist_num_buckets > 0) { ASSRT(hist_ns_per_bucket > 0); }

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

  /* Parse the warmup option: "warmup_loops,warmup_rate". */
  work_str = CPRT_STRDUP(o_warmup);
  char *warmup_loops_str = CPRT_STRTOK(work_str, ",", &strtok_context);
  ASSRT(warmup_loops_str != NULL);
  CPRT_ATOI(warmup_loops_str, warmup_loops);

  char *warmup_rate_str = CPRT_STRTOK(NULL, ",", &strtok_context);
  ASSRT(warmup_rate_str != NULL);
  CPRT_ATOI(warmup_rate_str, warmup_rate);

  ASSRT(CPRT_STRTOK(NULL, ",", &strtok_context) == NULL);
  free(work_str);
  if (warmup_loops > 0) { ASSRT(warmup_rate > 0); }

  if (strlen(o_xml_config) > 0) {
    /* Unlike lbm_config(), you can't load more than one XML file.
     * If user supplied -x more than once, only load last one. */
    E(lbm_config_xml_file(o_xml_config, app_name));
  }

  if (optind != argc) { usage("Unexpected positional parameter(s)"); }
}  /* get_my_opts */


/* Histogram. */
int *hist_buckets = NULL;
int hist_min_sample = 999999999;
int hist_max_sample = 0;
int hist_overflows = 0;  /* Number of values above the last bucket. */
int hist_num_samples = 0;
uint64_t hist_sample_sum = 0;

void hist_init()
{
  /* Re-initialize the data. */
  hist_min_sample = 999999999;
  hist_max_sample = 0;
  hist_overflows = 0;  /* Number of values above the last bucket. */
  hist_num_samples = 0;
  hist_sample_sum = 0;

  /* Init histogram (also makes sure it is mapped to physical memory. */
  int i;
  for (i = 0; i < hist_num_buckets; i++) {
    hist_buckets[i] = 0;
  }
}  /* hist_init */

void hist_create()
{
  hist_buckets = (int *)malloc(hist_num_buckets * sizeof(int));

  hist_init();
}  /* hist_create */

void hist_input(int in_sample)
{
  ASSRT(hist_buckets != NULL);
  ASSRT(in_sample > 0);

  hist_num_samples++;
  hist_sample_sum += in_sample;

  if (in_sample > hist_max_sample) {
    hist_max_sample = in_sample;
  }
  if (in_sample < hist_min_sample) {
    hist_min_sample = in_sample;
  }

  int bucket = in_sample / hist_ns_per_bucket;
  if (bucket >= hist_num_buckets) {
    hist_overflows++;
  }
  else {
    hist_buckets[bucket]++;
  }
}  /* hist_input */

void hist_print()
{
  int i;
  for (i = 0; i < hist_num_buckets; i++) {
    printf("%d\n", hist_buckets[i]);
  }
  printf("o_histogram=%s, hist_overflows=%d, hist_min_sample=%d, hist_max_sample=%d,\n",
      o_histogram, hist_overflows, hist_min_sample, hist_max_sample);
  uint64_t average_sample = hist_sample_sum / (uint64_t)hist_num_samples;
  printf("hist_num_samples=%d, average_sample=%d,\n",
      hist_num_samples, (int)average_sample);
}  /* hist_print */


/* Process source event. */
int handle_src_event(int event, void *extra_data, void *client_data)
{
  switch (event) {
    case LBM_SRC_EVENT_CONNECT:
      break;
    case LBM_SRC_EVENT_DISCONNECT:
      break;
    case LBM_SRC_EVENT_WAKEUP:
      break;
    case LBM_SRC_EVENT_UME_REGISTRATION_ERROR:
      break;
    case LBM_SRC_EVENT_UME_STORE_UNRESPONSIVE:
      break;
    case LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS_EX:
      break;
    case LBM_SRC_EVENT_UME_REGISTRATION_COMPLETE_EX:
      registration_complete++;
      break;
    case LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX:
      __sync_fetch_and_sub(&cur_flight_size, 1);
      ASSRT(cur_flight_size >= 0);  /* Die if negative. */
      break;
    case LBM_SRC_EVENT_SEQUENCE_NUMBER_INFO:
      break;
    case LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION:
      break;
    case LBM_SRC_EVENT_UME_MESSAGE_RECLAIMED_EX:
      break;
    case LBM_SRC_EVENT_UME_DEREGISTRATION_SUCCESS_EX:
      break;
    case LBM_SRC_EVENT_UME_DEREGISTRATION_COMPLETE_EX:
      break;
    case LBM_SRC_EVENT_UME_MESSAGE_NOT_STABLE:
      break;
    default:
      fprintf(stderr, "handle_src_event: unexpected event %d\n", event);
  }

  return 0;
}  /* handle_src_event */

/* Callback for UM smart source events. */
int ssrc_event_cb(lbm_ssrc_t *ssrc, int event, void *extra_data, void *client_data)
{
  handle_src_event(event, extra_data, client_data);

  return 0;
}  /* ssrc_event_cb */

/* Callback for UM source events. */
int src_event_cb(lbm_src_t *src, int event, void *extra_data, void *client_data)
{
  handle_src_event(event, extra_data, client_data);

  return 0;
}  /* src_event_cb */


/* UM callback for force reclaiom events. */
int force_reclaim_cb(const char *topic_str, lbm_uint_t seqnum, void *clientd)
{
  fprintf(stderr, "force_reclaim_cb: topic_str='%s', seqnum=%d, cur_flight_size=%d, max_flight_size=%d,\n",
      topic_str, seqnum, cur_flight_size, max_flight_size);

  cur_flight_size --;  /* Adjust flight size for reclaim. */

  return 0;
}  /* force_reclaim_cb */


/* The publisher can load balance messages across up to 16 sources. */
int num_srcs = 0;
int global_cur_src = 0;
#define MAX_SRCS 16
lbm_src_t *srcs[MAX_SRCS];  /* Used if o_generic_src is 1. */
lbm_ssrc_t *ssrcs[MAX_SRCS];  /* Used if o_generic_src is 0. */
char *ssrc_buffs[MAX_SRCS];

void create_sources(lbm_context_t *ctx)
{
  lbm_src_topic_attr_t *src_attr;
  lbm_topic_t *topic_obj;

  /* Set some options in code. */
  E(lbm_src_topic_attr_create(&src_attr));

  E(lbm_src_topic_attr_str_setopt(src_attr, "ume_session_id", "0x6"));

  /* Get notified for forced reclaims (should not happen). */
  lbm_ume_src_force_reclaim_func_t force_reclaim_cb_conf;
  force_reclaim_cb_conf.func = force_reclaim_cb;
  force_reclaim_cb_conf.clientd = NULL;
  E(lbm_src_topic_attr_setopt(src_attr, "ume_force_reclaim_function",
      &force_reclaim_cb_conf, sizeof(force_reclaim_cb_conf)));

  /* Parse out the individual topics in o_topics and create source objects. */
  char *strtok_context;
  char *work_str = CPRT_STRDUP(o_topics);
  char *cur_topic = CPRT_STRTOK(work_str, ",", &strtok_context);
  while (cur_topic != NULL) {
    ASSRT(strlen(cur_topic) > 0);
    ASSRT(num_srcs < MAX_SRCS);
    E(lbm_src_topic_alloc(&topic_obj, ctx, cur_topic, src_attr));
    if (o_generic_src) {
      E(lbm_src_create(&srcs[num_srcs], ctx, topic_obj,
          src_event_cb, NULL, NULL));
      perf_msg = (perf_msg_t *)msg_buf;  /* Set up perf_msg once. */
    }
    else {  /* Smart Src API. */
      E(lbm_ssrc_create(&ssrcs[num_srcs], ctx, topic_obj,
          ssrc_event_cb, NULL, NULL));
      E(lbm_ssrc_buff_get(ssrcs[num_srcs], &ssrc_buffs[num_srcs], 0));
      /* Set up perf_msg before each send. */
    }

    num_srcs++;
    cur_topic = CPRT_STRTOK(NULL, ",", &strtok_context);
  }

  free(work_str);
  E(lbm_src_topic_attr_delete(src_attr));
}  /* create_sources */


void delete_sources()
{
  int i;

  for (i = 0; i < num_srcs; i++) {
    if (o_generic_src) {  /* If using smart src API */
      E(lbm_src_delete(srcs[i]));
    }
    else {
      E(lbm_ssrc_buff_put(ssrcs[i], ssrc_buffs[i]));
      E(lbm_ssrc_delete(ssrcs[i]));
    }
  }
}  /* delete_sources */


int send_loop(int num_sends, uint64_t sends_per_sec)
{
  struct timespec cur_ts;
  struct timespec start_ts;
  uint64_t num_sent;
  int lbm_send_flags, max_tight_sends;
  static lbm_ssrc_send_ex_info_t ssrc_exinfo;
  int local_cur_src;

  /* Set up local variable so that test is fast. */
  int do_histogram = 0;
  if (hist_buckets != NULL) {
      do_histogram = 1;
  }

  if (o_generic_src) {
    lbm_send_flags = LBM_SRC_NONBLOCK;
  }
  else {  /* Smart Src API. */
    memset(&ssrc_exinfo, 0, sizeof(ssrc_exinfo));
    lbm_send_flags = 0;
  }

  max_tight_sends = 0;

  local_cur_src = global_cur_src;
  int last_src = num_srcs - 1;

  /* Send messages evenly-spaced using busy looping. Based on algorithm:
   * http://www.geeky-boy.com/catchup/html/ */
  CPRT_GETTIME(&start_ts);
  cur_ts = start_ts;
  num_sent = 0;
  do {  /* while num_sent < num_sends */
    uint64_t ns_so_far;
    CPRT_DIFF_TS(ns_so_far, cur_ts, start_ts);
    /* The +1 is because we want to send, then pause. */
    uint64_t should_have_sent = (ns_so_far * sends_per_sec)/1000000000 + 1;
    if (should_have_sent > num_sends) {
      should_have_sent = num_sends;
    }
    if (should_have_sent - num_sent > max_tight_sends) {
      max_tight_sends = should_have_sent - num_sent;
    }

    /* If we are behind where we should be, get caught up. */
    while (num_sent < should_have_sent) {
      if (o_generic_src) {
        /* Construct message. */
        perf_msg->msg_num = num_sent;
        perf_msg->flags = 0;

        struct timespec send_start_ts;
        if (do_histogram) {
          CPRT_GETTIME(&send_start_ts);
        }

        /* Send message. */
        int e = lbm_src_send(srcs[local_cur_src], (void *)perf_msg, o_msg_len, lbm_send_flags);
        if (e == -1) {
          printf("num_sent=%"PRIu64", global_max_tight_sends=%d, max_flight_size=%d\n",
              num_sent, global_max_tight_sends, max_flight_size);
        }
        E(e);  /* If error, print message and fail. */

        if (do_histogram) {
          struct timespec send_return_ts;
          CPRT_GETTIME(&send_return_ts);
          uint64_t ns_send;
          CPRT_DIFF_TS(ns_send, send_return_ts, send_start_ts);
          hist_input((int)ns_send);
        }
      }
      else {  /* Smart Src API. */
        perf_msg = (perf_msg_t *)ssrc_buffs[local_cur_src];
        /* Construct message in shared memory buffer. */
        perf_msg->msg_num = num_sent;
        perf_msg->flags = 0;

        struct timespec send_start_ts;
        if (do_histogram) {
          CPRT_GETTIME(&send_start_ts);
        }

        /* Send message and get next buffer from shared memory. */
        int e = lbm_ssrc_send_ex(ssrcs[local_cur_src], (char *)perf_msg, o_msg_len, lbm_send_flags, &ssrc_exinfo);
        if (e == -1) {
          printf("num_sent=%"PRIu64", global_max_tight_sends=%d, max_flight_size=%d\n",
              num_sent, global_max_tight_sends, max_flight_size);
        }
        E(e);  /* If error, print message and fail. */

        if (do_histogram) {
          struct timespec send_return_ts;
          CPRT_GETTIME(&send_return_ts);
          uint64_t ns_send;
          CPRT_DIFF_TS(ns_send, send_return_ts, send_start_ts);
          hist_input((int)ns_send);
        }
      }

      int cur = __sync_fetch_and_add(&cur_flight_size, 1);
      if (cur > max_flight_size) {
        max_flight_size = cur;
      }

      if (local_cur_src == last_src) {
        local_cur_src = 0;
      }
      else {
        local_cur_src++;
      }
      num_sent++;
    }  /* while num_sent < should_have_sent */
    CPRT_GETTIME(&cur_ts);
  } while (num_sent < num_sends);

  global_cur_src = local_cur_src;

  global_max_tight_sends = max_tight_sends;

  return num_sent;
}  /* send_loop */


int main(int argc, char **argv)
{
  uint64_t cpuset;
  lbm_context_t *ctx;
  struct timespec start_ts;  /* struct timespec is used by clock_gettime(). */
  struct timespec end_ts;
  uint64_t duration_ns;
  int actual_sends;
  double result_rate;
  CPRT_NET_START;

  CPRT_INITTIME();

  get_my_opts(argc, argv);

  if (hist_num_buckets > 0) {
    hist_create();
  }

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_cpu=%d, o_config=%s, o_generic_src=%d, o_histogram=%s, o_linger_ms=%d, o_loss_percent=%d, o_msg_len=%d, o_num_msgs=%d, o_persist='%s', o_rate=%d, o_topics='%s', o_warmup=%s, xml_config=%s, \n",
      o_affinity_cpu, o_config, o_generic_src, o_histogram, o_linger_ms, o_loss_percent, o_msg_len, o_num_msgs, o_persist, o_rate, o_topics, o_warmup, o_xml_config);

  msg_buf = (char *)malloc(o_msg_len);

  /* Context thread inherits the initial CPU set of the process. */
  E(lbm_context_create(&ctx, NULL, NULL, NULL));

  /* Pin time-critical thread (sending thread) to requested CPU core. */
  if (o_affinity_cpu > -1) {
    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(o_affinity_cpu, &cpuset);
    cprt_set_affinity(cpuset);
  }

  create_sources(ctx);

  if (strlen(o_persist) > 0) {
    /* Wait for registration complete. */
    while (registration_complete < num_srcs) {
      sleep(1);
      if (registration_complete < num_srcs) {
        printf("Waiting for %d store registrations.\n",
            num_srcs - registration_complete);
      }
    }

    /* Wait for receiver(s) to register and are ready to receive messages.
     * There is a proper algorithm for this, but it adds unnecessary complexity
     * and obscures the perf test algorithms. Sleep for simplicity. */
    sleep(5);
  }
  else {  /* Streaming (not persistence). */
    if (warmup_loops > 0) {
      /* Without persistence, need to initiate data on each src. */
      send_loop(num_srcs, 999999999);
      warmup_loops -= num_srcs;
      if (warmup_loops < 0) { warmup_loops = 0; }
    }
    /* Wait for topic resolution. */
    sleep(1);
  }

  if (warmup_loops > 0) {
    /* Warmup loops to get CPU caches loaded. */
    send_loop(warmup_loops, warmup_rate);
  }

  if (o_loss_percent > 0) {
    lbm_set_lbtrm_src_loss_rate(o_loss_percent);
  }

  /* Measure overall send rate by timing the main send loop. */
  if (hist_buckets != NULL) {
    hist_init();  /* Zero out data from warmup period. */
  }
  CPRT_GETTIME(&start_ts);
  actual_sends = send_loop(o_num_msgs, o_rate);
  CPRT_GETTIME(&end_ts);
  CPRT_DIFF_TS(duration_ns, end_ts, start_ts);

  result_rate = (double)(duration_ns);
  result_rate /= (double)1000000000;
  /* Don't count initial message. */
  result_rate = (double)(actual_sends - 1) / result_rate;

  /* This is for internal UM debugging. */
#if defined(PRINT4)
  histo_print4();
#endif

  if (hist_buckets != NULL) {
    hist_print();
  }

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("actual_sends=%d, duration_ns=%"PRIu64", result_rate=%f, global_max_tight_sends=%d, max_flight_size=%d\n",
      actual_sends, duration_ns, result_rate, global_max_tight_sends,
      max_flight_size);

  if (strlen(o_persist) > 0) {
    /* Wait for Store to get caught up. */
    int num_checks = 0;
    while (cur_flight_size > 0) {
      num_checks++;
      if (num_checks > 3) {
        printf("Giving up.\n");
        break;
      }
      printf("Waiting for flight size %d to clear.\n", cur_flight_size);
      sleep(num_checks);  /* Sleep longer each check. */
    }
  }

  if (o_linger_ms > 0) {
    usleep(o_linger_ms * 1000);
  }

  delete_sources();

  E(lbm_context_delete(ctx));

  free(msg_buf);

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
