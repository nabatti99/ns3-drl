#include "ns3-env.h"

#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/my-custom-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellDrlSimulation");

static void
traceThroughput(Ptr<OutputStreamWrapper> stream,
                Ptr<DRLQueueDisc> queueDisc,
                DRLQueueId queueId,
                Ptr<QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),QueueID,Throughput(KB)" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<DRLQueueId, std::queue<uint64_t>> bitRateRecords{0};
    static uint64_t totalBitRate = 0;

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    if (elapsedTime < Time("200ms"))
    {
        uint64_t bitRate = queueDisc->m_bandwidth.GetBitRate();
        bitRateRecords[queueId].push(bitRate);
        totalBitRate += bitRate;
        return;
    }

    uint64_t avgBitRate = totalBitRate / bitRateRecords[queueId].size();
    uint64_t bandwidth = avgBitRate / 1e3; // Convert to Kb

    *stream->GetStream() << currentTime.GetSeconds() << "," << DRL_QUEUE_IDS[queueId] << ","
                         << bandwidth << std::endl;

    bitRateRecords.clear();
    totalBitRate = 0;

    uint64_t bitRate = queueDisc->m_bandwidth.GetBitRate();
    bitRateRecords[queueId].push(bitRate);
    totalBitRate += bitRate;

    lastTime = currentTime;
}

static void
traceQueueSize(Ptr<OutputStreamWrapper> stream,
               Ptr<DRLQueueDisc> queueDisc,
               DRLQueueId queueId,
               Ptr<QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),QueueID,QueueSize" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    if (elapsedTime < Time("200ms"))
    {
        return;
    }

    uint64_t queueSize = queueDisc->GetInternalQueue(queueId)->GetNPackets();

    *stream->GetStream() << currentTime.GetSeconds() << "," << DRL_QUEUE_IDS[queueId] << ","
                         << queueSize << std::endl;

    lastTime = currentTime;
}

// Log current progress
static void
logCurrentProgress(uint32_t stopTime)
{
    NS_LOG_UNCOND("Current simulation time: " << Simulator::Now().GetMilliSeconds() << "ms");

    static uint8_t lastCheckpoint = 0;

    double progress = Simulator::Now().GetSeconds() / stopTime * 100.0;

    if (uint8_t(progress) % 5 == 0 && lastCheckpoint != uint8_t(progress))
    {
        lastCheckpoint = uint8_t(progress);
        NS_LOG_UNCOND("✈️ Simulation progress: " << progress << "%");
    }

    Simulator::Schedule(MilliSeconds(100), &logCurrentProgress, stopTime);
}

int
main(int argc, char* argv[])
{
    LogComponentEnableAll(LOG_PREFIX_ALL);
    LogComponentEnable("OpenGymEnv", LOG_LEVEL_LOGIC);
    LogComponentEnable("DumbbellDrlSimulation", LOG_LEVEL_LOGIC);
    // LogComponentEnable("PacketSink", LOG_LEVEL_LOGIC);
    // LogComponentEnable("RealtimeApplication", LOG_LEVEL_LOGIC);

    static const uint32_t NUM_ROUTERS = 2;
    static std::string BOTTLENECK_BW = "100Mbps";
    static const uint32_t BOTTLENECK_MTU = 2000;
    static const std::string BOTTLENECK_DELAY = "1us";

    static const uint32_t NUM_REALTIME_FLOWS = 1;
    static const uint32_t NUM_NON_REALTIME_FLOWS = 0;
    static const uint32_t NUM_OTHER_FLOWS = 0;

    static const uint32_t NUM_LEAFS = NUM_REALTIME_FLOWS + NUM_NON_REALTIME_FLOWS + NUM_OTHER_FLOWS;

    Ptr<Ns3Env> ns3Env = CreateObject<Ns3Env>();
    ns3Env->Notify();

    try
    {
        NS_LOG_UNCOND("===Get environment inputs...===");
        // CommandLine cmd;
        // cmd.AddValue("bandwidth", "Bandwidth of the bottleneck link", BOTTLENECK_BW);
        // cmd.Parse(argc, argv);

        // Calculate after user inputs
        const uint32_t REALTIME_FLOWS_START_INDEX = 0;
        NS_LOG_UNCOND("REALTIME_FLOWS_START_INDEX: " << REALTIME_FLOWS_START_INDEX);
        const uint32_t REALTIME_FLOWS_END_INDEX = REALTIME_FLOWS_START_INDEX + NUM_REALTIME_FLOWS;
        NS_LOG_UNCOND("REALTIME_FLOWS_END_INDEX: " << REALTIME_FLOWS_END_INDEX);

        const uint32_t NON_REALTIME_FLOWS_START_INDEX = REALTIME_FLOWS_END_INDEX;
        NS_LOG_UNCOND("NON_REALTIME_FLOWS_START_INDEX: " << NON_REALTIME_FLOWS_START_INDEX);
        const uint32_t NON_REALTIME_FLOWS_END_INDEX =
            NON_REALTIME_FLOWS_START_INDEX + NUM_NON_REALTIME_FLOWS;
        NS_LOG_UNCOND("NON_REALTIME_FLOWS_END_INDEX: " << NON_REALTIME_FLOWS_END_INDEX);

        const uint32_t OTHER_FLOWS_START_INDEX = NON_REALTIME_FLOWS_END_INDEX;
        NS_LOG_UNCOND("OTHER_FLOWS_START_INDEX: " << OTHER_FLOWS_START_INDEX);
        const uint32_t OTHER_FLOWS_END_INDEX = OTHER_FLOWS_START_INDEX + NUM_OTHER_FLOWS;
        NS_LOG_UNCOND("OTHER_FLOWS_END_INDEX: " << OTHER_FLOWS_END_INDEX);

        // Application types
        static std::pair<std::string, FlowType> LEFT_LEAF_APP_TYPES[NUM_LEAFS];
        for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX;
             ++i) // Realtime flows
        {
            LEFT_LEAF_APP_TYPES[i] = {"ns3::MyApplication", FlowType::REALTIME_FLOW};
        }
        for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX;
             ++i) // Non-realtime flows
        {
            LEFT_LEAF_APP_TYPES[i] = {"ns3::MyApplication", FlowType::NON_REALTIME_FLOW};
        }
        for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i) // Other flows
        {
            LEFT_LEAF_APP_TYPES[i] = {"ns3::MyApplication", FlowType::OTHER_FLOW};
        }

        // Packet size at the application layer.
        // Packet header size: 54B = P2P (2B) + IPv4 (20B) + TCP (32B = 20B Standard + 12B Option)
        static uint32_t PACKET_HEADER_SIZE = 54;
        static uint32_t LEFT_LEAF_APP_PACKET_SIZES[NUM_LEAFS];
        for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX;
             ++i) // Realtime flows
        {
            LEFT_LEAF_APP_PACKET_SIZES[i] = 64;
        }
        for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX;
             ++i) // Non-realtime flows
        {
            LEFT_LEAF_APP_PACKET_SIZES[i] = 1500;
        }
        for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i) // AR/VR flows
        {
            LEFT_LEAF_APP_PACKET_SIZES[i] = 256;
        }

        // Leaf bandwidths
        static std::string LEAF_BWS[NUM_LEAFS];
        for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX;
             ++i) // Realtime flows
        {
            LEAF_BWS[i] = "1Gbps";
        }
        for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX;
             ++i) // Non-realtime flows
        {
            LEAF_BWS[i] = "1Gbps";
        }
        for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i) // AR/VR flows
        {
            LEAF_BWS[i] = "1Gbps";
        }

        // Leaf MTUs
        static uint32_t LEAF_MTUS[NUM_LEAFS];
        for (int i = 0; i < NUM_LEAFS; ++i) // Realtime flows
        {
            LEAF_MTUS[i] = BOTTLENECK_MTU;
        }

        static std::string LEFT_LEAF_DELAYS[NUM_LEAFS];
        static std::string RIGHT_LEAF_DELAYS[NUM_LEAFS];
        for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX;
             ++i) // Realtime flows 1
        {
            LEFT_LEAF_DELAYS[i] = "1ms";
            RIGHT_LEAF_DELAYS[i] = "1us";
        }
        for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX;
             ++i) // Non-realtime flows
        {
            LEFT_LEAF_DELAYS[i] = "10ms";
            RIGHT_LEAF_DELAYS[i] = "1us";
        }
        for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i) // AR/VR flows
        {
            LEFT_LEAF_DELAYS[i] = "5ms";
            RIGHT_LEAF_DELAYS[i] = "1us";
        }

        static const std::string DEFAULT_QUEUE_TYPE = "ns3::DropTailQueue";
        static const std::string DEFAULT_QUEUE_SIZE = "10000p";

        // Packet sending rate at the application layer.
        // The sending rate need to compensate the header size to make sure the actual bandwidth is
        // what we set here.
        static DataRate LEFT_LEAF_APP_BWS[NUM_LEAFS];
        for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX;
             ++i) // Realtime flows
        {
            LEFT_LEAF_APP_BWS[i] = DataRate("1Mbps");
        }
        for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX;
             ++i) // Non-realtime flows
        {
            LEFT_LEAF_APP_BWS[i] = DataRate("20Mbps");
        }
        for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i)
        {
            LEFT_LEAF_APP_BWS[i] = DataRate("5Mbps");
        }

        // Interval working time (second).
        static std::tuple<double, double, double> LEFT_LEAF_APP_INTERVAL_TIMES[NUM_LEAFS];
        for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX;
             ++i) // Realtime flows
        {
            LEFT_LEAF_APP_INTERVAL_TIMES[i] = {1.0, 0, 0};
        }
        for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX;
             ++i) // Non-realtime flows
        {
            LEFT_LEAF_APP_INTERVAL_TIMES[i] = {1.0, 0, 0};
        }
        for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i)
        {
            LEFT_LEAF_APP_INTERVAL_TIMES[i] = {1.0, 0, 0};
        }

        static const uint32_t APP_STOP_TIME = 10; // seconds
        static const uint32_t STOP_TIME = 10;     // seconds

        static const std::filesystem::path CWD = std::filesystem::current_path();
        static const std::string OUTPUT_FOLDER = BOTTLENECK_BW;
        static const std::filesystem::path OUTPUT_DIR =
            CWD / "visualization" / "output" / "drl-queue" / OUTPUT_FOLDER;

        NS_LOG_UNCOND("⭐️ Starting simulation...");

        NS_LOG_UNCOND("===Creating nodes...===");
        NodeContainer routers;
        routers.Create(NUM_ROUTERS);

        NodeContainer leftLeafs;
        leftLeafs.Create(NUM_LEAFS);

        NodeContainer rightLeafs;
        rightLeafs.Create(NUM_LEAFS);

        NS_LOG_UNCOND("===Creating links...===");
        PointToPointHelper p2pBottleneck;
        p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(BOTTLENECK_BW));
        p2pBottleneck.SetDeviceAttribute("Mtu", UintegerValue(BOTTLENECK_MTU));
        p2pBottleneck.SetChannelAttribute("Delay", StringValue(BOTTLENECK_DELAY));
        p2pBottleneck.SetQueue(DEFAULT_QUEUE_TYPE, "MaxSize", StringValue(DEFAULT_QUEUE_SIZE));

        PointToPointHelper p2pLeftLeafs[NUM_LEAFS];
        PointToPointHelper p2pRightLeafs[NUM_LEAFS];
        for (uint32_t i = 0; i < NUM_LEAFS; i++)
        {
            p2pLeftLeafs[i].SetDeviceAttribute("DataRate", StringValue(LEAF_BWS[i]));
            p2pLeftLeafs[i].SetDeviceAttribute("Mtu", UintegerValue(LEAF_MTUS[i]));
            p2pLeftLeafs[i].SetChannelAttribute("Delay", StringValue(LEFT_LEAF_DELAYS[i]));
            p2pLeftLeafs[i].SetQueue(DEFAULT_QUEUE_TYPE,
                                     "MaxSize",
                                     StringValue(DEFAULT_QUEUE_SIZE));

            p2pRightLeafs[i].SetDeviceAttribute("DataRate", StringValue(LEAF_BWS[i]));
            p2pRightLeafs[i].SetDeviceAttribute("Mtu", UintegerValue(LEAF_MTUS[i]));
            p2pRightLeafs[i].SetChannelAttribute("Delay", StringValue(RIGHT_LEAF_DELAYS[i]));
            p2pRightLeafs[i].SetQueue(DEFAULT_QUEUE_TYPE,
                                      "MaxSize",
                                      StringValue(DEFAULT_QUEUE_SIZE));
        }

        NS_LOG_UNCOND("===Creating network devices...===");
        NetDeviceContainer routersDevices = p2pBottleneck.Install(routers);

        NetDeviceContainer leftLeafDevices;
        NetDeviceContainer rightLeafDevices;
        NetDeviceContainer leftRouterDevices;
        NetDeviceContainer rightRouterDevices;

        for (uint32_t i = 0; i < NUM_LEAFS; i++)
        {
            NetDeviceContainer leftDevices =
                p2pLeftLeafs[i].Install(leftLeafs.Get(i), routers.Get(0));
            leftLeafDevices.Add(leftDevices.Get(0));
            leftRouterDevices.Add(leftDevices.Get(1));

            NetDeviceContainer rightDevices =
                p2pRightLeafs[i].Install(rightLeafs.Get(i), routers.Get(NUM_ROUTERS - 1));
            rightLeafDevices.Add(rightDevices.Get(0));
            rightRouterDevices.Add(rightDevices.Get(1));
        }

        NS_LOG_UNCOND("===Creating network stack...===");
        InternetStackHelper internet;
        internet.Install(routers);
        internet.Install(leftLeafs);
        internet.Install(rightLeafs);

        NS_LOG_UNCOND("===Config transport layer...===");
        // 2 MB (large enough) TCP buffers to prevent the applications from bottlenecking the exp
        Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(2 * 1e6));
        Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(2 * 1e6));
        // Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1000000));
        Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(BOTTLENECK_MTU));
        Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true));
        Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
        Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                           TypeIdValue(TypeId::LookupByName("ns3::TcpClassicRecovery")));

        NS_LOG_UNCOND("===Create transport layer...===");
        Ptr<TcpL4Protocol> tcpL4Left;
        Ptr<TcpL4Protocol> tcpL4Right;

        for (uint32_t i = 0; i < NUM_LEAFS; i++)
        {
            tcpL4Left = leftLeafs.Get(i)->GetObject<TcpL4Protocol>();
            tcpL4Left->SetAttribute("SocketType",
                                    TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));

            tcpL4Right = rightLeafs.Get(i)->GetObject<TcpL4Protocol>();
            tcpL4Right->SetAttribute("SocketType",
                                     TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
        }

        NS_LOG_UNCOND("===Create queuing mechanism for routers...===");
        TrafficControlHelper tcRouters;
        tcRouters.SetRootQueueDisc("ns3::DRLQueueDisc");

        QueueDiscContainer queueDiscContainer = tcRouters.Install(routersDevices.Get(0));
        Ptr<DRLQueueDisc> experimentQueueDisc =
            queueDiscContainer.Get(0)->GetObject<DRLQueueDisc>();

        NS_LOG_UNCOND("===Assigning IP addresses...===");
        Ipv4AddressHelper ipv4Left("10.0.0.0", "255.255.255.0");
        Ipv4AddressHelper ipv4Right("20.0.0.0", "255.255.255.0");
        Ipv4AddressHelper ipv4Router("100.0.0.0", "255.255.255.0");

        Ipv4InterfaceContainer routerInterfaces = ipv4Router.Assign(routersDevices);

        Ipv4InterfaceContainer leftLeafInterface;
        Ipv4InterfaceContainer leftRouterInterface;
        Ipv4InterfaceContainer rightLeafInterface;
        Ipv4InterfaceContainer rightRouterInterface;
        for (uint32_t i = 0; i < NUM_LEAFS; i++)
        {
            Ipv4InterfaceContainer leftInterface = ipv4Left.Assign(
                NetDeviceContainer(leftLeafDevices.Get(i), leftRouterDevices.Get(i)));
            leftLeafInterface.Add(leftInterface.Get(0));
            leftRouterInterface.Add(leftInterface.Get(1));
            ipv4Left.NewNetwork();
            // NS_LOG_UNCOND("Left Leaf " << i << ": " << leftLeafInterface.GetAddress(i));

            Ipv4InterfaceContainer rightInterface = ipv4Right.Assign(
                NetDeviceContainer(rightLeafDevices.Get(i), rightRouterDevices.Get(i)));
            rightLeafInterface.Add(rightInterface.Get(0));
            rightRouterInterface.Add(rightInterface.Get(1));
            ipv4Right.NewNetwork();
            // NS_LOG_UNCOND("Right Leaf " << i << ": " << rightLeafInterface.GetAddress(i));
        }

        NS_LOG_UNCOND("===Populate routing table...===");
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        NS_LOG_UNCOND("===Create receiver applications...===");
        static const uint32_t SINK_PORT = 9;
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), SINK_PORT));

        ApplicationContainer serverApps = sinkHelper.Install(rightLeafs);
        serverApps.Start(Seconds(0));
        serverApps.Stop(Seconds(APP_STOP_TIME));

        NS_LOG_UNCOND("===Create sender applications...===");
        ApplicationContainer clientApps;
        for (uint32_t i = 0; i < NUM_LEAFS; i++)
        {
            NS_LOG_UNCOND("Sender " << i << " -> Receiver " << i << " : "
                                    << leftLeafInterface.GetAddress(i) << " -> "
                                    << rightLeafInterface.GetAddress(i));

            ApplicationHelper applicationHelper(LEFT_LEAF_APP_TYPES[i].first);
            applicationHelper.SetAttribute("SourceId", UintegerValue(i));
            applicationHelper.SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
            applicationHelper.SetAttribute("FlowType",
                                           UintegerValue(LEFT_LEAF_APP_TYPES[i].second));
            applicationHelper.SetAttribute("DataRate", DataRateValue(LEFT_LEAF_APP_BWS[i]));
            applicationHelper.SetAttribute("PacketSize",
                                           UintegerValue(LEFT_LEAF_APP_PACKET_SIZES[i]));
            applicationHelper.SetAttribute(
                "OnTime",
                StringValue("ns3::ConstantRandomVariable[Constant=" +
                            std::to_string(std::get<0>(LEFT_LEAF_APP_INTERVAL_TIMES[i])) + "]"));
            applicationHelper.SetAttribute(
                "OffTime",
                StringValue("ns3::ConstantRandomVariable[Constant=" +
                            std::to_string(std::get<1>(LEFT_LEAF_APP_INTERVAL_TIMES[i])) + "]"));
            applicationHelper.SetAttribute(
                "Remote",
                AddressValue(InetSocketAddress(rightLeafInterface.GetAddress(i), SINK_PORT)));

            ApplicationContainer applications = applicationHelper.Install(leftLeafs.Get(i));

            // Adjust the application to compensate the packet header size
            applications.Get(0)->GetObject<MyApplication>()->AdjustPacketSize(PACKET_HEADER_SIZE);
            applications.Get(0)->GetObject<MyApplication>()->AdjustSendingRate(PACKET_HEADER_SIZE);

            applications.Start(Seconds(std::get<2>(LEFT_LEAF_APP_INTERVAL_TIMES[i])));
            applications.Stop(Seconds(APP_STOP_TIME));

            clientApps.Add(applications);
        }

        NS_LOG_UNCOND("===Config tracing...===");
        // PointToPointHelper p2pTracingHelper;
        // p2pTracingHelper.EnablePcap(std::string(OUTPUT_DIR / "bottleneck"),
        //                             routers.Get(0)->GetId(),
        //                             0,
        //                             true);

        // Tracing on experimental router & Write tracing files
        std::filesystem::create_directories(OUTPUT_DIR);
        AsciiTraceHelper ascii;

        // Throughput tracing
        Ptr<OutputStreamWrapper> throughputStream =
            ascii.CreateFileStream(OUTPUT_DIR / "throughput.csv");
        experimentQueueDisc->TraceConnectWithoutContext(
            "DRLQueueDequeue",
            MakeBoundCallback(&traceThroughput, throughputStream));

        // Queue size tracing
        Ptr<OutputStreamWrapper> queueSizeStream =
            ascii.CreateFileStream(OUTPUT_DIR / "queue-size.csv");
        experimentQueueDisc->TraceConnectWithoutContext(
            "DRLQueueEnqueue",
            MakeBoundCallback(&traceQueueSize, queueSizeStream));
        experimentQueueDisc->TraceConnectWithoutContext(
            "DRLQueueDequeue",
            MakeBoundCallback(&traceQueueSize, queueSizeStream));

        NS_LOG_UNCOND("===Starting simulation...===");
        Simulator::Schedule(Seconds(1), &logCurrentProgress, STOP_TIME);

        auto start = std::chrono::high_resolution_clock::now();

        Simulator::Stop(Seconds(STOP_TIME));
        Simulator::Run();
        Simulator::Destroy();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;

        NS_LOG_UNCOND("✅ Simulation finished -> Duration: " << duration.count() << "s");
        ns3Env->NotifySimulationEnd();
        return 0;
    }
    catch (const std::exception& e)
    {
        NS_LOG_UNCOND("❌ Simulation error: " << e.what());
        ns3Env->NotifySimulationEnd();
        return 1;
    }
}
