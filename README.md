# um_perf - test programs to measure the performance of Ultra Messaging.

Tools for measuring the performance of Ultra Messaging (UM) persistence
and streaming.

- [um_perf - test programs to measure the performance of Ultra Messaging.](#um_perf---test-programs-to-measure-the-performance-of-ultra-messaging)
  - [COPYRIGHT AND LICENSE](#copyright-and-license)
  - [REPOSITORY](#repository)
  - [TESTS](#tests)
  - [RESULTS](#results)
    - [Reproduction](#reproduction)
    - [Requirements](#requirements)
    - [Choose CPUs](#choose-cpus)
    - [Build Test Tools](#build-test-tools)
    - [Update Configuration File](#update-configuration-file)
      - [UM Configuration File](#um-configuration-file)
      - [Store Configuration Files](#store-configuration-files)
    - [Measure System Interruptions](#measure-system-interruptions)
    - [Measure Maximum Sustainable Message Rates](#measure-maximum-sustainable-message-rates)
      - [Test 1: Streaming](#test-1-streaming)
      - [Test 2: Single SPP Store](#test-2-single-spp-store)
      - [Test 3: Single RPP Store](#test-3-single-rpp-store)
      - [Test 4: Quorum/Consensus](#test-4-quorumconsensus)
      - [Test 5: Load Balance](#test-5-load-balance)
      - [Test 6: Three-Source Streaming](#test-6-three-source-streaming)
      - [Test 7: Single SPP Store, Application Batching](#test-7-single-spp-store-application-batching)
      - [Test 8: Single RPP Store, Application Batching](#test-8-single-rpp-store-application-batching)
      - [Test 9: Load Balance, Application Batching](#test-9-load-balance-application-batching)
  - [WARMUP](#warmup)
  - [TOOL USAGE NOTES](#tool-usage-notes)
    - [um_perf_pub](#um_perf_pub)
      - [Linger Time](#linger-time)
      - [Warmup](#warmup)
      - [Histogram](#histogram)
    - [um_perf_sub](#um_perf_sub)
      - [Persist Mode](#persist-mode)
      - [Spin Count](#spin-count)
    - [Affinity](#affinity)
  - [MEASUREMENT OUTLIERS](#measurement-outliers)
    - [Interruptions](#interruptions)
  - [CODE NOTES](#code-notes)
    - [Error Handling](#error-handling)
    - [SAFE_ATOI](#safe_atoi)
    - [DIFF_TS](#diff_ts)
    - [send_loop()](#send_loop)
  - [Notes on Going Fast](#notes-on-going-fast)
    - [Core Count and Network Interfaces](#core-count-and-network-interfaces)
    - [Burst VS. Sustain?](#burst-vs-sustain)
      - [Loss Recovery at 1.64M Msgs/Sec](#loss-recovery-at-164m-msgssec)
    - [Exceeding Line Rate?](#exceeding-line-rate)
    - [Busy Looping](#busy-looping)
    - [Application Optimizations](#application-optimizations)
    - [Server Optimizations](#server-optimizations)
  - [INFORMATICA TEST HARDWARE](#informatica-test-hardware)
    - [Host 1](#host-1)
    - [Host 2](#host-2)
    - [Host S1](#host-s1)
    - [Host S2](#host-s2)
    - [Host S3](#host-s3)

<sup>(table of contents from https://luciopaiva.com/markdown-toc/)</sup>

## COPYRIGHT AND LICENSE

All of the documentation and software included in this and any
other Informatica Ultra Messaging GitHub repository
Copyright (C) Informatica. All rights reserved.

Permission is granted to licensees to use
or alter this software for any purpose, including commercial applications,
according to the terms laid out in the Software License Agreement.

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

## REPOSITORY

See https://github.com/UltraMessaging/um_perf for code and documentation.

## TESTS

Informatica used the tools in this repository to measure the
maximum-sustainable message rate for streaming and persistent sources.
The primary motivation for these tools is to measure persistence.

The Informatica Ultra Messaging computer lab has some fast hosts,
but not enough to run a representative test of persistence.
That requires five fast hosts in total, three with fast disks.
But the UM lab only has one host with a fast disk.

So we devised a series of tests to estimate the performance of a
fully-provisioned production datacenter using a minimally-provisioned lab.

Here are the tests:
1. Single source, single receiver, streaming (no Store).
Not persistence, but provides a baseline rate.
2. Single source, single SPP-based Store (disk-based), single receiver.
This characterizes a single disk-based store's performance (throughput).
3. Single source, single RPP-based Store, single receiver.
This allows us to compare SPP to RPP (see [RPP Vs. SPP](#rpp-vs-spp)).
For the hardware we used, RPP is a little bit faster than SPP.
4. Single source, three RPP-based Stores in quorum/consensus, single receiver.
This allows us to measure the impact of a 3-Store Q/C group compared to
a single-store Q/C group.
5. Three sources (single sending thread), three RPP-based Stores
(one per source), three receivers (single receiver thread).
This demonstrates balancing the load across multiple Stores
(each Store only sees one-third of the messages).
6. Three sources (single sending thread), streaming (no Store).
This demonstrates that sending to three streaming sources can send almost as
fast as a single streaming source.
7. Single source, batching two messages,
single SPP-based Store (disk-based), single receiver.
This characterizes application batching two messages together per send.
8. Single source, batching two messages,
single RPP-based Store (disk-based), single receiver.
This characterizes application batching two messages together per send
for an RPP Store.
9. Three sources (single sending thread), batching two messages,
three RPP-based Stores (one per source),
three receivers (single receiver thread).
This demonstrates balancing the load of application batched messages across
multiple Stores (each Store only sees one-third of the messages).

The method of discovering the maximum sustainable throughput is
to run a publisher at a given message rate for at least 1 minute
and check for failure indications. If the test succeeds,
repeat with a higher message rate.
When a test fails,
the message rate is decreased, and the test is repeated.
(A test is said to have "failed" if any component has packet loss,
or if the publisher exhausts flight size
(LBM_EWOULDBLOCK send error) due to exceeding the disk write speed.)

Note that the comparisons will be *very* dependent on the underlying
hardware (CPU and disk).
Users are strongly advised to perform the same tests on hardware that
is as close as possible to their anticipated production hardware.

Finally, note that many users choose SSD disks for their high performance.
And while it is true that SSDs eliminate the "seek time" that can severely
limit spinning disk performance,
standard SSDs are typically "optimized for reading",
meaning that their write speeds will often be significantly lower than
cheaper spinning disk write speeds.
Informatica strongly recommends selecting SSDs for persistent Stores that
are "optimized for write".

All tests were performed with 700-byte application messages.
Most tests flushed every message, but the last two included
application batching.
The tests also used
UM "Smart Sources" and Xilinx (formerly Solarflare) 10-gig NICs with
Onload kernel-bypass drivers were used on all hosts.
In the results below, "K" represents 1,000; "M" represents 1,000,000;
"G" represents 1,000,000,000 (i.e. they are not powers of 2).

## RESULTS

Using load-balanced SPP Stores and application batching,
UM persistence can easily sustain 1.5M messages/sec.

Using a single RPP Store and application batching,
UM persistence can sustain 1.4M messages/sec.

Load-balanced means that the sending thread sends messages to three
different topics.
Each topic is handled by its own Store process.
The subscriber receives from all three topics.

Note that inter-topic message ordering is not guaranteed.

Here are the results from the full test suite:

Test | Message Rate | Summary
---- | ------------ | ------------
[1](#test-1-streaming) | 1.4M | Streaming (no Stores)
[2](#test-2-single-spp-store) | 550K | 1 SPP Store (disk-based)
[3](#test-3-single-rpp-store) | 760K | 1 RPP Store
[4](#test-4-quorumconsensus) | 740K | 3 RPP Stores (Q/C)
[5](#test-5-load-balance) | 1M | 3 sources, load balanced to 3 RPP Stores (not Q/C)
[6](#test-6-three-source-streaming) | 1.5M | 3 streaming sources (no Stores)
[7](#test-7-single-spp-store-application-batching) | 800K | application batching, 1 SPP Store (disk-based)
[8](#test-8-single-rpp-store-application-batching) | 1.44M | application batching, 1 RPP Store (disk-based)
[9](#test-9-load-balance-application-batching) | 1.5M | application batching, 3 sources, load balanced to 3 RPP Stores (not Q/C)

### Reproduction

This section contains details of how these results were generated at Informatica.
These results can reliably be reproduced in our test lab.

It is assumed in all of these steps that the "LD_LIBRARY_PATH" environment
variable includes the path to the Ultra Messaging version 6.14
library directory.
For example:
````
LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.

export LD_LIBRARY_PATH=$LBM/lib
````

Furthermore, it is assumed that the "PATH" environment variable includes
the path to the directory containing the um_perf tools'
executables that you build.

Finally, the "taskset" command is used to run the test programs,
setting affinity to a non-time-critical CPU in the same NUMA node as
the time-critical CPUs.
The test programs use the "-a" command-line option to change the affinity
of the time-critical threads to the time-critical CPUs.
See [Affinity](#affinity).

ATTENTION: the "taskset" command expects a bitmap of CPUs,
with 0x01 representing CPU number 0, 0x02 representing CPU 1,
0x04 representing CPU 2, etc.
The um_perf tools' "-a" options expect the actual CPU number.

### Requirements

1. 5 Linux-based hosts (X86, 64-bit).
hyperthreading turned off,
16 gigabytes or more memory.
At least one host must have a fast disk.
These should be "bare metal" machines, NOT virtual machines.
2. C compiler (gcc) and related tools.
3. Ultra Messaging version 6.14, including development files (lbm.h,
libraries, etc.).

See [Test Hardware](#informatica-test-hardware) for details of Informatica's
test hosts.

### Choose CPUs

Different hardware systems assign CPU numbers to NUMA nodes differently.
Our hosts have even/odd assignments, but there are
[other numbering schemes](https://itectec.com/unixlinux/understanding-output-of-lscpu/).

Enter the Linux command "lscpu". For example:
````
$ lscpu
Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              12
On-line CPU(s) list: 0-11
Thread(s) per core:  1
Core(s) per socket:  6
Socket(s):           2
NUMA node(s):        2
...
NUMA node0 CPU(s):   0,2,4,6,8,10
NUMA node1 CPU(s):   1,3,5,7,9,11
...
````

Choose two CPU numbers on the same NUMA node for your time-critical
CPUs.
Choose another CPU in the same NUMA node for your non-time-critical CPU.

However, be aware that different CPUs will show different performances
for network-intensive workloads.
It is best to try different CPUs on a host to determine the fastest
CPU for a given component (publisher, subscriber, Store).

### Build Test Tools

* um_perf_pub - publisher (source).
* um_perf_sub - subscriber (subscriber).

The source code for these tools can be found in the GitHub repository
"um_perf" at: https://github.com/UltraMessaging/um_perf

The files can be obtained by cloning the repository using "git" or
[GitHub Desktop](https://desktop.github.com), or by browsing to
https://github.com/UltraMessaging/um_perf and clicking the green "Code"
button (select "Download ZIP").

To build the tools, the shell script "bld.sh" should be modified.
For example:
````
#!/bin/sh
# bld.sh - build the programs on Linux.

LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.
...
````
The assignment to the shell variable "LBM" should be changed to the location
of your UM installation.

After running the "bld.sh" script,
update the "PATH" environment variable to include the
directory containing these executables.
For example:
````
export PATH="$HOME/um_perf:$PATH"
````

### Update Configuration File

Contact your network administration group and request four multicast groups
that you can use exclusively.
You don't want your testing to interfere with others,
and you don't want others' activities to interfere with your test.

Also, take note of the publisher's 10G interface IP address.
Ours is 10.29.4.121.
It is usually possible to mask off the final 8 bits and use the CIDR form
of the network address.
For example: "10.29.4.0/24".
All hosts can typically use this on the same LAN.

#### UM Configuration File

The file "um.xml" should be modified.
Here is an excerpt:
````
<?xml version="1.0" encoding="UTF-8" ?>
<um-configuration version="1.0">
  <templates>
    <template name="um_perf">
      <options type="context"> 
        <option name="resolver_multicast_interface" default-value="10.29.4.0/24"/>
        <option name="request_tcp_interface" default-value="10.29.4.0/24"/>
        <option name="resolver_multicast_address" default-value="239.101.3.1"/>
...
  <applications>
    <application name="um_perf" template="um_perf">
      <contexts>
        <context>
          <sources>
            <topic topicname="topic1">
              <options type="source">
                <option name="transport_lbtrm_multicast_address" default-value="239.101.3.2"/>
...
````
Search this file for "239" to find all lines that contain multicast groups,
and "10.29" for other site-specific IPs.

We use multicast groups "239.101.3.1" through "239.101.3.4".
Change those to the group provided by your network admins.

WARNING:
The "um.xml" configuration is designed for ease of performing the desired
tests, and is not suitable for production.
It does not contain proper tunings for many other options.
It uses multicast topic resolution for ease of setting up the test,
even though we typically recommend the use of
[TCP-based topic resolution](https://ultramessaging.github.io/currdoc/doc/Design/topicresolutiondescription.html#tcptr).

We recommend conducting a configuration workshop with Informatica.

#### Store Configuration Files

There are five different Store XML configuration files that are used by
different Stores for different tests.

* store_1a.xml - used on host S1, which has a fast disk.
* store_1b.xml - used on host S2.
* store_1c.xml - used on host S3.
* store_2a.xml - used on host S2.
* store_3a.xml - used on host S3.

You should modify the following lines in each file per your environment:
````
...
    <store name="store_a_topic1" interface="10.29.4.0/24" port="12001">
    <ume-attributes>
      <option type="store" name="disk-cache-directory" value="/home/tmp/sford/cache1a"/>
      <option type="store" name="disk-state-directory" value="/home/tmp/sford/state1a"/>
...
````
Note that the directory names vary for each configuration file.
Use the same interface CIDR value as in the "um.xml" file.

For "store_1a.xml", ensure the "cache" directory is on host S1's fast disk
and the "state" directory is on a different (not-fast) disk.
For the other configuration files, the "cache" and "state" directories can be
on the same disk.
In all cases, the directories must be on disks that are local to the hosts,
not network mounts.

### Measure System Interruptions

As explained in [Measurement Outliers](#measurement-outliers),
there are many sources of application execution interruption that are
beyond the control of UM.
These interruptions result in latency outliers.

To get an idea of the magnitude of outliers on your host,
run the jitter test.

Open two "terminal" windows to your test host.

***Window 1***: run "top -d 1" to continuously display system usage statistics.
When "top" is running, press the "1" key.
This displays per-CPU statistics.
It may be helpful to expand this window vertically to maximize the number
of lines displayed.

***Window 2***: run "taskset 0x01 um_perf_jitter -a 1 -j 200000000 -s 150".
Substitute the "-a 0x01" and the "-a 1" with the non-time-critical CPU bitmask
and the time-critical CPU number you previously chose.
For example:
````
taskset 0x01 um_perf_jitter -a 1 -j 200000000 -s 150
o_affinity_cpu=1, o_jitter_loops=200000000, o_spin_cnt=150, ts_min_ns=186, ts_max_ns=49743,
````

In this run of the jitter test, "ts_min_ns=186" represents the time,
in nanoseconds, to execute the 150-count spin loop plus one call to
"clock_gettime()".
The "ts_max_ns=49743" (49.7 microseconds) represents the longest interruption
observed.
Different hosts can experience radically different interrupt outliers.

On a publishing CPU, this kind of interrupt can lead to a pause in messages,
followed by a burst (to get caught up).
On a subscribing CPU, this kind of interrupt can lead to buffering of messages
in the network socket buffer, followed by a burst of message deliveries.

### Measure Maximum Sustainable Message Rates

The following hosts are referenced:
````
1 - host running the subscriber.
2 - host running the publisher.
S1 - host running a Store. Must have a fast disk.
S2 - for 3-Store tests, host running the second store.
S3 - for 3-Store tests, host running the third store.
````

Note that in all tests, the command-line for host 1 (subscriber) is the same.

Also, note that the CPU affinities used are optimal for the Informatica hosts
that we used.
You will need to experiment with CPU numbers on your hardware to determine
your optimum choices.

#### Test 1: Streaming

Single source, single receiver, streaming (no Store).

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts, potentially minus 1 (due to head loss).
I.e., in this test, it might be 50000015 or 50000014.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 700 -n 50000000 -r 999999999 -t topic1 -w 15,5
````
When the publisher completes, the output should be something like:
````
actual_sends=50000000, duration_ns=33471056772, result_rate=1493827.946354, global_max_tight_sends=49648605, max_flight_size=50000014
````
Since the requested rate is 999M msgs/sec, which is far greater than line rate,
the publisher sends most of its messages in a tight loop as fast as it can,
with a resulting rate of 1.519M msgs/sec.

#### Test 2: Single SPP Store

Single source, single SPP-based Store, single receiver.
SPP means that it writes all messages to disk.
This characterizes a single disk-based store's performance (throughput).

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host S1 (Store):
````
umestored -a "4,2,4,4,6" store_1a.xml | tee store1a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 700 -n 50000000 -r 550000 -t topic1 -w 15,5 -p s
````
When the publisher completes, the output should be something like:
````
actual_sends=50000000, duration_ns=90909090526, result_rate=550000.002318, global_max_tight_sends=33, max_flight_size=29535
````

#### Test 3: Single RPP Store

Single source, single RPP-based Store, single receiver.
This allows us to compare SPP to RPP.
For the hardware we used, RPP is 15% faster than SPP.

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host S1 (Store):
````
umestored -a "4,2,4,4,6" store_1a.xml | tee store1a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 700 -n 50000000 -r 760000 -t topic1 -w 15,5 -p r
````
When the publisher completes, the output should be something like:
````
actual_sends=50000000, duration_ns=65789473262, result_rate=760000.004877, global_max_tight_sends=2241, max_flight_size=81726
````

#### Test 4: Quorum/Consensus

Single source, three RPP-based Stores in quorum/consensus, single receiver.
This allows us to measure the impact of a 3-Store Q/C group compared to
a single-store Q/C group.

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host S1 (Store):
````
umestored -a "4,2,4,4,6" store_1a.xml | tee store1a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host S2 (Store):
````
umestored -a "3,1,3,3,5" store_1b.xml | tee store1b.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host S3 (Store):
````
umestored -a "3,1,3,3,5" store_1c.xml | tee store1c.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 700 -n 50000000 -r 740000 -t topic1abc -w 15,5 -p r
````
When the publisher completes, the output should be something like:
````
actual_sends=50000000, duration_ns=67567629876, result_rate=739999.317599, global_max_tight_sends=1094, max_flight_size=83218
````

#### Test 5: Load Balance

Three sources (single sending thread), three RPP-based Stores (one per source),
three receivers (single receiver thread).
This demonstrates balancing the load across multiple Stores
(each Store only sees one-third of the messages).

Note that inter-topic message ordering is not guaranteed.

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host S1 (Store):
````
umestored -a "4,2,4,4,6" store_1a.xml | tee store1a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host S2 (Store):
````
umestored -a "3,1,3,3,5" store_2a.xml | tee store2a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host S3 (Store):
````
umestored -a "3,1,3,3,5" store_3a.xml | tee store3a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 700 -n 50000000 -r 999999999 -t topic1,topic2,topic3 -w 15,5 -p r
````
When the publisher completes, the output should be something like:
````
actual_sends=50000000, duration_ns=48010773767, result_rate=1041432.913426, global_max_tight_sends=49104891, max_flight_size=105913
````

#### Test 6: Three-Source Streaming

Three sources (single sending thread), streaming (no Store).
This demonstrates that sending to three streaming sources can send almost
as fast as a single streaming source.

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 700 -n 50000000 -r 999999999 -t topic1,topic2,topic3 -w 15,5
````
When the publisher completes, the output should be something like:
````
actual_sends=50000000, duration_ns=32010201629, result_rate=1562002.032337, global_max_tight_sends=49483772, max_flight_size=50000014
````

#### Test 7: Single SPP Store, Application Batching

Single source, batching two messages,
single SPP-based Store (disk-based), single receiver.
This characterizes application batching two messages together per send.

NOTE: an application batching algorithm is *not* included in the source code.
Instead, the message size was simply increased to 1420,
enough to fit two 700-byte messages plus 20 bytes of overhead.

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host S1 (Store):
````
umestored -a "4,2,4,4,6" store_1a.xml | tee store1a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 1420 -n 25000000 -r 400000 -t topic1 -w 15,5 -p s
````
When the publisher completes, the output should be something like:
````
actual_sends=25000000, duration_ns=62499998820, result_rate=400000.007552, global_max_tight_sends=15, max_flight_size=14359
````

Given the send rate of 400K, and that each send contains two application
messages, the application message rate is 800K messages/sec.

#### Test 8: Single RPP Store, Application Batching

Single source, batching two messages,
single RPP-based Store (disk-based), single receiver.
This characterizes application batching two messages together per send
to an RPP Store.

NOTE: an application batching algorithm is *not* included in the source code.
Instead, the message size was simply increased to 1420,
enough to fit two 700-byte messages plus 20 bytes of overhead.

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host S1 (Store):
````
umestored -a "4,2,4,4,6" store_1a.xml | tee store1a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 1420 -n 25000000 -r 720000 -t topic1 -w 15,5 -p r
````
When the publisher completes, the output should be something like:
````
actual_sends=25000000, duration_ns=34722221743, result_rate=720000.009937, global_max_tight_sends=40, max_flight_size=80605
````

Given the send rate of 720K, and that each send contains two application
messages, the application message rate is 1.44M messages/sec.

#### Test 9: Load Balance, Application Batching

Three sources (single sending thread), batching two messages,
three RPP-based Stores (one per source),
three receivers (single receiver thread).
This demonstrates balancing the load of application batched messages across
multiple Stores (each Store only sees one-third of the messages).

Note that inter-topic message ordering is not guaranteed.

NOTE: an application batching algorithm is *not* included in the source code.
Instead, the message size was simply increased to 1420,
enough to fit two 700-byte messages plus 20 bytes of overhead.

Host 1 (subscriber):
````
EF_POLL_USEC=-1 taskset 0x01 onload ./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Host S1 (Store):
````
umestored -a "4,2,4,4,6" store_1a.xml | tee store1a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host S2 (Store):
````
umestored -a "3,1,3,3,5" store_2a.xml | tee store2a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host S3 (Store):
````
umestored -a "3,1,3,3,5" store_3a.xml | tee store3a.log
````
When the publisher completes, ensure that the Store does NOT display any
"unrecoverable" loss log messages.
Before running another test, wait for the Store to delete the topic (will log
a message of the form "Store-5688-5285: store "..." topic "..." deleted").

If this is the first test run after the Store's startup,
there might be unrecoverable loss due to insufficient "warmup" of the Store.
Repeat the test.

Host 2 (publisher):
````
taskset 0x1 onload ./um_perf_pub -a 1 -x um.xml -m 1420 -n 25000000 -r 750000 -t topic1,topic2,topic3 -w 15,5 -p r
````
When the publisher completes, the output should be something like:
````
actual_sends=25000000, duration_ns=33333332926, result_rate=750000.009165, global_max_tight_sends=57, max_flight_size=74917
````

Given the send rate of 750K, and that each send contains two application
messages, the application message rate is 1.5M messages/sec.

## WARMUP

The publisher test tool has a command-line option:
````
  -w warmup_loops,warmup_rate
````
This sends a series of initial messages at a low rate to the Store(s)
before the throughput send loop starts.
It is needed because the Store creates its files and other infrastructure
after the first application message is sent.
This activity consumes enough time that, at the very high throughput rate,
the Store's socket buffer can overflow and result in loss.

The time duration of the warmup period is most important,
not the number of messages.
We find that after receiving the first message of a new source,
the Store requires about 2 seconds to stabilize and be ready for the
full throughput.
The message rate during that warmup period should be no more than 10%
of the maximum-sustainable message rate.
(Our testing here extends that to 3 seconds for reliability.)

## TOOL USAGE NOTES

### um_perf_pub

````
Usage: um_perf_pub [-h] [-a affinity_cpu] [-c config] [-g]
  [-H histo_num_buckets,histo_ns_per_bucket] [-l linger_ms]
  [-L loss_percentage] [-m msg_len] [-n num_msgs] [-s store_list]
  [-r rate] [-t topic] [-w warmup_loops,warmup_rate] [-x xml_config]
where:
  -h : print help
  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]
  -c config : configuration file; can be repeated [%s]
  -g : generic source [%d]
  -H histo_num_buckets,histo_ns_per_bucket : send time histogram [%s]
  -l linger_ms : linger time before source delete [%d]
  -L loss_percent : Source-side artificial packet loss (after warmup) [%d]
  -m msg_len : message length [%d]
  -n num_msgs : number of messages to send [%d]
  -p ''|r|s : persist mode (empty=streaming, r=RPP, s=SPP) [%s]
  -r rate : messages per second to send [%d]
  -t topics : comma-separated topic strings [\"%s\"]
  -w warmup_loops,warmup_rate : messages to send before measurement [%s]
  -x xml_config : XML configuration file [%s]
````

#### Linger Time

The "-l linger_ms" command-line option introduces a delay between the
last message sent and deletion of the UM source.
Informatica generally recommends a delay before deleting the source to
allow any receivers to get caught up.
Once the source is deleted, a receiver that is behind might experience
a type of unrecoverable loss called
"[tail loss](https://ultramessaging.github.io/currdoc/doc/Design/fundamentalconcepts.html#tailloss)".

#### Warmup

When measuring performance, we recommended performing a number
of "warmup" loops of the time-critical code.
This is to get code and data pages loaded into physical memory, and to
the CPU caches loaded.

The execution of those initial warmup loops is not included in the
performance measurements.

#### Histogram

The "um_perf_pub" tool supports a histogram of time spent inside the UM
"send" call (lbm_src_send or lbm_ssrc_send_ex).
The output can be helpful when tuning a host to reduce latency outliers.

Note that the "um_perf_jitter" tool also supports the same histogram.

Contact UM Support for more information on using the histograms.

### um_perf_sub

````
Usage: um_perf_sub [-h] [-a affinity_cpu] [-c config] [-s spin_cnt]
  [-t topic] [-x xml_config]
where:
  -h : print help
  -a affinity_cpu : CPU number (0..N-1) for receive thread [%d]
  -c config : configuration file; can be repeated [%s]
  -p ''|r|s : persist mode (empty=streaming, r=RPP, s=SPP) [%s]
  -s spin_cnt : empty loop inside receiver callback [%d]
  -t topics : comma-separated topic strings [%s]
  -x xml_config : configuration file [%s]
````

#### Persist Mode

The "-p ''|r|s" option selects between non-persistence (streamin),
RPP persistence, or SPP persistence.
However, this option is only used to select the application name passed
to lbm_config_xml_file(), which in turn is used to select a group of
configurations in the "um.xml" configuration file.
The application names are "um_perf", "um_perf_rpp", and "umm_perf_spp"
respectively.

Note that the publisher and subscriber both select among the same three
application names.
For example, the application name "um_perf_rpp" can contain both source and
receiver options,
making it applicable to both "um_perf_pub" and "um_perf_sub".

However, it turns out that the subscriber run with "-p r" is compatible with 
all three data modes (streaming, RPP, and SPP) since the receiver simply
conforms to the type of source.
So for all tests, we run "um_perf_sub" with "-p r".


#### Spin Count

There is an empty "for" loop inside the receiver callback:
````C
for (global_counter = 0; global_counter < o_spin_cnt; global_counter++) {
}
````
This loop is used to add a small amount of per-message "work" to the subscriber.
Note that the "global_counter" variable is made global so the compiler
won't optimize the loop away.

This option is not used in these tests, but can be used to artificially
slow down the subscriber.

### Affinity

The perf tools' "-a" command-line option is used to specify the CPU core number
to use for the time-critical thread.

For the publisher (um_perf_pub.c),
the time-critical thread is the "main" thread,
since that is the thread that sends the messages.
The publisher program is typically started with affinity set to a
non-critical CPU core, typically 1, using the "taskset" command.
The publisher creates its context, which creates the context thread,
inheriting the CPU affinity to core 1.
Then, before it starts sending messages,
it sets affinity to the CPU core number specified with the "-a"
command-line option.

For the subscriber (um_perf_sub.c),
the time-critical thread is the "context" thread,
since that is the thread that reads the network socket.
The context thread's affinity is set when the context thread delivers the BOS event
(beginning of session) to the application's receiver callback.

ATTENTION: the "taskset" command expects a bitmap of CPUs,
with 0x01 representing CPU number 0, 0x02 representing CPU 1,
0x04 representing CPU 2, etc.
The um_perf tools' "-a" options expect the actual CPU number.

## MEASUREMENT OUTLIERS

The SmartSource transport code is written to provide a very constant
execution time.
Dynamic memory (malloc/free) is not used during message transfer.
There is very little source for measurement outliers
(jitter) in the SmartSource code itself.

However, the measurements made at Informatica show significant outliers.
Two environmental factors cause these outliers:
* Interruptions.
* Memory contention and cache invalidation.

### Interruptions

There are many sources of execution interruptions on a CPU core running
a typical OS (Linux or Windows).
Some of them are actual hardware interrupts, like page faults,
disk controllers, NICs, and timers.
Others are soft, like virtual memory maintenance,
scheduler-related operations, and potentially even system
or user processes "stealing" time from the main application.
It is possible to eliminate or at least reduce many of these sources of
interrupt by modifying the host's configuration,
both in its BIOS and the kernel.
For example, see:
* https://lwn.net/Articles/549580/
* https://lwn.net/Articles/659490/
* https://www.kernel.org/doc/Documentation/timers/NO_HZ.txt

There are many other kernel tweaks and optimizations that can be made
to prevent interruptions and allow the application to minimize instances
of giving up the CPU.
Informatica recommends that users work with an experienced Linux performance
engineer to understand the tradeoffs of potential optimizations.
However, Informatica does not know of any way to eliminate
all interruptions.

Without doing these optimizations,
the test results are highly susceptible to interruptions.

See [Measure System Interruptions](#measure-system-interruptions)
for a method to measure these interruptions.

## CODE NOTES

We attempt to explain some of the "why"s of non-obvious parts of the code.

### Error Handling

Informatica strongly advises users to check return status for errors after
every UM API call.
As this can clutter the source code, making it harder to read,
the "um_perf_pub.c" and "um_perf_sub.c" programs use a code macro called
"E()" to make the handling of UM API errors uniform and non-intrusive.
For example:
````C
E(lbm_config(o_config));
````

The "E()" code macro checks for error, prints the source code file name,
line number, and the UM error message, and then calls "exit(1)",
terminating the program with bad status.

In a production program, users typically have their own well-defined
error handling conventions which typically includes logging messages to
a file and alerting operations staff of the exceptional condition.
Informatica does not recommend the use of this "E()" macro in production
programs, at least not as it is implemented here.

Another simple shortcut macro is "PERRNO()" which prints the source code
file name, line number, and the error message associated with the contents
of "errno", then calls "exit(1)", terminating the program with bad status.
This is useful if a system library function fails.

### SAFE_ATOI

This is a helper macro that is similar to the system function "atoi()"
with three improvements:
* Automatically conforms to different integer types
(8-bit, 16-bit, 32-bit, 64-bit, signed or unsigned).
* Treats numbers with the "0x" prefix as hexadecimal.
* Adds significant error checking and reporting.

As with the "E()" macro, it accomplishes these goals without code clutter.

### DIFF_TS

This is a helper macro that subtracts two "struct timespec" values, as
returned by [clock_gettime()](https://linux.die.net/man/3/clock_gettime),
and puts the difference into a uint64_t variable
as the number of nanoseconds between the two timestamps.

For example:
````C
struct timespec start_ts;
struct timespec end_ts;
uint64 duration_ns;  /* In nanoseconds. */

clock_gettime(&start_ts);
... /* code to be timed */
clock_gettime(&end_ts);
DIFF_TS(duration_ns, end_ts, start_ts);
````

### send_loop()

The "send_loop()" function in "um_perf_pub.c" does the work of
sending messages at the desired rate.
It is designed to "busy-loop" between sends so that the time spacing between
messages is as constant and uniform as possible.
The publisher's traffic is not subject to bursts and pauses.

This approach is not intended to model the behavior of a real-life trading system,
where message traffic is highly subject to intense bursts.
Generating bursty traffic is very important when testing trading system
designs,
but is not desired when measuring maximum sustainable throughput.

Maximum sustainable throughput is the message rate at which the subscriber
can just barely keep up.
Sending a burst of traffic at a higher rate can be accommodated
temporarily by buffering the excess messages until the burst is over.
After the burst, the send rate needs to drop below the maximum sustainable
message rate so that the subscriber can empty the buffer and get caught up.
But none of this is useful in measuring the maximum sustainable throughput.
Instead, evenly-spaced messages should be sent to get an accurate measurement
of the maximum sustainable throughput.
This gives you a baseline for calculating the size of the buffer required to
handle bursts of a maximum intensity and duration.

When running at or near the maximum sustainable throughput,
some amount of buffering latency is inevitable due to the subscriber being
susceptible to execution [interruptions](#interruptions).
This contributes to latency variation since messages after a
subscriber interruption can experience buffering latency if the subscriber
hasn't yet gotten caught up.

## Notes on Going Fast

### RPP Vs. SPP

There are two forms of persistence that UM supports:
* SPP - Source-Paced Persistence.
* RPP - Receiver-Paced Persistence.

These forms differ in many ways, most of which are not relevant to this
report.
Of particular interest when measuring performance is the way that the
disk is used.

With SPP, every message sent by the source is recorded to disk.

With RPP, messages are not normally written to disk.
Instead, messages are deleted from the Store after the receiver(s) acknowledge
the messages, typically without being written to disk.
See [RPP: Receiver-Paced Persistence](https://ultramessaging.github.io/currdoc/doc/UME/operationalview.html#receiverpacedpersistenceoperations)
for full details.

Most persistence users configure for SPP,
so measuring SPP performance is the primary goal of this report.
However, SPP's performance is very dependent on a host's disk speed.
During engineering testing, it is rare for a test lab to have multiple
systems with fast disks.

By testing with RPP, you can get a good idea of the Store's performance
on a given host, even if that host does not have a high-speed disk.

RPP will typically outperform SPP on a fast disk,
so performance estimates need to be adjusted accordingly.

### Core Count and Network Interfaces

The IT industry has been moving towards fewer hosts with higher core counts
in each host (a.k.a. "server consolidation").
64 core hosts are common; higher core counts are readily available.
However, because the cores must compete for main memory,
these high-count hosts tend to have lower clock frequencies and slower
memory accesses.
The result is that while you can have very many threads running in parallel,
the speed of a given thread can be lower than in a host with fewer cores.
When you are trying to maximize the throughput of a single thread,
you may need more hosts with fewer cores each.
Over-consolidation will lead to failure to achieve the highest throughput.

The creation of virtual machines with small numbers of cores does not solve
this issue if the underlying server hardware has low clock frequencies or
low memory bandwidth.

In particular, users of "blade servers" have had bad luck achieving high
throughput and low latency.
This is especially true for blade enclosures that aggregate network
interfaces into interconnect devices (such as switches) built into the
blade enclosure or in networking blades.
This can force network traffic to be routed via software to the required
blade(s).
This can result in high packet loss, especially for multicast traffic.

Finally, there are specialized network interface cards from vendors
like Xilinx (formerly Solarflare) and Cisco (formerly Exablaze)
that support "kernel-bypass" drivers.
This technology is necessary to achieve the very high performance
demonstrated in this report.

Ultra Messaging is designed to get the highest messaging performance
possible from any given hardware platform.
However, to achieve the highest possible performance,
hardware must be chosen with attention to these issues.

### Intelligent Batching

When we send 700-byte messages in separate packets,
we can get a send rate of 1.5M packets per second.
However, hosts with slower CPUs might not be able to keep up with this
packet rate.
Batching might be needed to keep up reliably.

Batching - the combining of many small application messages into a smaller
number of network packets - has gotten a bad reputation among
low-latency developers.
This is because many batching algorithms include timers to flush out partial
batches, which can introduce milliseconds of latency.
For this reason, most low-latency applications flush every message.
However, as the throughput requirement rises,
it might not be possible to achieve the desired
message rate without batching.

Informatica recommends using an algorithm that we call "intelligent batching"
to provide batching without introducing latency outliers.
In fact, intelligent batching can reduce average latency in an intense
burst of messages, while retaining optimum latency at low message rates.

The basic principle of intelligent batching is to have some knowledge of
the availability of "future" messages.
For example, you might have a sending thread that pulls messages off a
queue and sends them to the messaging layer.
After dequeuing a message, the sending thread can examine the queue to
determine if there is another message waiting.
If so, the application can take appropriate steps to combine the current
message with that future message.
(The details depend on the nature of the application and the type of
messaging layer send function being used.)
This provides a self-adaptive algorithm - at low message rates,
each message is flushed immediately without waiting.
As the message rate increases, messages will become available more quickly
than they can be sent individually.
At the point where the send thread is one message behind,
it will automatically batch two messages, which will essentially let the
send thread catch up.

This is how TCP can achieve very high throughputs.
The send-side socket buffer is the "queue", and the device driver is the
"send thread".
When the incoming data rate is higher than the driver can keep up with,
the socket buffer will build up.
The next time the driver is ready to send a packet,
it will pull a full packet's worth of data (minus protocol overhead)
and send it.

For an application that has no practical means of determining the
availability of future messages,
a queue and send thread can be added to the application design.
This provides a TCP-like self-adapting batching algorithm that optimizes
latency at both low and high message rates.

If you conclude that you cannot tolerate this batching,
you can send messages in individual packets.
You will not be able to sustain the same rate reliably as batching over a
long-running test, but if your sustainable rate meets your requirements,
then we still recommend designing your message format to accommodate
application batching in the future should you require it.
It can be very difficult to retrofit message format changes after
initial deployment.

### Burst VS. Sustain?

When designing a high-throughput system,
it is important to set goals for the expected average message rate and
expected maximum burst message rate.
It is prudent to design the system to be capable of sustaining the
maximum burst message rate over a significant period,
much longer than the expected burst durations.

In this demonstration, we can sustain 1.64M msgs/sec.
However, be aware that at this rate, the 10G Ethernet is fully saturated
(possibly "over" saturated; see "Exceeding Line Rate" below).
This leaves no headroom for error recovery or future growth.

Informatica assumes that the requirement to handle 1.5M msgs/sec is a
case of ensuring that the system can sustain the expected burst rates,
not that the planned production system will routinely maintain
1.5M msgs/sec over long periods.

#### Loss Recovery at 1.64M Msgs/Sec

In our testing, we saw zero loss.
This is because the tests were very clean - there was only one publisher
and one subscriber with no other traffic to interfere.
The subscriber was able to keep up with the publisher,
so no buffers overflowed.

We did some testing introducing artificial loss.
The results were expected: running at 1.64M msgs/sec with 712-byte
messages leaves no bandwidth for loss recovery.
Our testing has verified that a very low loss rate can be recovered
successfully at a high sustained message rate.
But any loss of more than a few packets per second will result in
significant unrecoverable loss for the lossy receiver.
The receiver does not degrade gradually,
it degrades suddenly and significantly ("falls off a cliff").

However, 1.64M msgs/sec is only expected during bursts of traffic,
followed by a significantly lower message rate,
then LBT-RM can be easily tuned to recover from significant loss
during those bursts.

The best approach here is to model the expected traffic patterns with
a traffic generator,
with loss artificially introduced during bursts,
and verify that the LBT-RM algorithms properly recover the loss.

### Exceeding Line Rate?

Look again at [test 9 results](#test-9-load-balance-application-batching).
Here's the output of "um_perf_pub":

````
actual_sends=50000000, duration_ns=60716897922, result_rate=823493.981267, global_max_tight_sends=49100739, max_flight_size=90382
````

Be aware that the 1420 bytes of user data are combined with overhead
bytes from UM, UDP, and IP to produce a 1494-byte IP datagram.
This datagram is encapsulated in an ethernet frame that adds 30 more bytes.
And the Ethernet standard requires an interpacket gap equal to 12 bytes.
So each frame represents 8*(1494+30+12) = 12,288 bit times.
The publisher sent 50,000,000 of those 12,288-bit frames in 60,716,897,922 ns.
That works out to a line rate of 10.11 gigabits per second.
But the network should only run at 10 gigabits per second.
How is it possible that our test ran faster than 10G?

If we speculate that Solarflare eliminates the interpacket gap,
we get 10.04 gigabits per second.
Solarflare may not abide by the 12-byte-time interpacket gap.
According to
[Wikipedia](https://en.wikipedia.org/wiki/Interpacket_gap#Ethernet):
````
Some manufacturers design adapters with a smaller interpacket gap for
slightly higher data transfer rates. That can lead to data loss when
mixed with standard adaptors.
````

Informatica recommends using the same vendor's NICs for all publishers
and subscribers of high-throughput data.

### Busy Looping

The subscriber commands are run with:

````
EF_POLL_USEC=-1 taskset 0x10 onload ...
````

The "EF_POLL_USEC=-1" environment variable is a special Onload control
that tells it to busy loop when epoll is called.
This causes the UM context thread to become CPU-bound and consume 100%
of the CPU core, independent of message rate.
This improves the ability of the receiver to keep up with high rates.
In our testing, it was not needed for sending 1424-byte messages.
However, for smaller messages that can achieve higher rates,
it quickly became necessary.

Given that busy looping is also recommended to minimize latency,
we recommend its use in all high-performance data reception flows.

### Application Optimizations

The applications used in this demonstration were not written specifically
to test the maximum possible throughput with the maximum stability.
A number of optimizations should be made to real applications that
require extremely high throughput.

Recommended application optimizations:
1. Use the XSP feature to map heavy data streams to specific threads,
each of which is given exclusive use to a CPU core.
This removes CPU contention between critical threads.
2. Critical processing threads should not be burdened with collecting and
reporting logs and statistics.
We recommend the use of the Automatic Monitoring feature.
3. The UM logger callback has the potential to introduce latencies,
depending on how the application writes the logs.
We recommend passing logs to a queue for processing by a separate
non-critical thread.
Note that this queue must be multi-writer (different threads can be enqueuing)
and the enqueue operation should not use locks or dynamic memory
(malloc/free, new/delete).

### Host Optimizations

The hosts in Informatica's labs have had very few optimizations done to them.
We want our systems to be as off-the-shelf as practical so that our testing
applies to as many customer environments as possible.

A high-performance production system should have various optimizations done
to minimize interruptions of critical application threads by the operating
system.
However,
it is hard to give generic advice here that applies to everybody.
For example, for users of kernel network drivers,
IRQs and interrupt coalescing are important considerations for
latency/throughput tradeoffs.
But not for many Onload users who avoid using interrupts.
There are too many tradeoffs to provide a simple checklist of tunings.

Here are some resources:

https://access.redhat.com/sites/default/files/attachments/201501-perf-brief-low-latency-tuning-rhel7-v2.1.pdf

https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/7/html-single/tuning_guide/index

https://access.redhat.com/articles/3720611  (Requires Red Hat subscription.)

https://www.kernel.org/doc/Documentation/kernel-per-CPU-kthreads.txt

Note that even without these optimizations,
our test could send and receive at full line rate.
So the above optimizations are recommended to provide additional headroom.

## INFORMATICA TEST HARDWARE

Here are command excerpts that document the hosts used to
perform the in-house measurements.

### Host 1

Host 1 - subscriber

````
crush:um_perf$ lscpu
Architecture:          x86_64
CPU op-mode(s):        32-bit, 64-bit
Byte Order:            Little Endian
CPU(s):                12
On-line CPU(s) list:   0-11
Thread(s) per core:    1
Core(s) per socket:    6
Socket(s):             2
NUMA node(s):          2
Vendor ID:             GenuineIntel
CPU family:            6
Model:                 79
Model name:            Intel(R) Xeon(R) CPU E5-2643 v4 @ 3.40GHz
Stepping:              1
CPU MHz:               1199.960
BogoMIPS:              6804.84
Virtualization:        VT-x
L1d cache:             32K
L1i cache:             32K
L2 cache:              256K
L3 cache:              20480K
NUMA node0 CPU(s):     0,2,4,6,8,10
NUMA node1 CPU(s):     1,3,5,7,9,11
...

crush:um_perf$ vmstat -s
     65689948 K total memory
...

crush:um_perf$ cat /etc/centos-release
CentOS Linux release 7.3.1611 (Core)
````

### Host 2

Host 2 - publisher

````
hal:um_perf$ lscpu
Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              16
On-line CPU(s) list: 0-15
Thread(s) per core:  2
Core(s) per socket:  8
Socket(s):           1
NUMA node(s):        1
Vendor ID:           GenuineIntel
CPU family:          6
Model:               85
Model name:          Intel(R) Core(TM) i7-9800X CPU @ 3.80GHz
Stepping:            4
CPU MHz:             3421.062
CPU max MHz:         4500.0000
CPU min MHz:         1200.0000
BogoMIPS:            7600.00
Virtualization:      VT-x
L1d cache:           32K
L1i cache:           32K
L2 cache:            1024K
L3 cache:            16896K
NUMA node0 CPU(s):   0-15
...

hal:um_perf$ vmstat -s
     32465552 K total memory
...

hal:um_perf$ cat /etc/centos-release
CentOS Linux release 8.1.1911 (Core)
````

### Host S1

Host S1 - Store

This host probably most-resembles the kinds of hosts that our customers
would use.
It does not have the highest possible CPU speed, but should be close to
most customers' modern production systems.
It does have a fast SSD disk, which is optimized for writing.
Informatica strongly recommends using SSD disks that are optimized for
writing for disk-based Stores.

````
zeus:um_perf$ lscpu
Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              32
On-line CPU(s) list: 0-31
Thread(s) per core:  1
Core(s) per socket:  16
Socket(s):           2
NUMA node(s):        2
Vendor ID:           GenuineIntel
CPU family:          6
Model:               85
Model name:          Intel(R) Xeon(R) Gold 6242 CPU @ 2.80GHz
Stepping:            7
CPU MHz:             3146.526
CPU max MHz:         3900.0000
CPU min MHz:         1200.0000
BogoMIPS:            5600.00
Virtualization:      VT-x
L1d cache:           32K
L1i cache:           32K
L2 cache:            1024K
L3 cache:            22528K
NUMA node0 CPU(s):   0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30
NUMA node1 CPU(s):   1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31
...

zeus:um_perf$ vmstat -s
     65234840 K total memory
...

zeus:um_perf$ cat /etc/centos-release
CentOS Linux release 8.2.2004 (Core)
````

### Host S2

Host S2 - Store

````
jarvis:um_perf$ lscpu
Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              16
On-line CPU(s) list: 0-15
Thread(s) per core:  2
Core(s) per socket:  8
Socket(s):           1
NUMA node(s):        1
Vendor ID:           GenuineIntel
CPU family:          6
Model:               85
Model name:          Intel(R) Core(TM) i7-9800X CPU @ 3.80GHz
Stepping:            4
CPU MHz:             1683.597
CPU max MHz:         4500.0000
CPU min MHz:         1200.0000
BogoMIPS:            7600.00
Virtualization:      VT-x
L1d cache:           32K
L1i cache:           32K
L2 cache:            1024K
L3 cache:            16896K
NUMA node0 CPU(s):   0-15
...

jarvis:um_perf$ vmstat -s
     32465552 K total memory
...

jarvis:um_perf$ cat /etc/centos-release
CentOS Linux release 8.1.1911 (Core)
````

### Host S3

Host S3 - Store

````
mamba:um_perf$ lscpu
Architecture:          x86_64
CPU op-mode(s):        32-bit, 64-bit
Byte Order:            Little Endian
CPU(s):                16
On-line CPU(s) list:   0-15
Thread(s) per core:    2
Core(s) per socket:    8
Socket(s):             1
NUMA node(s):          1
Vendor ID:             GenuineIntel
CPU family:            6
Model:                 85
Model name:            Intel(R) Core(TM) i7-9800X CPU @ 3.80GHz
Stepping:              4
CPU MHz:               1200.024
CPU max MHz:           4500.0000
CPU min MHz:           1200.0000
BogoMIPS:              7600.00
Virtualization:        VT-x
L1d cache:             32K
L1i cache:             32K
L2 cache:              1024K
L3 cache:              16896K
NUMA node0 CPU(s):     0-15
...

mamba:um_perf$ vmstat -s
     32467580 K total memory
...

mamba:um_perf$ cat /etc/centos-release
CentOS Linux release 7.9.2009 (Core)
````
