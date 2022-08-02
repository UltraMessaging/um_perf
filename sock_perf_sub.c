/* sock_perf_sub.c - performance measurement tool. */
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

/* The sock_perf_sub program was initially cloned from sock_perf_pub and
 * therefore uses many of the same portibility constructs defined in cprt.
 * However, due to the differences between the Linux and Windows socket
 * APIs, no attempt is made to make sock_perf_pub portible to Windows.
 */
#include "cprt.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#if ! defined(_WIN32)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <stdlib.h>
  #include <unistd.h>
#endif

#include "um_perf.h"


/* Command-line options and their defaults. String defaults are set
 * in "get_my_opts()".
 */
static int o_affinity_cpu = -1;
static char *o_group = NULL;
static char *o_histogram = NULL;  /* -H */
static char *o_interface = NULL;

/* Parameters parsed out from command-line options. */
int hist_num_buckets;
int hist_ns_per_bucket;
struct in_addr iface_in;
struct in_addr group_in;

#define MAXEVENTS 8
/* Globals. The code depends on the loader initializing them to all zeros. */


char usage_str[] = "Usage: sock_perf_sub [-h] [-a affinity_cpu] [-g group] [-H hist_num_buckets,hist_ns_per_bucket] [-i interface]";

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
      "  -g group : multicast group address [%s]\n"
      "  -H hist_num_buckets,hist_ns_per_bucket : send time histogram [%s]\n"
      "  -i interface : interface for multicast bind [%s]\n"
      , o_affinity_cpu, o_group, o_histogram, o_interface
  );
  CPRT_NET_CLEANUP;
  exit(0);
}


/* Process command-line options. */
void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  /* Set defaults for string options. */
  o_group = CPRT_STRDUP("");
  o_histogram = CPRT_STRDUP("0,0");
  o_interface = CPRT_STRDUP("");

  while ((opt = cprt_getopt(argc, argv, "ha:g:H:i:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': CPRT_ATOI(cprt_optarg, o_affinity_cpu); break;
      case 'g': free(o_group); o_group = CPRT_STRDUP(cprt_optarg); break;
      case 'H': free(o_histogram); o_histogram = CPRT_STRDUP(cprt_optarg); break;
      case 'i': free(o_interface); o_interface = CPRT_STRDUP(cprt_optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  /* Parse the group option. */
  ASSRT(strlen(o_group) > 0);
  memset((char *)&group_in, 0, sizeof(group_in));
  ASSRT(inet_aton(o_group, &group_in) != 0);

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

  /* Parse the interface option. */
  ASSRT(strlen(o_interface) > 0);
  memset((char *)&iface_in, 0, sizeof(iface_in));
  ASSRT(inet_aton(o_interface, &iface_in) != 0);

  if (cprt_optind != argc) { usage("Unexpected positional parameter(s)"); }
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


void init_sock(int sock)
{
  int opt_enable = 1;
  int flags;
  struct sockaddr_in dest_sin;
  struct ip_mreq add_member;
  uint32_t rcvbuf32 = 128000000;

  CPRT_EOK0(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
      (char*)&opt_enable, sizeof(opt_enable)));

  /* Bind to port 12000. */
  memset(&dest_sin, 0, sizeof(dest_sin));
  dest_sin.sin_family = AF_INET;
  dest_sin.sin_addr.s_addr = group_in.s_addr;
  dest_sin.sin_port = htons(12000);
  CPRT_EM1(bind(sock, &dest_sin, sizeof(dest_sin)));

  /* Join multicast group. */
  memset(&add_member, 0, sizeof(add_member));
  add_member.imr_multiaddr.s_addr = group_in.s_addr;
  add_member.imr_interface.s_addr = iface_in.s_addr;
  CPRT_EM1(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
      (char*)&add_member, sizeof(add_member)));

  /* Set non-blocking. */
  CPRT_EM1(flags = fcntl(sock, F_GETFL, 0));
  flags |= O_NONBLOCK | O_NDELAY;
  CPRT_EM1(fcntl(sock, F_SETFL, flags));

  /* 128 MB socket buffer */
  CPRT_EM1(setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
      (char*)&rcvbuf32, sizeof(rcvbuf32)));
}  /* init_sock */


int main(int argc, char **argv)
{
  int sock;
  int epoll_fd;
  uint64_t cpuset;
  struct epoll_event event;
  struct epoll_event rtn_events[MAXEVENTS];
  struct sockaddr from;
  socklen_t addrlen;
#ifdef PRTOUT
  int s;
#endif

  CPRT_NET_START;

  CPRT_INITTIME();

  get_my_opts(argc, argv);

  if (hist_num_buckets > 0) {
    hist_create();
    hist_init();  /* Zero out data from warmup period. */
  }

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_cpu=%d, o_group=%s, o_histogram=%s, o_interface=%s, \n",
         o_affinity_cpu, o_group, o_histogram, o_interface);

  /* Pin time-critical thread (sending thread) to requested CPU core. */
  if (o_affinity_cpu > -1) {
    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(o_affinity_cpu, &cpuset);
    cprt_set_affinity(cpuset);
  }

  /* Create the socket for receiving. */
  sock = socket(PF_INET,SOCK_DGRAM,0);
  ASSRT(sock != -1);
  init_sock(sock);

  /* Create the epoll FD. */
  CPRT_EM1(epoll_fd = epoll_create(1024));
  event.events = EPOLLIN;
  event.data.fd = sock;
  CPRT_EM1(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event));

  while (1) {
    int n, i;
    ssize_t count;
    char buf[8192];

    while (1) {
      n = epoll_wait(epoll_fd, rtn_events, MAXEVENTS, -1);
      if (n > 0) {
        break;
      }
    }

    for (i = 0; i < n; i++) {
      if ((rtn_events[i].events & EPOLLERR) ||
          (rtn_events[i].events & EPOLLHUP) ||
          (!(rtn_events[i].events & EPOLLIN))) {
        /* An error has occured on this fd, or the socket is not
           ready for reading (why were we notified then?) */
        fprintf (stderr, "epoll error\n");
        close (rtn_events[i].data.fd);
        continue;
      }

      addrlen = sizeof(from);

      count = recvfrom(rtn_events[i].data.fd, buf, sizeof buf, 0, &from, &addrlen);
      if (count == -1) {
        if (errno != EAGAIN) {
          perror ("read");
        }
      }

      if (count == 0) {
        close (rtn_events[i].data.fd);
        continue;
      }

#ifdef PRTOUT
      /* Write the buffer to standard output */
      s = write (1, buf, count);
      if (s == -1) {
        perror ("write");
        abort ();
      }
#endif
    }
  }

  close (epoll_fd);

  /* Time to exit. */
  /* Done, print results. */
  if (hist_buckets != NULL) {
    hist_print();
  }

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
