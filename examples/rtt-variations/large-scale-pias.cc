#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

#include <vector>
#include <map>
#include <utility>
#include <set>

// The CDF in TrafficGenerator
extern "C"
{
#include "cdf.h"
}

#define LINK_CAPACITY_BASE    1000000000          // 1Gbps
#define BUFFER_SIZE 250                           // 250 packets

// The flow port range, each flow will be assigned a random port number within this range

static uint16_t PORT = 1000;

#define PACKET_SIZE 1400

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LargeScalePias");

enum AQM {
  TCN,
  ECNSharp
};

// Acknowledged to https://github.com/HKUST-SING/TrafficGenerator/blob/master/src/common/common.c
double poission_gen_interval(double avg_rate)
{
  if (avg_rate > 0)
    return -logf(1.0 - (double)rand() / RAND_MAX) / avg_rate;
  else
    return 0;
}

template<typename T>
T rand_range (T min, T max)
{
  return min + ((double)max - min) * rand () / RAND_MAX;
}

void install_applications (int fromLeafId, NodeContainer servers, double requestRate, struct cdf_table *cdfTable,
                           long &flowCount, long &totalFlowSize, int SERVER_COUNT, int LEAF_COUNT, double START_TIME, double END_TIME, double FLOW_LAUNCH_END_TIME)
{
  NS_LOG_INFO ("Install applications:");
  for (int i = 0; i < SERVER_COUNT; i++)
    {
      int fromServerIndex = fromLeafId * SERVER_COUNT + i;

      double startTime = START_TIME + poission_gen_interval (requestRate);
      while (startTime < FLOW_LAUNCH_END_TIME)
        {
          flowCount ++;
          uint16_t port = PORT++;

          int destServerIndex = fromServerIndex;
          while (destServerIndex >= fromLeafId * SERVER_COUNT && destServerIndex < fromLeafId * SERVER_COUNT + SERVER_COUNT)
            {
              destServerIndex = rand_range (0, SERVER_COUNT * LEAF_COUNT);
            }

          Ptr<Node> destServer = servers.Get (destServerIndex);
          Ptr<Ipv4> ipv4 = destServer->GetObject<Ipv4> ();
          Ipv4InterfaceAddress destInterface = ipv4->GetAddress (1,0);
          Ipv4Address destAddress = destInterface.GetLocal ();

          BulkSendPiasHelper source ("ns3::TcpSocketFactory", InetSocketAddress (destAddress, port));
          uint32_t flowSize = gen_random_cdf (cdfTable);
          uint32_t deplayClass = rand() % 5;

          totalFlowSize += flowSize;

          source.SetAttribute ("PiasThreshold", UintegerValue (PACKET_SIZE * 100));
          source.SetAttribute ("SendSize", UintegerValue (PACKET_SIZE));
          source.SetAttribute ("MaxBytes", UintegerValue(flowSize));
          source.SetAttribute ("DelayClass", UintegerValue (deplayClass));

          // Install apps
          ApplicationContainer sourceApp = source.Install (servers.Get (fromServerIndex));
          sourceApp.Start (Seconds (startTime));
          sourceApp.Stop (Seconds (END_TIME));

          // Install packet sinks
          PacketSinkHelper sink ("ns3::TcpSocketFactory",
                                 InetSocketAddress (Ipv4Address::GetAny (), port));
          ApplicationContainer sinkApp = sink.Install (servers. Get (destServerIndex));
          sinkApp.Start (Seconds (START_TIME));
          sinkApp.Stop (Seconds (END_TIME));

          startTime += poission_gen_interval (requestRate);
        }
    }
}

int main (int argc, char *argv[])
{
#if 1
  LogComponentEnable ("LargeScalePias", LOG_LEVEL_INFO);
#endif

  // Command line parameters parsing
  std::string id = "undefined";
  unsigned randomSeed = 0;
  std::string cdfFileName = "examples/rtt-variations/DCTCP_CDF.txt";
  double load = 0.0;
  std::string transportProt = "DcTcp";

  std::string aqmStr = "ECNSharp";

  // The simulation starting and ending time
  double START_TIME = 0.0;
  double END_TIME = 0.5;

  double FLOW_LAUNCH_END_TIME = 0.2;

  uint32_t linkLatency = 10;

  int SERVER_COUNT = 8;
  int SPINE_COUNT = 4;
  int LEAF_COUNT = 4;
  int LINK_COUNT = 1;

  uint64_t spineLeafCapacity = 10;
  uint64_t leafServerCapacity = 10;

  uint32_t TCNThreshold = 80;

  uint32_t ECNSharpInterval = 150;
  uint32_t ECNSharpTarget = 10;
  uint32_t ECNSharpMarkingThreshold = 80;

  CommandLine cmd;
  cmd.AddValue ("ID", "Running ID", id);
  cmd.AddValue ("StartTime", "Start time of the simulation", START_TIME);
  cmd.AddValue ("EndTime", "End time of the simulation", END_TIME);
  cmd.AddValue ("FlowLaunchEndTime", "End time of the flow launch period", FLOW_LAUNCH_END_TIME);
  cmd.AddValue ("randomSeed", "Random seed, 0 for random generated", randomSeed);
  cmd.AddValue ("cdfFileName", "File name for flow distribution", cdfFileName);
  cmd.AddValue ("load", "Load of the network, 0.0 - 1.0", load);
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
  cmd.AddValue ("linkLatency", "Link latency, should be in MicroSeconds", linkLatency);

  cmd.AddValue ("serverCount", "The Server count", SERVER_COUNT);
  cmd.AddValue ("spineCount", "The Spine count", SPINE_COUNT);
  cmd.AddValue ("leafCount", "The Leaf count", LEAF_COUNT);
  cmd.AddValue ("linkCount", "The Link count", LINK_COUNT);

  cmd.AddValue ("spineLeafCapacity", "Spine <-> Leaf capacity in Gbps", spineLeafCapacity);
  cmd.AddValue ("leafServerCapacity", "Leaf <-> Server capacity in Gbps", leafServerCapacity);

  cmd.AddValue ("AQM", "AQM to use: TCN or ECNSharp", aqmStr);

  cmd.AddValue ("TCNThreshold", "The threshold for TCN", TCNThreshold);

  cmd.AddValue ("ECNShaprInterval", "The persistent interval for ECNSharp", ECNSharpInterval);
  cmd.AddValue ("ECNSharpTarget", "The persistent target for ECNShapr", ECNSharpTarget);
  cmd.AddValue ("ECNShaprMarkingThreshold", "The instantaneous marking threshold for ECNSharp", ECNSharpMarkingThreshold);


  cmd.Parse (argc, argv);

  uint64_t SPINE_LEAF_CAPACITY = spineLeafCapacity * LINK_CAPACITY_BASE;
  uint64_t LEAF_SERVER_CAPACITY = leafServerCapacity * LINK_CAPACITY_BASE;
  Time LINK_LATENCY = MicroSeconds (linkLatency);

  if (load <= 0.0 || load >= 1.0)
    {
      NS_LOG_ERROR ("The network load should within 0.0 and 1.0");
      return 0;
    }

  AQM aqm;
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

  if (transportProt.compare ("DcTcp") == 0)
    {
      NS_LOG_INFO ("Enabling DcTcp");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));

      // TCN Configuration
      Config::SetDefault ("ns3::TCNQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
      Config::SetDefault ("ns3::TCNQueueDisc::MaxPackets", UintegerValue (BUFFER_SIZE));
      Config::SetDefault ("ns3::TCNQueueDisc::Threshold", TimeValue (MicroSeconds (TCNThreshold)));

      // ECN Sharp Configuration
      Config::SetDefault ("ns3::ECNSharpQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
      Config::SetDefault ("ns3::ECNSharpQueueDisc::MaxPackets", UintegerValue (BUFFER_SIZE));
      Config::SetDefault ("ns3::ECNSharpQueueDisc::InstantaneousMarkingThreshold", TimeValue (MicroSeconds (ECNSharpMarkingThreshold)));
      Config::SetDefault ("ns3::ECNSharpQueueDisc::PersistentMarkingTarget", TimeValue (MicroSeconds (ECNSharpTarget)));
      Config::SetDefault ("ns3::ECNSharpQueueDisc::PersistentMarkingInterval", TimeValue (MicroSeconds (ECNSharpInterval)));
    }

  NS_LOG_INFO ("Config parameters");
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (5)));

  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (5)));
  Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (80)));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

  Config::SetDefault ("ns3::Ipv4GlobalRouting::PerflowEcmpRouting", BooleanValue(true));

  NodeContainer spines;
  spines.Create (SPINE_COUNT);
  NodeContainer leaves;
  leaves.Create (LEAF_COUNT);
  NodeContainer servers;
  servers.Create (SERVER_COUNT * LEAF_COUNT);

  NS_LOG_INFO ("Install Internet stacks");
  InternetStackHelper internet;
  Ipv4GlobalRoutingHelper globalRoutingHelper;

  internet.SetRoutingHelper (globalRoutingHelper);


  internet.Install (servers);
  internet.Install (spines);
  internet.Install (leaves);

  NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper p2p;
  Ipv4AddressHelper ipv4;

  NS_LOG_INFO ("Configuring servers");
  // Setting servers
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LEAF_SERVER_CAPACITY)));
  p2p.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (10));

  ipv4.SetBase ("10.1.0.0", "255.255.255.0");

  for (int i = 0; i < LEAF_COUNT; i++)
    {
      ipv4.NewNetwork ();

      for (int j = 0; j < SERVER_COUNT; j++)
        {
          int serverIndex = i * SERVER_COUNT + j;
          NodeContainer nodeContainer = NodeContainer (leaves.Get (i), servers.Get (serverIndex));
          NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);

          //TODO We should change this, at endhost we are not going to mark ECN but add delay using delay queue disc

          Ptr<DelayQueueDisc> delayQueueDisc = CreateObject<DelayQueueDisc> ();
          Ptr<Ipv4SimpleDelayFilter> filter = CreateObject<Ipv4SimpleDelayFilter> ();

          delayQueueDisc->AddPacketFilter (filter);

          delayQueueDisc->AddDelayClass (0, MicroSeconds (1));
          delayQueueDisc->AddDelayClass (1, MicroSeconds (20));
          delayQueueDisc->AddDelayClass (2, MicroSeconds (50));
          delayQueueDisc->AddDelayClass (3, MicroSeconds (80));
          delayQueueDisc->AddDelayClass (4, MicroSeconds (160));

          ObjectFactory switchSideQueueFactory;

          if (aqm == TCN)
            {
              switchSideQueueFactory.SetTypeId ("ns3::TCNQueueDisc");
            }
          else
            {
              switchSideQueueFactory.SetTypeId ("ns3::ECNSharpQueueDisc");
            }
          Ptr<QueueDisc> switchSideQueueDisc = switchSideQueueFactory.Create<QueueDisc> ();

          Ptr<NetDevice> netDevice0 = netDeviceContainer.Get (0);
          Ptr<TrafficControlLayer> tcl0 = netDevice0->GetNode ()->GetObject<TrafficControlLayer> ();
          delayQueueDisc->SetNetDevice (netDevice0);
          tcl0->SetRootQueueDiscOnDevice (netDevice0, delayQueueDisc);

          Ptr<NetDevice> netDevice1 = netDeviceContainer.Get (1);
          Ptr<TrafficControlLayer> tcl1 = netDevice1->GetNode ()->GetObject<TrafficControlLayer> ();
          switchSideQueueDisc->SetNetDevice (netDevice1);
          tcl1->SetRootQueueDiscOnDevice (netDevice1, switchSideQueueDisc);

          Ipv4InterfaceContainer interfaceContainer = ipv4.Assign (netDeviceContainer);

          NS_LOG_INFO ("Leaf - " << i << " is connected to Server - " << j << " with address "
                       << interfaceContainer.GetAddress(0) << " <-> " << interfaceContainer.GetAddress (1)
                       << " with port " << netDeviceContainer.Get (0)->GetIfIndex () << " <-> " << netDeviceContainer.Get (1)->GetIfIndex ());
        }
    }

  NS_LOG_INFO ("Configuring switches");
  // Setting up switches
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (SPINE_LEAF_CAPACITY)));

  for (int i = 0; i < LEAF_COUNT; i++)
    {
      for (int j = 0; j < SPINE_COUNT; j++)
        {

          for (int l = 0; l < LINK_COUNT; l++)
            {
              ipv4.NewNetwork ();

              NodeContainer nodeContainer = NodeContainer (leaves.Get (i), spines.Get (j));
              NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);

              Ptr<SPQueueDisc> leafQueueDisc = CreateObject<SPQueueDisc> ();
              Ptr<SPQueueDisc> spineQueueDisc = CreateObject<SPQueueDisc> ();
              
              ObjectFactory innerQueueFactory;
              if (aqm == TCN)
              {
                innerQueueFactory.SetTypeId ("ns3::TCNQueueDisc");
              }
              else
              {
                innerQueueFactory.SetTypeId ("ns3::ECNSharpQueueDisc");
              }

              Ptr<QueueDisc> leafQueueDisc0 = innerQueueFactory.Create<QueueDisc> ();
              Ptr<QueueDisc> leafQueueDisc1 = innerQueueFactory.Create<QueueDisc> ();
              Ptr<QueueDisc> leafQueueDisc2 = innerQueueFactory.Create<QueueDisc> ();
              Ptr<QueueDisc> leafQueueDisc3 = innerQueueFactory.Create<QueueDisc> ();

              Ptr<Ipv4SimplePiasFilter> leafFilter = CreateObject<Ipv4SimplePiasFilter> ();
              leafQueueDisc->AddPacketFilter (leafFilter);
              leafQueueDisc->AddSPClass(leafQueueDisc0, 0, 10);
              leafQueueDisc->AddSPClass(leafQueueDisc1, 1, 8);
              leafQueueDisc->AddSPClass(leafQueueDisc2, 2, 6);
              leafQueueDisc->AddSPClass(leafQueueDisc3, 3, 0);

              Ptr<QueueDisc> spineQueueDisc0 = innerQueueFactory.Create<QueueDisc> ();
              Ptr<QueueDisc> spineQueueDisc1 = innerQueueFactory.Create<QueueDisc> ();
              Ptr<QueueDisc> spineQueueDisc2 = innerQueueFactory.Create<QueueDisc> ();
              Ptr<QueueDisc> spineQueueDisc3 = innerQueueFactory.Create<QueueDisc> ();

              Ptr<Ipv4SimplePiasFilter> spineFilter = CreateObject<Ipv4SimplePiasFilter> ();
              spineQueueDisc->AddPacketFilter (spineFilter);
              spineQueueDisc->AddSPClass(spineQueueDisc0, 0, 10);
              spineQueueDisc->AddSPClass(spineQueueDisc1, 1, 8);
              spineQueueDisc->AddSPClass(spineQueueDisc2, 2, 6);
              spineQueueDisc->AddSPClass(spineQueueDisc3, 3, 0);

              Ptr<NetDevice> netDevice0 = netDeviceContainer.Get (0);
              Ptr<TrafficControlLayer> tcl0 = netDevice0->GetNode ()->GetObject<TrafficControlLayer> ();
              leafQueueDisc->SetNetDevice (netDevice0);
              tcl0->SetRootQueueDiscOnDevice (netDevice0, leafQueueDisc);


              Ptr<NetDevice> netDevice1 = netDeviceContainer.Get (1);
              Ptr<TrafficControlLayer> tcl1 = netDevice1->GetNode ()->GetObject<TrafficControlLayer> ();
              spineQueueDisc->SetNetDevice (netDevice1);
              tcl1->SetRootQueueDiscOnDevice (netDevice1, spineQueueDisc);

              TrafficControlHelper tc;
              tc.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (BUFFER_SIZE));

              Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
              NS_LOG_INFO ("Leaf - " << i << " is connected to Spine - " << j << " with address "
                           << ipv4InterfaceContainer.GetAddress(0) << " <-> " << ipv4InterfaceContainer.GetAddress (1)
                           << " with port " << netDeviceContainer.Get (0)->GetIfIndex () << " <-> " << netDeviceContainer.Get (1)->GetIfIndex ()
                           << " with data rate " << spineLeafCapacity);

            }
        }
    }

  NS_LOG_INFO ("Populate global routing tables");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  double oversubRatio = static_cast<double>(SERVER_COUNT * LEAF_SERVER_CAPACITY) / (SPINE_LEAF_CAPACITY * SPINE_COUNT * LINK_COUNT);
  NS_LOG_INFO ("Over-subscription ratio: " << oversubRatio);

  NS_LOG_INFO ("Initialize CDF table");
  struct cdf_table* cdfTable = new cdf_table ();
  init_cdf (cdfTable);
  load_cdf (cdfTable, cdfFileName.c_str ());

  NS_LOG_INFO ("Calculating request rate");
  double requestRate = load * LEAF_SERVER_CAPACITY * SERVER_COUNT / oversubRatio / (8 * avg_cdf (cdfTable)) / SERVER_COUNT;
  NS_LOG_INFO ("Average request rate: " << requestRate << " per second");

  NS_LOG_INFO ("Initialize random seed: " << randomSeed);
  if (randomSeed == 0)
    {
      srand ((unsigned)time (NULL));
    }
  else
    {
      srand (randomSeed);
    }

  NS_LOG_INFO ("Create applications");

  long flowCount = 0;
  long totalFlowSize = 0;

  for (int fromLeafId = 0; fromLeafId < LEAF_COUNT; fromLeafId ++)
    {
      install_applications(fromLeafId, servers, requestRate, cdfTable, flowCount, totalFlowSize, SERVER_COUNT, LEAF_COUNT, START_TIME, END_TIME, FLOW_LAUNCH_END_TIME);
    }

  NS_LOG_INFO ("Total flow: " << flowCount);

  NS_LOG_INFO ("Actual average flow size: " << static_cast<double> (totalFlowSize) / flowCount);

  NS_LOG_INFO ("Enabling flow monitor");

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();


  flowMonitor->CheckForLostPackets ();

  std::stringstream flowMonitorFilename;

  flowMonitorFilename << "Large_Scale_" <<id << "_" << LEAF_COUNT << "X" << SPINE_COUNT << "_" << aqmStr << "_"  << transportProt << "_" << load << ".xml";


  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME));
  Simulator::Run ();

  flowMonitor->SerializeToXmlFile(flowMonitorFilename.str (), true, true);

  Simulator::Destroy ();
  free_cdf (cdfTable);
  NS_LOG_INFO ("Stop simulation");
}
