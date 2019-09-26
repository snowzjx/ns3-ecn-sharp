# ns3 Simulator for ECN#

## Download and Compile

1. Ubuntu + gcc-4.9 has been verified to compatiable with the project.

``` docker run -it gcc:4.9 ```

2. Clone the proejct.

``` git clone git@github.com:snowzjx/ns3-ecn-sharp.git ```

3. Configuration.

``` cd ns3-ecn-sharp ```

``` ./waf -d optimized --enable-examples configure ```

4. If you want to enable the debug mode for logging, can pass ```-d debug ``` to the configuration.

``` ./waf -d debug --enable-examples configure ```

5. Compile the simulator.

``` ./waf ```

## Docker Image

You can also directly use our docker image for this simulator.

``` docker run -it snowzjx/ns3-ecn-sharp:optimized ```

``` cd ~/ns3-ecn-sharp ```

## ECN# Implementation

The ECN# (ECN Sharp)'s implementation is here:

[https://github.com/snowzjx/ns3-ecn-sharp/blob/master/src/traffic-control/model/ecn-sharp-queue-disc.h](https://github.com/snowzjx/ns3-ecn-sharp/blob/master/src/traffic-control/model/ecn-sharp-queue-disc.h)

[https://github.com/snowzjx/ns3-ecn-sharp/blob/master/src/traffic-control/model/ecn-sharp-queue-disc.cc](https://github.com/snowzjx/ns3-ecn-sharp/blob/master/src/traffic-control/model/ecn-sharp-queue-disc.cc)

### Measuring the sojourn time

The sojourn time is measured using ```ECNSharpTimestampTag```.

When a packet enqueues, we add a timestamp tag on the packet.

```
ECNSharpTimestampTag tag;
p->AddPacketTag (tag);
GetInternalQueue (0)->Enqueue (item);
```

When a packet dequeues, we calculate the sojourn time by deducing enqueue timestamp from current timestamp.

```
Ptr<QueueDiscItem> item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ());
Ptr<Packet> p = item->GetPacket ();
ECNSharpTimestampTag tag;
bool found = p->RemovePacketTag (tag);
if (!found)
{
  NS_LOG_ERROR ("Cannot find the ECNSharp Timestamp Tag");
  return NULL;
}
Time sojournTime = now - tag.GetTxTime ();
```

### Instantaneous ECN Marking

```
if (sojournTime > m_instantMarkingThreshold)
{
  instantaneousMarking = true;
}
```

### Persistent ECN Marking
```
bool okToMark = OkToMark (p, sojournTime, now);
if (m_marking)
{
  if (!okToMark)
  {
    m_marking = false;
  }
  else if (now >= m_markNext)
  {
    m_markCount ++;
    m_markNext = now + ECNSharpQueueDisc::ControlLaw ();
    persistentMarking = true;
  }
}
else
{
  if (okToMark)
  {
    m_marking = true;
    m_markCount = 1;
    m_markNext = now + m_persistentMarkingInterval;
    persistentMarking = true;
   }
}
```

### Mark ECN based on the above 2 conditions
```
if (instantaneousMarking || persistentMarking)
{
  if (!ECNSharpQueueDisc::MarkingECN (item))
  {
    NS_LOG_ERROR ("Cannot mark ECN");
    return item; // Hey buddy, if the packet is not ECN supported, we should never drop it
   }
}
```

## Run Simulations
Run ```large-scale``` program for this experiment:

```
./waf --run "large-scale --help"
```

Please note, the default simulation time is very short, you should tune the simulation time by setting the ```--EndTime``` and ```--FlowLaunchEndTime``` to obtain a similar results in our paper.

In this program, TCN is identical to RED because here we only use one queue.

You should run:
```
./waf --run "large-scale --randomSeed=233 --load=0.6 --ID=TCN_High --AQM=TCN --TCNThreshold=70"
```
```
./waf --run "large-scale --randomSeed=233 --load=0.6 --ID=TCN_Low --AQM=TCN --TCNThreshold=30"
```
```
./waf --run "large-scale --randomSeed=233 --load=0.6 --ID=ECNSharp --AQM=ECNSharp --ECNShaprInterval=70 --ECNSharpTarget=10 --ECNShaprMarkingThreshold=70"
```

to compare the ECN#, RED with marking threshold calculated based on tail RTT and average RTT.

After simulation finishes, you will get a flow monitor file. The file is xml format and can be parsed by ```fct_parser.py``` script. Please note, our [flow monitor](https://github.com/snowzjx/ns3-ecn-sharp/blob/master/src/flow-monitor/model/flow-monitor.cc) is slight different from the original version (some bugs are fixed).

```
python examples/rtt-variations/fct_parser.py Large_Scale_TCN_High_4X4_TCN_DcTcp_0.6.xml
```
```
python examples/rtt-variations/fct_parser.py Large_Scale_TCN_Low_4X4_TCN_DcTcp_0.6.xml
```
```
python examples/rtt-variations/fct_parser.py Large_Scale_ECNSharp_4X4_ECNSharp_DcTcp_0.6.xml
```

You can obtain the results as follows. Here we give a sample with default parameters (short simulation time) only to demonstrate the trends.

For ECN#:
```
...
AVG FCT: 0.009724
AVG Large flow FCT: 0.075342
AVG Small flow FCT: 0.001556
AVG Small flow 99 FCT: 0.008763
...
```

For RED (TCN) with marking threshold calculated based on high percentile RTT:
```
...
AVG FCT: 0.010133
AVG Large flow FCT: 0.073115
AVG Small flow FCT: 0.002375
AVG Small flow 99 FCT: 0.009398
...
```

For RED (TCN) with marking threshold calculated based on average RTT:
```
...
AVG FCT: 0.009593
AVG Large flow FCT: 0.081166
AVG Small flow FCT: 0.001483
AVG Small flow 99 FCT: 0.008892
...
```
We can see RED suffers from either throughput loss (poor FCT for all flows and large flows) or increased latency (poor FCT and tail FCT for short flows).

ECN# simultaneously deliver high throughput and low latency communications. 

### Queue Track
Run ```queue-track``` program for this experiment:

```
./waf --run "queue-track --help"
```

You can use GNU Plot to plot the queue.
```
gnuplot Queue_Track_ ... .plt
```

The results are as follows, we can see ECN# can at the same time mitigate the persistent queue buildups and tolerate traffic burstiness.

![Queue Track](https://raw.githubusercontent.com/snowzjx/ns3-ecn-sharp/master/examples/rtt-variations/queue-track.png)


### Multi-Queue

Run ```mq``` program for this experiment:

```
./waf --run "mq --help"
```

Both TCN and ECN# will output the throughput of all 3 flows. The results should be similar as follows, which shows both strategy can preserve the packet sceduling policy.

```
...
Flow: 0, throughput (Gbps): 9.57376
Flow: 1, throughput (Gbps): 0
Flow: 2, throughput (Gbps): 0
...
Flow: 0, throughput (Gbps): 6.55424
Flow: 1, throughput (Gbps): 3.01392
Flow: 2, throughput (Gbps): 0
...
Flow: 0, throughput (Gbps): 4.86864
Flow: 1, throughput (Gbps): 2.42928
Flow: 2, throughput (Gbps): 2.2372
...
```

When we anaylzing FCT of all flows, we should obtain the following results. This shows ECN# has much better results for short flows by mitigating the unnecessary persistent queue buildups.

For ECN#:

```
...
AVG Small flow FCT: 0.001329
AVG Small flow 99 FCT: 0.001694
...
```

For TCN:

```
...
AVG Small flow FCT: 0.002105
AVG Small flow 99 FCT: 0.005068
...
```

## Implemented Modules

We have implemented the following transportation protocols, tc modules and load balance schemes in this simulator.

### Transport Protocol

1. [DCTCP](https://people.csail.mit.edu/alizadeh/papers/dctcp-sigcomm10.pdf)

### Traffic Control Module

1. ECN#
2. RED
3. [TCN](http://www.cse.ust.hk/~kaichen/papers/tcn-conext16.pdf)
4. [CoDel(with ECN)](https://queue.acm.org/detail.cfm?id=2209336)
5. DWRR
6. WFQ

### Load Balance Scheme

1. [Hermes](http://www.cse.ust.hk/~kaichen/papers/hermes-sigcomm17.pdf)
2. Per flow ECMP
3. [CONGA](https://people.csail.mit.edu/alizadeh/papers/conga-sigcomm14.pdf)
4. [DRB](http://conferences.sigcomm.org/co-next/2013/program/p49.pdf)
5. [Presto](http://pages.cs.wisc.edu/~akella/papers/presto-sigcomm15.pdf)
6. Weighted Presto, which has to be used together with asymmetric topology
7. [FlowBender](http://conferences2.sigcomm.org/co-next/2014/CoNEXT_papers/p149.pdf) 
8. [CLOVE](https://www.cs.princeton.edu/~jrex/papers/clove16.pdf)
9. [DRILL](http://conferences.sigcomm.org/hotnets/2015/papers/ghorbani.pdf)
10. [LetFlow](https://people.csail.mit.edu/alizadeh/papers/letflow-nsdi17.pdf)

### Routing 

1. [XPath](http://www.cse.ust.hk/~kaichen/papers/xpath-nsdi15.pdf)

## Others
This project is based on [snowzjx/ns3-load-balance](https://github.com/snowzjx/ns3-load-balance) for the SIGCOMM 2017 paper, [Resilient Datacenter Load Balancing in the Wild](http://www.cse.ust.hk/~kaichen/papers/hermes-sigcomm17.pdf).
