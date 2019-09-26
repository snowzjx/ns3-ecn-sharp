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

#define FLOW_SIZE_MIN 3000  // 3k
#define FLOW_SIZE_MAX 60000 // 60k


// The CDF in TrafficGenerator
extern "C"
{
#include "cdf.h"
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("QueueTrack");

Gnuplot2dDataset queuediscDataset;

enum AQM {
    RED,
    CODEL,
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

template<typename T> T
rand_range (T min, T max)
{
    return min + ((double)max - min) * rand () / RAND_MAX;
}

std::string
GetFormatedStr (std::string id, std::string str, std::string terminal, AQM aqm)
{
    std::stringstream ss;
    if (aqm == RED)
    {
        ss << "Queue_Track_RED_" << id << "_" << str <<  "." << terminal;
    }
    else if (aqm == CODEL)
    {
        ss << "Queue_Track_CODEL_" << id << "_" << str << "." << terminal;
    }
    else if (aqm == ECNSharp)
    {
        ss << "Queue_Track_ECNSharp_" << id << "_" << str << "." << terminal;
    }
    return ss.str ();
}

void
DoGnuPlot (std::string id, AQM aqm)
{
    Gnuplot queuediscGnuplot (GetFormatedStr (id, "Queue", "png", aqm).c_str ());
    queuediscGnuplot.SetTitle ("Queue");
    queuediscGnuplot.SetTerminal ("png");
    queuediscGnuplot.AppendExtra ("set yrange [0:+120]");
    queuediscGnuplot.AddDataset (queuediscDataset);
    std::ofstream queuediscGnuplotFile (GetFormatedStr (id, "Queue", "plt", aqm).c_str ());
    queuediscGnuplot.GenerateOutput (queuediscGnuplotFile);
    queuediscGnuplotFile.close ();
}

void
CheckQueueDiscSize (Ptr<QueueDisc> queue)
{
    uint32_t qSize = queue->GetNPackets ();
    queuediscDataset.Add (Simulator::Now ().GetSeconds (), qSize);
    Simulator::Schedule (Seconds (0.00001), &CheckQueueDiscSize, queue);
}

int main (int argc, char *argv[])
{
#if 1
    LogComponentEnable ("QueueTrack", LOG_LEVEL_INFO);
#endif

    std::string id = "undefined";

    std::string transportProt = "DcTcp";
    std::string aqmStr = "ECNSharp";
    AQM aqm;
    double endTime = 1.0;
    double simEndTime = 2.0;

    uint32_t numOfSenders = 16;

    double load = 0.9;
    std::string cdfFileName = "examples/rtt-variations/VL2_CDF.txt";

    unsigned randomSeed = 0;
    uint32_t flowNum = 1000;

    uint32_t bufferSize = 120;

    uint32_t REDMarkingThreshold = 40;

    uint32_t CODELInterval = 150;
    uint32_t CODELTarget = 10;

    uint32_t ECNSharpInterval = 150;
    uint32_t ECNSharpTarget = 10;
    uint32_t ECNSharpMarkingThreshold = 50;

    CommandLine cmd;
    cmd.AddValue ("id", "The running ID", id);
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
    cmd.AddValue ("AQM", "AQM to use: RED, CODEL and ECNSharp", aqmStr);
    cmd.AddValue ("numOfSenders", "Concurrent senders", numOfSenders);
    cmd.AddValue ("endTime", "Flow launch end time", endTime);
    cmd.AddValue ("simEndTime", "Simulation end time", simEndTime);
    cmd.AddValue ("cdfFileName", "File name for flow distribution", cdfFileName);
    cmd.AddValue ("load", "Load of the network, 0.0 - 1.0", load);
    cmd.AddValue ("randomSeed", "Random seed, 0 for random generated", randomSeed);
    cmd.AddValue ("flowNum", "Total flow num", flowNum);
    cmd.AddValue ("bufferSize", "The buffer size", bufferSize);
    cmd.AddValue ("REDMarkingThreshold", "The RED marking threshold", REDMarkingThreshold);
    cmd.AddValue ("CODELInterval", "The interval parameter in CODEL", CODELInterval);
    cmd.AddValue ("CODELTarget", "The target parameter in CODEL", CODELTarget);
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

    if (aqmStr.compare ("RED") == 0)
    {
        aqm = RED;
    }
    else if (aqmStr.compare ("CODEL") == 0)
    {
        aqm = CODEL;
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
    Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MicroSeconds (1000)));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
    Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MicroSeconds (1000)));
    Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
    Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (40)));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

    Config::SetDefault ("ns3::TcpSocket::ConnCount", UintegerValue (10));
    Config::SetDefault ("ns3::TcpSocket::DataRetries", UintegerValue (10));

    // CoDel Configuration
    Config::SetDefault ("ns3::CoDelQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::CoDelQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::CoDelQueueDisc::Target", TimeValue (MicroSeconds (CODELTarget)));
    Config::SetDefault ("ns3::CoDelQueueDisc::Interval", TimeValue (MicroSeconds (CODELInterval)));

    // RED Configuration
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1400));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));

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

    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (5));

    TrafficControlHelper tc;
    if (aqm == CODEL)
    {
        tc.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    }
    else if (aqm == RED)
    {
        tc.SetRootQueueDisc ("ns3::RedQueueDisc", "MinTh", DoubleValue (REDMarkingThreshold),
                                                  "MaxTh", DoubleValue (REDMarkingThreshold));
    }
    else
    {
        tc.SetRootQueueDisc ("ns3::ECNSharpQueueDisc");
    }

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        uint32_t linkLatency = 10;
	if (i % 5 == 1) {
		linkLatency += 10;
	} else if (i % 5 == 2) {
		linkLatency += 25;
	} else if (i % 5 == 3) {
		linkLatency += 40;
	} else if (i % 5 == 4) {
		linkLatency += 80;
	}
        NS_LOG_INFO ("Generate link latency: " << linkLatency);
        p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(linkLatency)));
        NodeContainer nodeContainer = NodeContainer (senders.Get (i), switches.Get (0));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
        QueueDiscContainer queuediscDataset = tc.Install (netDeviceContainer);
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork ();
    }

    p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(5)));
    NodeContainer switchToRecvNodeContainer = NodeContainer (switches.Get (0), receivers.Get (0));
    NetDeviceContainer switchToRecvNetDeviceContainer = p2p.Install (switchToRecvNodeContainer);
    QueueDiscContainer switchToRecvQueueDiscContainer = tc.Install (switchToRecvNetDeviceContainer);
    Ipv4InterfaceContainer switchToRecvIpv4Container = ipv4.Assign (switchToRecvNetDeviceContainer);


    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    NS_LOG_INFO ("Initialize CDF table");
    struct cdf_table* cdfTable = new cdf_table ();
    init_cdf (cdfTable);
    load_cdf (cdfTable, cdfFileName.c_str ());

    NS_LOG_INFO ("Calculating request rate");
    double requestRate = load * 10e9 / (8 * avg_cdf (cdfTable)) / numOfSenders;
    NS_LOG_INFO ("Average request rate: " << requestRate << " per second per sender");

    NS_LOG_INFO ("Initialize random seed: " << randomSeed);
    if (randomSeed == 0)
    {
        srand ((unsigned)time (NULL));
    }
    else
    {
        srand (randomSeed);
    }

    NS_LOG_INFO ("Install background application");

    uint16_t basePort = 8080;

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        uint32_t totalFlow = 0;
        double startTime = 0.0 + poission_gen_interval (requestRate);
        while (startTime < endTime && totalFlow < (flowNum / numOfSenders))
        {
            uint32_t flowSize = gen_random_cdf (cdfTable);
            BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
            source.SetAttribute ("MaxBytes", UintegerValue (flowSize));
            source.SetAttribute ("SendSize", UintegerValue (1400));
            ApplicationContainer sourceApps = source.Install (senders.Get (i));
            sourceApps.Start (Seconds (startTime));
            sourceApps.Stop (Seconds (simEndTime));

            PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort));
            ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
            sinkApp.Start (Seconds (0.0));
            sinkApp.Stop (Seconds (simEndTime));

            ++totalFlow;
            ++basePort;
            startTime += poission_gen_interval (requestRate);
        }
    }

    NS_LOG_INFO ("Install incast application");

    double incast_period = endTime / 10;

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        double startTime = 0.0 + incast_period;
        while (startTime < endTime)
        {
            BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
            source.SetAttribute ("MaxBytes", UintegerValue (rand_range (FLOW_SIZE_MIN, FLOW_SIZE_MAX)));
            source.SetAttribute ("SendSize", UintegerValue (1400));
            ApplicationContainer sourceApps = source.Install (senders.Get (i));
            sourceApps.Start (Seconds (startTime));
            sourceApps.Stop (Seconds (simEndTime));

            PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort));
            ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
            sinkApp.Start (Seconds (0.0));
            sinkApp.Stop (Seconds (simEndTime));

            ++basePort;
            startTime += incast_period;
        }
    }


    NS_LOG_INFO ("Start Tracing System");

    queuediscDataset.SetTitle ("Queue");
    queuediscDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    Simulator::ScheduleNow (&CheckQueueDiscSize, switchToRecvQueueDiscContainer.Get (0));

    NS_LOG_INFO ("Enabling Flow Monitor");
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    NS_LOG_INFO ("Run Simulations");

    Simulator::Stop (Seconds (simEndTime));
    Simulator::Run ();

    flowMonitor->SerializeToXmlFile(GetFormatedStr (id, "Flow_Monitor", "xml", aqm), true, true);

    Simulator::Destroy ();

    DoGnuPlot (id, aqm);

    return 0;
}
