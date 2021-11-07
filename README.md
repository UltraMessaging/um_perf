# um_perf - test programs to measure the performance of Ultra Messagingj.

<small><i><a href='http://ecotrust-canada.github.io/markdown-toc/'>Table of contents generated with markdown-toc</a></i></small>

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

## RESULTS

Informatica used the tools in this repository to measure the
maximum-sustainable message rate for streaming and persistent sources.

The Informatica Ultra Messaging computer lab has some fast machines,
but not enough to run a single representative test of Persistence.
That requires 5 fast machines in total, three with fast disks.
But the UM lab only has one machine with a fast disk.

So we devised a series of tests to estimate the performance of a
fully-provisioned production datacenter using a minimally-provisioned lab.

Here are the persistence tests:
1. Single source, single receiver, streaming (no Store).
2. Single source, single SPP-based Store, single receiver.
This characterizes a single disk-based store's performance (throughput).
3. Single source, single RPP-based Store, single receiver.
This allows us to compare SPP to RPP. For the hardware we used,
RPP is 15% faster than SPP.
4. Single source, three RPP-based Stores in quorum/consensus, single receiver.
This allows us to measure the impact of a 3-Store Q/C group compared to
a single-store Q/C group.
5. Three sources (single sending thread), three RPP-based Stores
(one per source), three receivers (single receiver thread).
This demonstrates balancing the load across multiple Stores
(each Store only sees one third of the messages).
6. Three sources (single sending thread), streaming (no Store).
This demonstrates that sending to three streaming sources can send almost as
fast as a single streaming source.

The method of discovering the maximun sustainable throughput is
to run a publisher at a given message rate for at least 1 minute
and check for failure indications. If the test succeeds,
repeat with a higher message rate.
When a test fails,
the message rate is decreased and the test is repeated.
(A test is said to have "failed" if any component has packet loss,
or if the publisher exhausts flight size
(LBM_EWOULDBLOCK send error) due exceeding the disk write speed.)

Note that the comparisons will be *very* dependent on the underlying
hardware (server and disk).
Users are strongly advised to perform the same tests on hardware that
is as close as possible to their anticipated production hardware.

Finally, note that many users choose SSD disks for their high performance.
And while it is true that SSDs eliminate the "seek time" that can severely
limit spinning disk performance,
standard SSDs are typically "optimized for read",
meaning that their write speeds will often be significantly lower than
cheaper spinning disk write speeds.
Informatica strongly recommends selecting SSDs for persistent Stores that
are "optimized for write".

All tests were performed with 700-byte messages that are flushed
(no batching).
UM Smart Sources and Xilinx (formerly Solarflare) 10-gig NICs with
Onload kernel-bypass drivers were used on all machines.
In the results below, "K" represents 1,000; "M" represents 1,000,000;
"G" represents 1,000,000,000 (i.e. they are not powers of 2).

### Test Results

Test | Message Rate | Summary
---- | ------------ | ------------
1 | 1.5M | Streaming (no Stores)
2 | 750K | 1 SPP Store (disk-based)
3 | 860K | 1 RPP Store
4 | 830K | 3 RPP Stores (Q/C)
5 | 1.07M | 3 sources, load balanced
6 | 1.4M | 3 streaming sources (no Stores)

## REPRODUCE RESULTS

It is assumed in all of these steps that the "LD_LIBRARY_PATH" environment
variable includes the path to the Ultra Messaging version 6.14
library directory.
For example:
````
LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.

export LD_LIBRARY_PATH=$LBM/lib
````

Furthermore, it is assumed that the "PATH" environment variable includes
the path to the directory containing the um_perf
executables that you build.

Finally, the "taskset" command is used to run the test programs,
setting affinity to a non-time-critical CPU in the same NUMA node as
the time-critical CPUs.
The test programs use the "-a" command-line option to change the affinity
of the time-critical threads to the time-critical CPUs.
See [Affinity](#affinity).

ATTENTION: the "taskset" command's "-a" option expects a bitmap of CPUs,
with 0x01 representing CPU number 0, 0x02 representing CPU 1,
0x04 representing CPU 2, etc.
The um_perf tools' "-a" options expect the actual CPU number.

### Requirements

1. 5 Linux-based servers (X86, 64-bit).
hyperthreading turned off,
16 gigabytes or more memory.
At least one must have a fast disk.
2. C compiler (gcc) and related tools.
3. Ultra Messaging version 6.14, including development files (lbm.h,
libraries, etc.).

See [Test Hardware](#informatica-test-hardware) for details of Informatica's
test hosts.

### Choose CPUs

Different hardware systems assign CPU numbers to NUMA nodes differently.
Our servers do even/odd assignments, but there are
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

### Build Test Tools

* um_perf_pub - publisher (source).
* um_perf_sub - subscriber (receiver).

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

Also take note of the publisher's 10G interface IP address.
Ours is 10.29.4.121.
It is usually possible to mask off the final 8 bits and use the CIDR form
of the network address.
For example: "10.29.4.0/24".
This can typically be used by all hosts on the same LAN.

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

### Measure System Interruptions

As explained in [Measurement Outliers](#measurement-outliers),
there are many sources of application execution interruption that are
beyond the control of UM.
These interruptions result in latency outliers.

To get an idea of the magnitude of outliers on your system,
run the jitter test.

Open two "terminal" windows to your test host.

***Window 1***: run "top -d 1" to continuously display system usage statistics.
When "top" is running, press the "1" key.
This displays per-CPU statistics.
It may be helpful to expand this window vertically to maximize the number
of lines displayed.
While a test is running, CPU 5 is receiving, and CPU 7 is sending.
Typically, both will be at 100% user mode CPU utilization.

***Window 2***: run "taskset -a 1 um_perf_pub -a 7 -c ump.cfg -f 0x0 -r 100000 -n 1000000 -l 2000 -j 1000000000".
Substitute the "-a 1" and the "-a 7" with the non-time-critical and
time-critical CPUs you previously chose.
For example:
````
taskset -a 1 um_perf_pub -a 7 -c ump.cfg -f 0x0 -r 100000 -n 1000000 -l 2000 -j 1000000000
Core-7911-1: Onload extensions API has been dynamically loaded
o_affinity_cpu=7, o_config=ump.cfg, o_flags=0x00, o_jitter_loops=1000000000, o_linger_ms=2000, o_msg_len=25, o_num_msgs=1000000, o_rate=100000, o_topic='um_perf', o_warmup_loops=10000,
ts_min_ns=12, ts_max_ns=365127,
````

In this run of the jitter test, "ts_min_ns=12" represents the time,
in nanoseconds, to execute "clock_gettime()" once.
The "ts_max_ns=365127" represents the longest interruption observed
during the 1G loops.

### Measure Maximum Sustainable Message Rate

## TOOL USAGE NOTES

### um_perf_pub

````
Usage: um_perf_pub [-h] [-a affinity_cpu] [-c config] [-f flags]
    [-j jitter_loops] [-l linger_ms] [-m msg_len] [-n num_msgs]
    [-r rate] [-t topic] [-w warmup_loops]
where:
  -h : print help
  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]
  -c config : configuration file; can be repeated [%s]
  -f flags : bitmap [0x%x]: 0x01: FLAGS_TIMESTAMP (to measure latency)
                           0x02: FLAGS_NON_BLOCKING
                           0x04: FLAGS_GENERIC_SRC
  -j jitter_loops : jitter measurement loops [%d]
  -l linger_ms : linger time before source delete [%d]
  -m msg_len : message length [%d]
  -n num_msgs : number of messages to send [%d]
  -r rate : messages per second to send [%d]
  -t topic : topic string [\"%s\"]
  -w warmup_loops : messages to send before measurement loop [%d]
````

### Affinity

The "-a" command-line option is used to specify the CPU core number
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
the time-critical thread is the receiver thread (context thread).
As with the publisher program,
the subscriber program is typically started with affinity set to a
non-critical CPU core, typically 1, using the "taskset" command.
The publisher creates its context, which creates the context thread,
inheriting the CPU affinity to core 1.
The receiver thread sets its affinity during the processing of the
"Beginning Of Stream" (LBM_MSG_BOS) receiver event.

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

### um_perf_sub

````
Usage: um_perf_sub [-h] [-a affinity_cpu] [-c config] [-f]
    [-s spin_cnt] [-t topic]
where:
  -a affinity_cpu : CPU number (0..N-1) for receive thread [%d]
  -c config : configuration file; can be repeated [%s]
  -f : fast - minimal processing in message receive (no latency calcs)
  -s spin_cnt : empty loop inside receiver callback [%d]
  -t topic : topic string [%s]
````

#### Fast

The "-f" command-line option causes the subscriber to use a very minimal
receiver callback that has the lowest-possible per-message execution time.
It is used when measuring the maximum sustainable message rate.

Without "-f", the receiver callback contains code that can sometimes add
enough execution time to mask the effects of
[memory contention](#memory-contention-and-cache-invalidation)
on performance.

#### Spin Count

There is an empty "for" loop inside the receiver callback:
````C
for (global_counter = 0; global_counter < o_spin_cnt; global_counter++) {
}
````
This loop is used to add a small amount of per-message "work" to the subscriber.
Note that the "global_counter" variable is made global so the compiler
won't optimize the loop away.

This is used to explore the effects of
[memory contention](#memory-contention-and-cache-invalidation)
on performance.

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

See [Jitter Measurement](#jitter-measurement) for a method to measure
these interruptions.

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
The message traffic is not subject to bursts.

This approach is not intended to model the behavior of a real-life trading system,
where message traffic is highly subject to intense bursts.
Generating bursty traffic is very important when testing trading system
designs,
but is not desired when measuring maximum sustainable throughput and
latency under load.

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

Similarly, sending bursts of traffic that exceed the maximum
sustainable throughput is not desired for measuring the latency under load
of the underlying messaging system.
Again, a burst can be accommodated temporarily by buffering the excess messages,
but this simply adds buffering latency,
which is not the latency that we are trying to measure.

When running at or near the maximum sustainable throughput,
some amount of buffering latency is inevitable due to the subscriber being
susceptible to [execution interruptions](#interruptions).
This contributes to latency variation since messages after a
subscriber interruption can experience buffering latency if the subscriber
hasn't yet gotten caught up.

## INFORMATICA TEST HARDWARE

Here are command excerpts that document the host used to
perform the in-house measurements.

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
Vendor ID:           GenuineIntel
CPU family:          6
Model:               79
Model name:          Intel(R) Xeon(R) CPU E5-2643 v4 @ 3.40GHz
Stepping:            1
CPU MHz:             1507.237
CPU max MHz:         3700.0000
CPU min MHz:         1200.0000
BogoMIPS:            6800.30
Virtualization:      VT-x
L1d cache:           32K
L1i cache:           32K
L2 cache:            256K
L3 cache:            20480K
NUMA node0 CPU(s):   0,2,4,6,8,10
NUMA node1 CPU(s):   1,3,5,7,9,11
...

$ vmstat -s
     65608176 K total memory
...

$ cat /etc/centos-release
CentOS Linux release 8.0.1905 (Core)
````
