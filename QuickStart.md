# Quick Start for UMP.

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

## INSTALL

For Linux, installing UM consists of executing the "UMP_6.14_Linux-glibc-2.17-x86_64.sh" self-extracting script.
This will create the directory "UMP_6.14" and populate it with the binaries.
You can install it in a system area, or in a user home directory.
There are no system level modifications made, no kernel tweaks. Just a directory of files.

The commands and scripts in this repository assume four environment
variables are set up: LBM_LICENSE_INFO, LBM, LD_LIBRARY_PATH, and PATH.

Here's an example of setting them up:
````
export LBM_LICENSE_INFO LBM LD_LIBRARY_PATH PATH

LBM_LICENSE_INFO="Product=LBM,UME,UMQ,UMDRO:Organization=UM RnD sford (RnD):Expiration-Date=never:License-Key=xxxx xxxx xxxx xxxx"

# Path to the install directory for the UM platform.
LBM="/home/sford/UMP_6.14/Linux-glibc-2.17-x86_64"

LD_LIBRARY_PATH="$LBM/lib"

PATH="$LBM/bin:$PATH"
````


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
Search this file for "10.29" to find all lines that contain a network address.
Change them for your network.

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

The GitHub repository has several store configuration files,
but for the purposes of the quick start, only one is needed:
* store_1a.xml

You should modify the following lines per your environment:
````
...
    <store name="store_a_topic1" interface="10.29.4.0/24" port="12001">
    <ume-attributes>
      <option type="store" name="disk-cache-directory" value="/home/tmp/sford/cache1a"/>
      <option type="store" name="disk-state-directory" value="/home/tmp/sford/state1a"/>
...
````
Use the same interface CIDR value as in the "um.xml" file.

THe "cache" and "state" files should be on disks that are local to the hosts,
not network mounts.

### Test 1: Streaming

Single source, single receiver, streaming (no Store).

Window 1 (subscriber):
````
./um_perf_sub -x um.xml -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts, potentially minus 1 (due to head loss).
I.e., in this test, it might be 50000015 or 50000014.

Window 2 (publisher):
````
./um_perf_pub -x um.xml -m 700 -n 500000 -r 50000 -t topic1
````
This sends 500,000 messages at a rate of 50,000 messages/sec.
When the publisher completes, the output should be something like:
````
actual_sends=500000, duration_ns=xxx, result_rate=xxx, global_max_tight_sends=1, max_flight_size=500000
````

#### Test 2: Single SPP Store

Single source, single SPP-based Store, single receiver.
SPP means that it writes all messages to disk.
This characterizes a single disk-based store's performance (throughput).

Window 1 (subscriber):
````
./um_perf_sub -x um.xml -a 2 -t "topic1,topic2,topic3,topic1abc" -p r
````
When the publisher completes, ensure that the subscriber's "EOS" log ends with
"num_rx_msgs=X, num_unrec_loss=0,".
The "num_rcv_msgs" value should be the sum of the publisher's "-n" and "-w"
message counts.

Window 2 (Store):
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

Window 3 (publisher):
````
./um_perf_pub -a 1 -x um.xml -m 700 -n 500000 -r 50000 -t topic1 -p s
````
When the publisher completes, the output should be something like:
````
actual_sends=500000, duration_ns=xxx, result_rate=xxx, global_max_tight_sends=1, max_flight_size=xxx
````

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
Usage: um_perf_sub [-h] [-a affinity_cpu] [-c config] [-E] [-s spin_cnt]
  [-p persist_mode] [-t topics] [-x xml_config]
where:
  -h : print help
  -a affinity_cpu : CPU number (0..N-1) for receive thread [%d]
  -c config : configuration file; can be repeated [%s]
  -E : exit on EOS [%d]
  -p ''|r|s : persist mode (empty=streaming, r=RPP, s=SPP) [%s]
  -s spin_cnt : empty loop inside receiver callback [%d]
  -t topics : comma-separated topic strings to subscribe [%s]
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

### sock_perf_sub

````
Usage: sock_perf_pub [-h] [-a affinity_cpu] [-g group]
  [-H hist_num_buckets,hist_ns_per_bucket] [-i interface] [-m msg_len] [-n num_msgs]
  [-s store_list] [-r rate] [-s sleep_usec] [-w warmup_loops,warmup_rate]
where:
  -h : print help
  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]
  -g group : multicast group address [%s]
  -H hist_num_buckets,hist_ns_per_bucket : send time histogram [%s]
  -i interface : interface for multicast bind [%s]
  -m msg_len : message length [%d]
  -n num_msgs : number of messages to send [%d]
  -r rate : messages per second to send [%d]
  -s sleep_usec : microseconds to sleep between sends [%d]]
  -w warmup_loops,warmup_rate : messages to send before measurement [%s]
````

This tool does not use UM at all.
It creates a socket and sends multicast datagrams using the same socket API
methods as UM.

Note that this tool was not written to be portable between Linux and Windows.

This tool adds a second method of sending with the "-s sleep_usec" option.
The "-r" and "-s" options are mutually exclusive; "-r rate" uses the catchup
algorithm to send at potentially very high rates without sleeping
(busy looping),
whereas "-s sleep_usec" performs a "usleep()" call between sends.

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

Finally, the "ASSRT()" macro is handy for checking conditions that must
be true.
If not, an error message is printed, and the program exits with bad status.
The error message is not very user-friendly,
and often requires access to the source code to understand.
For example, in the um_perf_pub tool, the "-r rate" option is not really
an "option" - it must be supplied.
If omitted, the "ASSRT(o_rate > 0);" line will print:
````
um_perf_pub.c:166, ERROR: 'o_rate > 0' not true
````
(The line number might be different now.)

### Portability

The "cprt.c" and "cprt.h" files were imported from
https://github.com/fordsfords/cprt
to make the um_perf_pub and um_perf_sub tools portable to Linux and Windows.
However, be aware that as of 27-Nov-2021, no attempt has yet been made to
build or run either tool on Windows.

We hope to try it out on Windows soon.

Also, note that the sock_perf_pub tool is *not* suitable for Windows use.
The socket API is different enough that a separate tool is called for.
(This work is not currently planned.)

### CPRT_ATOI

CPRT_ATOI is a helper macro that is similar to the system function "atoi()"
with three improvements:
* Automatically conforms to different integer types
(8-bit, 16-bit, 32-bit, 64-bit, signed or unsigned).
* Treats numbers with the "0x" prefix as hexadecimal.
* Adds significant error checking and reporting.

As with the "E()" macro, it accomplishes these goals without code clutter.

### CPRT_DIFF_TS

CPRT_DIFF_TS is a helper macro that subtracts two "struct timespec" values, as
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
handle bursts of maximum intensity and duration.

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
