#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/link-monitor-module.h"
#include "ns3/gnuplot.h"

extern "C"
{
#include "cdf.h"
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MQ");

enum AQM {
    TCN,
    ECNSharp
};

// Port from Traffic Generator // Acknowledged to https://github.com/HKUST-SING/TrafficGenerator/blob/master/src/common/common.c
double
poission_gen_interval(double avg_rate) {
    if (avg_rate > 0)
        return -logf(1.0 - (double)rand() / RAND_MAX) / avg_rate;
    else
        return 0;
}

uint32_t sendSize[3];
void CheckThroughput (Ptr<PacketSink> sink, uint32_t senderID) {
    uint32_t totalRecvBytes = sink->GetTotalRx ();
    uint32_t currentPeriodRecvBytes = totalRecvBytes - sendSize[senderID];
    sendSize[senderID] = totalRecvBytes;
    Simulator::Schedule (Seconds (0.02), &CheckThroughput, sink, senderID);
    NS_LOG_UNCOND ("Flow: " << senderID << ", throughput (Gbps): " << currentPeriodRecvBytes * 8 / 0.02 / 1000000000);
}

template<typename T> T
rand_range (T min, T max)
{
    return min + ((double)max - min) * rand () / RAND_MAX;
}

std::string
GetFormatedStr (std::string id, std::string str, std::string terminal, AQM aqm)
{
    std::stringstream ss;
    if (aqm == TCN)
    {
        ss << "MQ_TCN_" << id << "_" << str << "." << terminal;
    }
    else if (aqm == ECNSharp)
    {
        ss << "MQ_ECNSharp_" << id << "_" << str << "." << terminal;
    }
    return ss.str ();
}

int main (int argc, char *argv[])
{
#if 1
    LogComponentEnable ("MQ", LOG_LEVEL_INFO);
#endif

    uint32_t numOfSenders = 5;

    std::string id = "undefined";

    std::string transportProt = "DcTcp";
    std::string aqmStr = "TCN";

    AQM aqm;
    double endTime = 0.4;

    unsigned randomSeed = 0;

    uint32_t bufferSize = 600;

    uint32_t TCNThreshold = 150;

    uint32_t ECNSharpInterval = 200;
    uint32_t ECNSharpTarget = 10;
    uint32_t ECNSharpMarkingThreshold = 150;

    CommandLine cmd;
    cmd.AddValue ("id", "The running ID", id);
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
    cmd.AddValue ("AQM", "AQM to use: TCN and ECNSharp", aqmStr);

    cmd.AddValue ("endTime", "Simulation end time", endTime);
    cmd.AddValue ("randomSeed", "Random seed, 0 for random generated", randomSeed);

    cmd.AddValue ("bufferSize", "The buffer size", bufferSize);

    cmd.AddValue ("TCNThreshold", "The threshold for TCN", TCNThreshold);

    cmd.AddValue ("ECNSharpInterval", "The persistent interval for ECNSharp", ECNSharpInterval);
    cmd.AddValue ("ECNSharpTarget", "The persistent target for ECNSharp", ECNSharpTarget);
    cmd.AddValue ("ECNSharpMarkingThreshold", "The instantaneous marking threshold for ECNSharp", ECNSharpMarkingThreshold);

    cmd.Parse (argc, argv);

    if (transportProt.compare ("Tcp") == 0)
    {
        Config::SetDefault ("ns3::TcpSocketBase::Target", BooleanValue (false));
    }
    else if (transportProt.compare ("DcTcp") == 0)
    {
        NS_LOG_INFO ("Enabling DcTcp");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));
    }
    else
    {
        return 0;
    }

    if (aqmStr.compare ("TCN") == 0)
    {
        aqm = TCN;
    }
    else if (aqmStr.compare ("ECNSharp") == 0)
    {
        aqm = ECNSharp;
    }
    else
    {
        return 0;
    }

    // TCP Configuration
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1400));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
    Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
    Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
    Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (40)));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

    // TCN Configuration
    Config::SetDefault ("ns3::TCNQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::TCNQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::TCNQueueDisc::Threshold", TimeValue (MicroSeconds (TCNThreshold)));


    // ECNSharp Configuration
    Config::SetDefault ("ns3::ECNSharpQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::ECNSharpQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::ECNSharpQueueDisc::InstantaneousMarkingThreshold", TimeValue (MicroSeconds (ECNSharpMarkingThreshold)));
    Config::SetDefault ("ns3::ECNSharpQueueDisc::PersistentMarkingTarget", TimeValue (MicroSeconds (ECNSharpTarget)));
    Config::SetDefault ("ns3::ECNSharpQueueDisc::PersistentMarkingInterval", TimeValue (MicroSeconds (ECNSharpInterval)));

    NS_LOG_INFO ("Setting up nodes.");
    NodeContainer senders;
    senders.Create (numOfSenders);

    NodeContainer receivers;
    receivers.Create (1);

    NodeContainer switches;
    switches.Create (1);

    InternetStackHelper internet;
    internet.Install (senders);
    internet.Install (switches);
    internet.Install (receivers);

    PointToPointHelper p2p;

    TrafficControlHelper tc;

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        uint32_t linkLatency = 30;
	if (i == 2) {
		linkLatency = 40;
        } else if (i == 3) {
  		linkLatency = 70;
	}
        NS_LOG_INFO ("Generate link latency: " << linkLatency);
        p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(linkLatency)));
        p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
        p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (bufferSize));

        NodeContainer nodeContainer = NodeContainer (senders.Get (i), switches.Get (0));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork ();
        tc.Uninstall (netDeviceContainer);
    }

    p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(50)));
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (5));

    NodeContainer switchToRecvNodeContainer = NodeContainer (switches.Get (0), receivers.Get (0));
    NetDeviceContainer switchToRecvNetDeviceContainer = p2p.Install (switchToRecvNodeContainer);


    Ptr<DWRRQueueDisc> dwrrQdisc = CreateObject<DWRRQueueDisc> ();
    Ptr<Ipv4SimplePacketFilter> filter = CreateObject<Ipv4SimplePacketFilter> ();

    dwrrQdisc->AddPacketFilter (filter);

    ObjectFactory innerQueueFactory;
    if (aqm == TCN)
    {
        innerQueueFactory.SetTypeId ("ns3::TCNQueueDisc");
    }
    else
    {
        innerQueueFactory.SetTypeId ("ns3::ECNSharpQueueDisc");
    }


    Ptr<QueueDisc> queueDisc1 = innerQueueFactory.Create<QueueDisc> ();
    Ptr<QueueDisc> queueDisc2 = innerQueueFactory.Create<QueueDisc> ();
    Ptr<QueueDisc> queueDisc3 = innerQueueFactory.Create<QueueDisc> ();

    dwrrQdisc->AddDWRRClass (queueDisc1, 0, 3000);
    dwrrQdisc->AddDWRRClass (queueDisc2, 1, 1500);
    dwrrQdisc->AddDWRRClass (queueDisc3, 2, 1500);

    Ptr<NetDevice> device = switchToRecvNetDeviceContainer.Get (0);
    Ptr<TrafficControlLayer> tcl = device->GetNode ()->GetObject<TrafficControlLayer> ();

    dwrrQdisc->SetNetDevice (device);
    tcl->SetRootQueueDiscOnDevice (device, dwrrQdisc);

    tc.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (bufferSize));
    Ipv4InterfaceContainer switchToRecvIpv4Container = ipv4.Assign (switchToRecvNetDeviceContainer);

    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


    NS_LOG_INFO ("Initialize random seed: " << randomSeed);
    if (randomSeed == 0)
    {
        srand ((unsigned)time (NULL));
    }
    else
    {
        srand (randomSeed);
    }

    uint16_t basePort = 8080;

    NS_LOG_INFO ("Install 3 large TCP flows");
    for (uint32_t i = 0; i < 3; ++i)
    {
        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
        source.SetAttribute ("MaxBytes", UintegerValue (0)); // 150kb
        source.SetAttribute ("SendSize", UintegerValue (1400));
        source.SetAttribute ("SimpleTOS", UintegerValue (i));
        ApplicationContainer sourceApps = source.Install (senders.Get (i));
        sourceApps.Start (Seconds (0.1 * i));
        sourceApps.Stop (Seconds (endTime));

        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort++));
        ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
        sinkApp.Start (Seconds (0.0));
        sinkApp.Stop (Seconds (endTime));
        Ptr<PacketSink> pktSink = sinkApp.Get (0)->GetObject<PacketSink> ();
        Simulator::ScheduleNow (&CheckThroughput, pktSink, i);
    }


    NS_LOG_INFO ("Install 100 short TCP flows");
    for (uint32_t i = 0; i < 100; ++i)
    {
        double startTime = rand_range (0.0, 0.4);
        uint32_t tos = rand_range (0, 3);
        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
        source.SetAttribute ("MaxBytes", UintegerValue (28000)); // 14kb
        source.SetAttribute ("SendSize", UintegerValue (1400));
        source.SetAttribute ("SimpleTOS", UintegerValue (tos));
        ApplicationContainer sourceApps = source.Install (senders.Get (2 + i % 2));
        sourceApps.Start (Seconds (startTime));
        sourceApps.Stop (Seconds (endTime));

        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort++));
        ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
        sinkApp.Start (Seconds (0.0));
        sinkApp.Stop (Seconds (endTime));
    }

    NS_LOG_INFO ("Enabling Flow Monitor");
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    NS_LOG_INFO ("Run Simulations");

    Simulator::Stop (Seconds (endTime));
    Simulator::Run ();

    flowMonitor->SerializeToXmlFile(GetFormatedStr (id, "Flow_Monitor", "xml", aqm), true, true);

    Simulator::Destroy ();

    return 0;
}
