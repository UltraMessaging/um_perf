/* sock_perf_pub.c - performance measurement tool.
 * This version is intended to more-closely simulate UM by creating a
 * "context" thread that rests on an epoll_wait() for a socket and a pipe.
 */
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

/* The sock_perf_pub program was initially cloned from um_perf_pub and
 * therefore uses many of the same portibility constructs defined in cprt.
 * However, due to the differences between the Linux and Windows socket
 * APIs, no attempt is made to make sock_perf_pub portible to Windows.
 */
#include "cprt.h"

#include <stdio.h>
#include <string.h>
#if ! defined(_WIN32)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/epoll.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <stdlib.h>
  #include <unistd.h>
#endif

#include "um_perf.h"


/* Command-line options and their defaults. String defaults are set
 * in "get_my_opts()".
 */
static int o_affinity_cpu = -1;
static char *o_group_rcv = NULL;
static char *o_group_src = NULL;  /* -G */
static char *o_histogram = NULL;  /* -H */
static char *o_interface = NULL;
static int o_msg_len = 0;
static int o_num_msgs = 0;
static int o_rate = 0;
static int o_sleep_usec = 0;
static char *o_warmup = NULL;

/* Parameters parsed out from command-line options. */
int hist_num_buckets;
int hist_ns_per_bucket;
struct in_addr iface_in;
struct in_addr group_rcv_in;
struct in_addr group_src_in;
int warmup_loops;
int warmup_rate;

/* Globals. The code depends on the loader initializing them to all zeros. */
perf_msg_t *perf_msg;
int global_max_tight_sends;
int exit_context;


char usage_str[] = "Usage: sock_perf_pub [-h] [-a affinity_cpu] [-G group_rcv] [-g group_src] [-H hist_num_buckets,hist_ns_per_bucket] [-i interface] [-m msg_len] [-n num_msgs] [-r rate] [-s sleep_usec] [-w warmup_loops,warmup_rate]";

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
      "  -G group_rcv : multicast group address to receive [%s]\n"
      "  -g group_src : multicast group address to send [%s]\n"
      "  -H hist_num_buckets,hist_ns_per_bucket : send time histogram [%s]\n"
      "  -i interface : interface for multicast bind [%s]\n"
      "  -m msg_len : message length [%d]\n"
      "  -n num_msgs : number of messages to send [%d]\n"
      "  -r rate : messages per second to send [%d]\n"
      "  -s sleep_usec : microseconds to sleep between sends [%d]]\n"
      "  -w warmup_loops,warmup_rate : messages to send before measurement [%s]\n"
      , o_affinity_cpu, o_group_rcv, o_group_src, o_histogram, o_interface, o_msg_len
      , o_num_msgs, o_rate, o_sleep_usec, o_warmup
  );
  CPRT_NET_CLEANUP;
  exit(0);
}


/* Process command-line options. */
void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  /* Set defaults for string options. */
  o_group_rcv = CPRT_STRDUP("");
  o_group_src = CPRT_STRDUP("");
  o_histogram = CPRT_STRDUP("0,0");
  o_interface = CPRT_STRDUP("");
  o_warmup = CPRT_STRDUP("0,0");

  while ((opt = getopt(argc, argv, "ha:G:g:H:i:m:n:r:s:w:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': CPRT_ATOI(optarg, o_affinity_cpu); break;
      case 'G': free(o_group_rcv); o_group_rcv = CPRT_STRDUP(optarg); break;
      case 'g': free(o_group_src); o_group_src = CPRT_STRDUP(optarg); break;
      case 'H': free(o_histogram); o_histogram = CPRT_STRDUP(optarg); break;
      case 'i': free(o_interface); o_interface = CPRT_STRDUP(optarg); break;
      case 'm': CPRT_ATOI(optarg, o_msg_len); break;
      case 'n': CPRT_ATOI(optarg, o_num_msgs); break;
      case 'r': CPRT_ATOI(optarg, o_rate); break;
      case 's': CPRT_ATOI(optarg, o_sleep_usec); break;
      case 'w': free(o_warmup); o_warmup = CPRT_STRDUP(optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  /* Must supply one of -r or -s, but not both. */
  ASSRT((o_rate > 0 && o_sleep_usec == 0) || (o_rate == 0 && o_sleep_usec > 0));

  /* Must supply certain required "options". */
  ASSRT(o_num_msgs > 0);
  ASSRT(o_msg_len > 0);

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

  /* Parse the group options: -G and -g. */
  if (strlen(o_group_rcv) > 0) {
    memset((char *)&group_rcv_in, 0, sizeof(group_rcv_in));
    ASSRT(inet_aton(o_group_rcv, &group_rcv_in) != 0);  /* -G */
  }

  ASSRT(strlen(o_group_src) > 0);
  memset((char *)&group_src_in, 0, sizeof(group_src_in));
  ASSRT(inet_aton(o_group_src, &group_src_in) != 0);  /* -g */

  /* Parse the interface option. */
  ASSRT(strlen(o_interface) > 0);
  memset((char *)&iface_in, 0, sizeof(iface_in));
  ASSRT(inet_aton(o_interface, &iface_in) != 0);

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


void init_src_sock(int src_sock)
{
  CPRT_EOK0(setsockopt(src_sock, IPPROTO_IP, IP_MULTICAST_IF,
      (char*)&iface_in, sizeof(iface_in)));               /* From -i option. */

  char ttl = 15;
  CPRT_EOK0(setsockopt(src_sock, IPPROTO_IP, IP_MULTICAST_TTL,
      (char *)&ttl, sizeof(ttl)));
}  /* init_src_sock */


void init_rcv_sock(int rcv_sock)
{
  int opt_enable = 1;
  CPRT_EOK0(setsockopt(rcv_sock, SOL_SOCKET, SO_REUSEADDR,
      (char*)&opt_enable, sizeof(opt_enable)));

  /* Bind to port 12000. */
  struct sockaddr_in dest_sin;
  memset(&dest_sin, 0, sizeof(dest_sin));
  dest_sin.sin_family = AF_INET;
  dest_sin.sin_addr.s_addr = group_rcv_in.s_addr;  /* From -G option. */
  dest_sin.sin_port = htons(12000);
  CPRT_EOK0(bind(rcv_sock, (struct sockaddr *)&dest_sin, sizeof(dest_sin)));

  /* Join mulicast group. */
  struct ip_mreq add_member;
  memset(&add_member, 0, sizeof(add_member));
  add_member.imr_multiaddr.s_addr = group_rcv_in.s_addr;  /* From -G option. */
  add_member.imr_interface.s_addr = iface_in.s_addr;      /* From -i option. */
  CPRT_EOK0(setsockopt(rcv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
      (char*)&add_member, sizeof(add_member)));

  /* Set non-blocking. */
  int flags;
  CPRT_EM1(flags = fcntl(rcv_sock, F_GETFL, 0));
  flags |= O_NONBLOCK | O_NDELAY;
  CPRT_EM1(fcntl(rcv_sock, F_SETFL, flags));

  /* 128 MB socket buffer. */
  uint32_t rcvbuf32 = 128000000;
  CPRT_EOK0(setsockopt(rcv_sock, SOL_SOCKET, SO_RCVBUF,
      (char*)&rcvbuf32, sizeof(rcvbuf32)));
}  /* init_rcv_sock */


CPRT_THREAD_ENTRYPOINT context_thread(void *in_arg)
{
  /* Create read socket (like the TR socket). */
  int rcv_sock;
  CPRT_EM1(rcv_sock = socket(PF_INET,SOCK_DGRAM,0));
  init_rcv_sock(rcv_sock);

  int pipe_fds[2];  /* pipe[0] is read side, pipe[1] is write side. */
  CPRT_EOK0(pipe(pipe_fds));
  /* Set pipe non-blocking. */
  int flags;
  CPRT_EM1(flags = fcntl(pipe_fds[0], F_GETFL, 0));
  flags |= O_NONBLOCK | O_NDELAY;
  CPRT_EM1(fcntl(pipe_fds[0], F_SETFL, flags));

  int epoll_fd;
  CPRT_EM1(epoll_fd = epoll_create(1024));
  struct epoll_event event;

  event.events = EPOLLIN;
  event.data.fd = rcv_sock;
  CPRT_EOK0(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, rcv_sock, &event));

  event.events = EPOLLIN;
  event.data.fd = pipe_fds[0];
  CPRT_EOK0(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_fds[0], &event));

  while (! exit_context) {
#define MAX_FDS 8
    struct epoll_event rtn_events[MAX_FDS];
    int epoll_rtn;
    CPRT_EOK0(epoll_rtn = epoll_wait(epoll_fd, rtn_events, MAX_FDS, 500));
  }

  CPRT_EOK0(close(rcv_sock));
  CPRT_EOK0(close(epoll_fd));

  CPRT_THREAD_EXIT;
  return 0;
}  /* context_thread */


int send_loop(int src_sock, int num_sends, uint64_t sends_per_sec)
{
  struct timespec cur_ts;
  struct timespec start_ts;
  uint64_t num_sent;
  int max_tight_sends;
  struct sockaddr_in dest_sin;
  struct msghdr message_hdr;
  struct iovec message_iov;

  /* Set up destination group:port. */
  memset(&dest_sin, 0, sizeof(dest_sin));
  dest_sin.sin_family = AF_INET;
  dest_sin.sin_addr.s_addr = group_src_in.s_addr;  /* From -g option. */
  dest_sin.sin_port = htons(12001);

  /* Set up outgoing message buffer. */
  message_iov.iov_base = perf_msg;
  message_iov.iov_len = o_msg_len;

  /* Set up call to sendmsg(). */
  message_hdr.msg_name = &dest_sin;
  message_hdr.msg_namelen = sizeof(dest_sin);
  message_hdr.msg_iov = &message_iov;
  message_hdr.msg_iovlen = 1;
  message_hdr.msg_control = NULL;
  message_hdr.msg_controllen = 0;
  message_hdr.msg_flags = 0;

  /* Set up local variable so that test is fast. */
  int do_histogram = 0;
  if (hist_buckets != NULL) {
      do_histogram = 1;
  }

  max_tight_sends = 0;

  if (o_rate > 0) {
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
        /* Construct message. */
        perf_msg->msg_num = num_sent;
        perf_msg->flags = 0;

        struct timespec send_start_ts;
        if (do_histogram) {
          CPRT_GETTIME(&send_start_ts);
        }

        /* Send message. */
        CPRT_EM1(sendmsg(src_sock, &message_hdr, 0));

        if (do_histogram) {
          struct timespec send_return_ts;
          CPRT_GETTIME(&send_return_ts);
          uint64_t ns_send;
          CPRT_DIFF_TS(ns_send, send_return_ts, send_start_ts);
          hist_input((int)ns_send);
        }
        num_sent++;
      }  /* while num_sent < should_have_sent */
      CPRT_GETTIME(&cur_ts);
    } while (num_sent < num_sends);

    global_max_tight_sends = max_tight_sends;
  }  /* if o_rate > 0 */

  if (o_sleep_usec > 0) {
    /* Send with usleep between each send. */
    for (num_sent = 0; num_sent < num_sends; num_sent++) {
      /* Construct message. */
      perf_msg->msg_num = num_sent;
      perf_msg->flags = 0;

      struct timespec send_start_ts;
      if (do_histogram) {
        CPRT_GETTIME(&send_start_ts);
      }

      /* Send message. */
      CPRT_EM1(sendmsg(src_sock, &message_hdr, 0));

      if (do_histogram) {
        struct timespec send_return_ts;
        CPRT_GETTIME(&send_return_ts);
        uint64_t ns_send;
        CPRT_DIFF_TS(ns_send, send_return_ts, send_start_ts);
        hist_input((int)ns_send);
      }

      usleep(o_sleep_usec);
    }
  }  /* if o_sleep > 0 */

  return num_sent;
}  /* send_loop */


int main(int argc, char **argv)
{
  uint64_t cpuset;
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
  printf("o_affinity_cpu=%d, o_group_rcv=%s, o_group_src=%s, o_histogram=%s, o_interface=%s, o_msg_len=%d, o_num_msgs=%d, o_rate=%d, o_sleep_usec=%d, o_warmup=%s, \n",
      o_affinity_cpu, o_group_rcv, o_group_src, o_histogram, o_interface, o_msg_len, o_num_msgs, o_rate, o_sleep_usec, o_warmup);

  perf_msg = (perf_msg_t *)malloc(o_msg_len);
  CPRT_SNPRINTF((char *)perf_msg, o_msg_len - 1, "sock_perf_pub,sock_perf_pub,sock_perf_pub,sock_perf_pub,");

  int src_sock;
  CPRT_EM1(src_sock = socket(PF_INET,SOCK_DGRAM,0));
  ASSRT(src_sock != -1);

  init_src_sock(src_sock);

  CPRT_THREAD_T ctx_thread_id;
  if (strlen(o_group_rcv) > 0) {
    exit_context = 0;
    CPRT_THREAD_CREATE(ctx_thread_id, context_thread, NULL);
  }

  /* Pin time-critical thread (sending thread) to requested CPU core. */
  if (o_affinity_cpu > -1) {
    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(o_affinity_cpu, &cpuset);
    cprt_set_affinity(cpuset);
  }

  if (warmup_loops > 1) {
    /* Warmup loops to get CPU caches loaded. */
    send_loop(src_sock, warmup_loops, warmup_rate);
  }

  /* Measure overall send rate by timing the main send loop. */
  if (hist_buckets != NULL) {
    hist_init();  /* Zero out data from warmup period. */
  }
  CPRT_GETTIME(&start_ts);
  actual_sends = send_loop(src_sock, o_num_msgs, o_rate);
  CPRT_GETTIME(&end_ts);
  CPRT_DIFF_TS(duration_ns, end_ts, start_ts);

  result_rate = (double)(duration_ns);
  result_rate /= (double)1000000000;
  /* Don't count initial message. */
  result_rate = (double)(actual_sends - 1) / result_rate;

  if (hist_buckets != NULL) {
    hist_print();
  }

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("actual_sends=%d, duration_ns=%"PRIu64", result_rate=%f, global_max_tight_sends=%d\n",
      actual_sends, duration_ns, result_rate, global_max_tight_sends);

  if (strlen(o_group_rcv) > 0) {
    exit_context = 1;
    CPRT_THREAD_JOIN(ctx_thread_id);
  }

  free(perf_msg);

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
