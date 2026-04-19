#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/my-custom-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <chrono>
#include <filesystem>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FifoSimulation");

// ===Global variables===

// ===Tracing functions===

// Config socket function
static void
configSocket(uint32_t segmentSize, Ptr<Socket> oldSocket, Ptr<Socket> newSocket)
{
    if (newSocket != nullptr)
    {
        newSocket->SetAttribute("SegmentSize", UintegerValue(segmentSize));
    }
}

// Add tags function
static void
addTagsForSource(Ptr<const Packet> packet)
{
    // Add Send Time tag if not already present
    SendTimeTag sendTimeTag;
    sendTimeTag.SetAttribute("SendTime", TimeValue(Simulator::Now()));
    packet->AddByteTag(sendTimeTag);
}

static void
addEnqueueTimeTagForSource(Ptr<const Packet> packet)
{
    // Add Enqueue Time tag
    EnqueueTimeTag enqueueTimeTag;
    enqueueTimeTag.SetAttribute("EnqueueTime", TimeValue(Simulator::Now()));
    packet->AddByteTag(enqueueTimeTag);
}

static void
addTagsForACK(uint32_t applicationId, Ptr<const Packet> packet)
{
    // Add Application ID tag
    ApplicationIdTag applicationIdTag;
    applicationIdTag.SetAttribute("ApplicationId", UintegerValue(applicationId));
    packet->AddByteTag(applicationIdTag);

    // Add Send Time tag
    SendTimeTag sendTimeTag;
    sendTimeTag.SetAttribute("SendTime", TimeValue(Simulator::Now()));
    packet->AddByteTag(sendTimeTag);
}

// Trace network metrics
static void
enqueueFlowTrace(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,NumberPackets" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, uint64_t> flowType_CumulativePackets{0};

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    if (elapsedTime < Time("200ms"))
    {
        flowType_CumulativePackets[flowTypeTag.Get()] += 1;
        return;
    }

    for (uint8_t i = FlowType::HANDSHAKE_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);
        uint64_t cumulativePackets = flowType_CumulativePackets[flowType];
        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << cumulativePackets << std::endl;
        flowType_CumulativePackets[flowType] = 0;
    }

    flowType_CumulativePackets.clear();

    flowType_CumulativePackets[flowTypeTag.Get()] += 1;
    lastTime = currentTime;
}

static void
dequeueFlowTrace(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,NumberPackets" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, uint64_t> flowType_CumulativePackets{0};

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    if (elapsedTime < Time("200ms"))
    {
        flowType_CumulativePackets[flowTypeTag.Get()] += 1;
        return;
    }

    for (uint8_t i = FlowType::HANDSHAKE_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);
        uint64_t cumulativePackets = flowType_CumulativePackets[flowType];
        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << cumulativePackets << std::endl;
        flowType_CumulativePackets[flowType] = 0;
    }

    flowType_CumulativePackets.clear();

    flowType_CumulativePackets[flowTypeTag.Get()] += 1;
    lastTime = currentTime;
}

static void
dropFlowTrace(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,NumberPackets" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, uint64_t> flowType_CumulativePackets{0};

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    if (elapsedTime < Time("200ms"))
    {
        flowType_CumulativePackets[flowTypeTag.Get()] += 1;
        return;
    }

    for (uint8_t i = FlowType::HANDSHAKE_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);
        uint64_t cumulativePackets = flowType_CumulativePackets[flowType];
        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << cumulativePackets << std::endl;
        flowType_CumulativePackets[flowType] = 0;
    }

    flowType_CumulativePackets.clear();

    flowType_CumulativePackets[flowTypeTag.Get()] += 1;
    lastTime = currentTime;
}

static void
throughputRouterTrace(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),Throughput(Kbps)" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static uint64_t cumulativeBits = 0;

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    Ptr<Packet> packet = item->GetPacket();
    uint64_t dequeuedBits = item->GetSize() * 8;

    if (elapsedTime < Time("200ms"))
    {
        cumulativeBits += dequeuedBits;
        return;
    }

    uint64_t throughput = cumulativeBits / elapsedTime.GetSeconds();
    double throughputKbps = double(throughput) / 1e3; // Convert to Kbps

    *stream->GetStream() << currentTime.GetSeconds() << "," << throughputKbps << std::endl;

    cumulativeBits = dequeuedBits;
    lastTime = currentTime;
}

static void
throughputFlowTrace(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,Throughput(Kbps)" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, uint32_t> flowType_CumulativeBits;

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    Ptr<Packet> packet = item->GetPacket();
    uint64_t dequeuedBits = item->GetSize() * 8;

    if (elapsedTime < Time("200ms"))
    {
        FlowTypeTag flowTypeTag;
        item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

        flowType_CumulativeBits[flowTypeTag.Get()] += dequeuedBits;
        return;
    }

    for (int i = FlowType::REALTIME_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);
        uint64_t cumulativeBits = flowType_CumulativeBits[flowType];
        double throughput = double(cumulativeBits) / elapsedTime.GetSeconds();
        double throughputKbps = throughput / 1e3; // Convert to Kbps

        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << throughputKbps << std::endl;
    }

    flowType_CumulativeBits.clear();

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    flowType_CumulativeBits[flowTypeTag.Get()] = dequeuedBits;
    lastTime = currentTime;
}

static void
sendingDataRateAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,DataRate(Kbps)" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, uint32_t> flowType_CumulativeBits;

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    uint64_t dequeuedBits = packet->GetSize() * 8;

    if (elapsedTime < Time("200ms"))
    {
        FlowTypeTag flowTypeTag;
        packet->FindFirstMatchingByteTag(flowTypeTag);

        flowType_CumulativeBits[flowTypeTag.Get()] += dequeuedBits;
        return;
    }

    for (int i = FlowType::REALTIME_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);
        uint64_t cumulativeBits = flowType_CumulativeBits[flowType];
        double throughput = double(cumulativeBits) / elapsedTime.GetSeconds();
        double throughputKbps = throughput / 1e3; // Convert to Kbps

        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << throughputKbps << std::endl;
    }

    flowType_CumulativeBits.clear();

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    flowType_CumulativeBits[flowTypeTag.Get()] = dequeuedBits;
    lastTime = currentTime;
}

static void
receivingDataRateServerTrace(Ptr<OutputStreamWrapper> stream,
                             Ptr<const Packet> packet,
                             const Address& _address)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,DataRate(Kbps)" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, uint32_t> flowType_CumulativeBits;

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    uint64_t dequeuedBits = packet->GetSize() * 8;

    if (elapsedTime < Time("200ms"))
    {
        FlowTypeTag flowTypeTag;
        packet->FindFirstMatchingByteTag(flowTypeTag);

        flowType_CumulativeBits[flowTypeTag.Get()] += dequeuedBits;
        return;
    }

    for (int i = FlowType::REALTIME_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);
        uint64_t cumulativeBits = flowType_CumulativeBits[flowType];
        double throughput = double(cumulativeBits) / elapsedTime.GetSeconds();
        double throughputKbps = throughput / 1e3; // Convert to Kbps

        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << throughputKbps << std::endl;
    }

    flowType_CumulativeBits.clear();

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    flowType_CumulativeBits[flowTypeTag.Get()] = dequeuedBits;
    lastTime = currentTime;
}

static void
packetsInQueueFlowTrace(Ptr<OutputStreamWrapper> stream,
                        bool isEnqueue,
                        Ptr<const QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,PacketsInQueue" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, uint32_t> flowType_numPackets{0};

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    if (elapsedTime < Time("200ms"))
    {
        FlowTypeTag flowTypeTag;
        item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

        if (isEnqueue)
        {
            flowType_numPackets[flowTypeTag.Get()] += 1;
        }
        else
        {
            if (flowType_numPackets[flowTypeTag.Get()] > 0)
            {
                flowType_numPackets[flowTypeTag.Get()] -= 1;
            }
        }
        return;
    }

    for (uint8_t i = FlowType::HANDSHAKE_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);
        uint32_t packetCount = flowType_numPackets[flowType];
        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << packetCount << std::endl;
    }

    flowType_numPackets.clear();

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    if (isEnqueue)
    {
        flowType_numPackets[flowTypeTag.Get()] = 1;
    }
    else
    {
        flowType_numPackets[flowTypeTag.Get()] = 0;
    }

    lastTime = currentTime;
}

static void
delayFlowTrace(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
    static bool isOk = [stream]() {
        *stream->GetStream() << "Time(s),FlowType,DelayTime(us)" << std::endl;
        return true;
    }();
    NS_ASSERT(isOk);

    static Time lastTime = Time(0);
    static default_map<FlowType, Time> flowType_MaxDelayTime{0};

    Time currentTime = Simulator::Now();
    Time elapsedTime = currentTime - lastTime;

    if (elapsedTime < Time("200ms"))
    {
        FlowTypeTag flowTypeTag;
        item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

        EnqueueTimeTag enqueueTimeTag;
        item->GetPacket()->FindFirstMatchingByteTag(enqueueTimeTag);

        Time delayTime = Simulator::Now() - enqueueTimeTag.Get();
        flowType_MaxDelayTime[flowTypeTag.Get()] =
            std::max(flowType_MaxDelayTime[flowTypeTag.Get()], delayTime);

        return;
    }

    for (uint8_t i = FlowType::HANDSHAKE_FLOW; i <= FlowType::OTHER_FLOW; ++i)
    {
        FlowType flowType = static_cast<FlowType>(i);

        uint32_t maxDelayTime = flowType_MaxDelayTime[flowType].GetMicroSeconds();

        *stream->GetStream() << currentTime.GetSeconds() << "," << FLOW_TYPES[flowType] << ","
                             << maxDelayTime << std::endl;
    }

    flowType_MaxDelayTime.clear();

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    EnqueueTimeTag enqueueTimeTag;
    item->GetPacket()->FindFirstMatchingByteTag(enqueueTimeTag);

    Time delayTime = Simulator::Now() - enqueueTimeTag.Get();
    flowType_MaxDelayTime[flowTypeTag.Get()] =
        std::max(flowType_MaxDelayTime[flowTypeTag.Get()], delayTime);

    lastTime = currentTime;
}

class ApplicationTrace
{
  public:
    Ptr<OutputStreamWrapper> qosStream;
    Ptr<OutputStreamWrapper> rttStream;
    Ptr<OutputStreamWrapper> delayStream;

    Ptr<OutputStreamWrapper> sentReceivedBytesStream;
    Ptr<OutputStreamWrapper> retransmissionBytesStream;

    std::string* APP_BWS;
    std::pair<std::string, FlowType>* APP_TYPES;
    std::tuple<double, double, double>* APP_INTERVAL_TIMES;
    std::string* LEFT_LEAF_DELAYS;
    std::string BOTTLENECK_DELAY;
    uint32_t TOTAL_APPLICATIONS;

    Time SAMPLE_INTERVAL = Time("10s");
    Time LOG_INTERVAL = Time("200ms");
    Time lastTime{0};

    default_map<uint32_t, DataRate> applicationId_QoS;
    default_map<uint32_t, Time> applicationId_RTT;

    void Initialize()
    {
        *qosStream->GetStream() << "Time(s),ApplicationId,FlowType,Throughput(Kbps),QoS(Kbps)"
                                << std::endl;
        *rttStream->GetStream() << "Time(s),ApplicationId,FlowType,ActualRTTForward(us),"
                                   "ActualRTTBackward(us),ActualRTT(us),EstimatedRTT(us)"
                                << std::endl;
        *delayStream->GetStream() << "Time(s),ApplicationId,FlowType,DelayTime(us)" << std::endl;

        *sentReceivedBytesStream->GetStream()
            << "Time(s),ApplicationId,FlowType,SentBytes,ReceivedBytes" << std::endl;
        *retransmissionBytesStream->GetStream()
            << "Time(s),ApplicationId,FlowType,RetransmittedBytes" << std::endl;

        for (uint32_t i = 0; i < TOTAL_APPLICATIONS; ++i)
        {
            // Calculate expected QoS
            uint64_t applicationQoS = DataRate(APP_BWS[i]).GetBitRate();

            double applicationStartTime = std::get<0>(APP_INTERVAL_TIMES[i]);
            double applicationStopTime = std::get<1>(APP_INTERVAL_TIMES[i]);
            double applicationUtilization =
                applicationStartTime / (applicationStartTime + applicationStopTime);

            applicationQoS = static_cast<uint64_t>(applicationQoS * applicationUtilization);
            applicationId_QoS[i] = DataRate(applicationQoS);

            // Calculate expected RTT
            Time expectedRoundTimeTrip = Time(ApplicationTrace::LEFT_LEAF_DELAYS[i]) * 2;

            applicationId_RTT[i] = expectedRoundTimeTrip;
        }
    }

    default_map<uint32_t, uint64_t> applicationId_SentBytes{0};
    default_map<uint32_t, uint64_t> applicationId_ReceivedBytes{0};

    static void ApplicationSentReceivedBytesTrace(ApplicationTrace* applicationTrace,
                                                  uint32_t applicationId,
                                                  bool isSender,
                                                  Ptr<const Packet> packet)
    {
        static Time lastTime = Time(0);

        Time currentTime = Simulator::Now();
        Time elapsedTime = currentTime - lastTime;

        if (elapsedTime < Time("200ms"))
        {
            if (isSender)
            {
                applicationTrace->applicationId_SentBytes[applicationId] += packet->GetSize();
            }
            else
            {
                applicationTrace->applicationId_ReceivedBytes[applicationId] += packet->GetSize();
            }
            return;
        }

        for (uint32_t i = 0; i < applicationTrace->TOTAL_APPLICATIONS; ++i)
        {
            *applicationTrace->sentReceivedBytesStream->GetStream()
                << currentTime.GetSeconds() << "," << i << ","
                << FLOW_TYPES[applicationTrace->APP_TYPES[i].second] << ","
                << applicationTrace->applicationId_SentBytes[i] << ","
                << applicationTrace->applicationId_ReceivedBytes[i] << std::endl;

            applicationTrace->applicationId_SentBytes[i] = 0;
            applicationTrace->applicationId_ReceivedBytes[i] = 0;
        }

        if (isSender)
        {
            applicationTrace->applicationId_SentBytes[applicationId] += packet->GetSize();
        }
        else
        {
            applicationTrace->applicationId_ReceivedBytes[applicationId] += packet->GetSize();
        }

        lastTime = currentTime;
    }

    default_map<uint32_t, std::vector<Time>> applicationId_ActualRTTForwardRecords{};
    default_map<uint32_t, Time> applicationId_ActualRTTForwardTotal{};
    default_map<uint32_t, std::vector<Time>> applicationId_ActualRTTBackwardRecords{};
    default_map<uint32_t, Time> applicationId_ActualRTTBackwardTotal{};

    static void RTTTrace(ApplicationTrace* applicationTrace,
                         uint32_t applicationId,
                         bool isForward,
                         Ptr<const Packet> packet)
    {
        static Time lastTime = Time(0);

        Time currentTime = Simulator::Now();
        Time elapsedTime = currentTime - lastTime;

        SendTimeTag sendTimeTag;
        bool hasTag = packet->FindFirstMatchingByteTag(sendTimeTag);
        if (!hasTag)
        {
            return;
        }

        Time sendTime = sendTimeTag.Get();
        Time rtt = currentTime - sendTime;

        if (elapsedTime < Time("200ms"))
        {
            if (isForward)
            {
                applicationTrace->applicationId_ActualRTTForwardRecords[applicationId].push_back(
                    rtt);
                applicationTrace->applicationId_ActualRTTForwardTotal[applicationId] += rtt;
            }
            else
            {
                applicationTrace->applicationId_ActualRTTBackwardRecords[applicationId].push_back(
                    rtt);
                applicationTrace->applicationId_ActualRTTBackwardTotal[applicationId] += rtt;
            }

            return;
        }

        for (uint32_t i = 0; i < applicationTrace->TOTAL_APPLICATIONS; ++i)
        {
            if (applicationTrace->applicationId_ActualRTTForwardRecords[i].empty() ||
                applicationTrace->applicationId_ActualRTTBackwardRecords[i].empty())
            {
                continue;
            }

            Time actualRTTForward =
                applicationTrace->applicationId_ActualRTTForwardTotal[i] /
                applicationTrace->applicationId_ActualRTTForwardRecords[i].size();
            Time actualRTTBackward =
                applicationTrace->applicationId_ActualRTTBackwardTotal[i] /
                applicationTrace->applicationId_ActualRTTBackwardRecords[i].size();

            Time actualRTT = actualRTTForward + actualRTTBackward;

            *applicationTrace->rttStream->GetStream()
                << currentTime.GetSeconds() << "," << i << ","
                << FLOW_TYPES[applicationTrace->APP_TYPES[i].second] << ","
                << actualRTTForward.GetMicroSeconds() << "," << actualRTTBackward.GetMicroSeconds()
                << "," << actualRTT.GetMicroSeconds() << ","
                << applicationTrace->applicationId_RTT[i].GetMicroSeconds() << std::endl;

            applicationTrace->applicationId_ActualRTTForwardRecords[i].clear();
            applicationTrace->applicationId_ActualRTTForwardTotal[i] = Time(0);
            applicationTrace->applicationId_ActualRTTBackwardRecords[i].clear();
            applicationTrace->applicationId_ActualRTTBackwardTotal[i] = Time(0);
        }

        lastTime = currentTime;
    }

    static void delayTrace(ApplicationTrace* applicationTrace, Ptr<const QueueDiscItem> item)
    {
        static Time lastTime = Time(0);
        static default_map<uint32_t, Time> applicationId_MaxDelayTime{0};

        Time currentTime = Simulator::Now();
        Time elapsedTime = currentTime - lastTime;

        ApplicationIdTag applicationIdTag;
        item->GetPacket()->FindFirstMatchingByteTag(applicationIdTag);

        EnqueueTimeTag enqueueTimeTag;
        item->GetPacket()->FindFirstMatchingByteTag(enqueueTimeTag);

        if (elapsedTime < Time("200ms"))
        {
            Time delayTime = Simulator::Now() - enqueueTimeTag.Get();
            applicationId_MaxDelayTime[applicationIdTag.Get()] =
                std::max(applicationId_MaxDelayTime[applicationIdTag.Get()], delayTime);

            return;
        }

        for (uint8_t i = 0; i <= applicationTrace->TOTAL_APPLICATIONS; ++i)
        {
            uint32_t applicationId = i;
            uint32_t maxDelayTime = applicationId_MaxDelayTime[applicationId].GetMicroSeconds();

            *applicationTrace->delayStream->GetStream()
                << currentTime.GetSeconds() << "," << applicationId << ","
                << FLOW_TYPES[applicationTrace->APP_TYPES[applicationId].second] << ","
                << maxDelayTime << std::endl;
        }
        applicationId_MaxDelayTime.clear();

        Time delayTime = Simulator::Now() - enqueueTimeTag.Get();
        applicationId_MaxDelayTime[applicationIdTag.Get()] =
            std::max(applicationId_MaxDelayTime[applicationIdTag.Get()], delayTime);

        lastTime = currentTime;
    }

    default_map<uint32_t, uint64_t> applicationId_RetransmissionBytes{0};

    static void ApplicationRetransmissionBytesTrace(ApplicationTrace* applicationTrace,
                                                    uint32_t applicationId,
                                                    ns3::Ptr<const ns3::Packet> packet,
                                                    const ns3::TcpHeader& tcpHeader,
                                                    const ns3::Address& sourceAddress,
                                                    const ns3::Address& destinationAddress,
                                                    ns3::Ptr<const ns3::TcpSocketBase> tcpSocket)
    {
        static Time lastTime = Time(0);

        Time currentTime = Simulator::Now();
        Time elapsedTime = currentTime - lastTime;

        if (elapsedTime < Time("200ms"))
        {
            applicationTrace->applicationId_RetransmissionBytes[applicationId] += packet->GetSize();
            return;
        }

        for (uint32_t i = 0; i < applicationTrace->TOTAL_APPLICATIONS; ++i)
        {
            *applicationTrace->retransmissionBytesStream->GetStream()
                << currentTime.GetSeconds() << "," << i << ","
                << FLOW_TYPES[applicationTrace->APP_TYPES[i].second] << ","
                << applicationTrace->applicationId_RetransmissionBytes[i] << std::endl;

            applicationTrace->applicationId_RetransmissionBytes[i] = 0;
        }

        applicationTrace->applicationId_RetransmissionBytes[applicationId] += packet->GetSize();

        lastTime = currentTime;
    }

    static void SocketTrace(ApplicationTrace* applicationTrace,
                            uint32_t applicationId,
                            Ptr<Socket> oldSocket,
                            Ptr<Socket> newSocket)
    {
        if (newSocket != nullptr)
        {
            newSocket->TraceConnectWithoutContext(
                "Retransmission",
                MakeBoundCallback(&ApplicationTrace::ApplicationRetransmissionBytesTrace,
                                  applicationTrace,
                                  applicationId));
        }
        else
        {
            oldSocket->TraceDisconnectWithoutContext(
                "Retransmission",
                MakeBoundCallback(&ApplicationTrace::ApplicationRetransmissionBytesTrace,
                                  applicationTrace,
                                  applicationId));
        }
    }

    default_map<uint32_t, uint64_t> applicationId_ServerReceivedCumulativeBytes{0};

    static void ApplicationThroughputTrace(ApplicationTrace* applicationTrace,
                                           uint32_t applicationId,
                                           Ptr<const Packet> packet,
                                           const Address& _address1,
                                           const Address& _address2)
    {
        static Time lastTime = Time(0);

        Time currentTime = Simulator::Now();
        Time elapsedTime = currentTime - lastTime;

        uint64_t packetSize = packet->GetSize() * 8; // In bits;

        if (elapsedTime < Time("200ms"))
        {
            applicationTrace->applicationId_ServerReceivedCumulativeBytes[applicationId] +=
                packetSize;
            return;
        }

        for (uint32_t i = 0; i < applicationTrace->TOTAL_APPLICATIONS; ++i)
        {
            uint64_t applicationThroughput =
                applicationTrace->applicationId_ServerReceivedCumulativeBytes[i] * 1e9 /
                elapsedTime.GetNanoSeconds();

            *applicationTrace->qosStream->GetStream()
                << currentTime.GetSeconds() << "," << i << ","
                << FLOW_TYPES[applicationTrace->APP_TYPES[i].second] << ","
                << applicationThroughput / 1000 << ","
                << applicationTrace->applicationId_QoS[i].GetBitRate() / 1000 << std::endl;

            applicationTrace->applicationId_ServerReceivedCumulativeBytes[i] = 0;
        }

        applicationTrace->applicationId_ServerReceivedCumulativeBytes[applicationId] += packetSize;

        lastTime = currentTime;
    }
};

// Log current progress
static void
logCurrentProgress(uint32_t stopTime)
{
    static uint8_t lastCheckpoint = 0;

    double progress = Simulator::Now().GetSeconds() / stopTime * 100.0;

    if (uint8_t(progress) % 5 == 0 && lastCheckpoint != uint8_t(progress))
    {
        lastCheckpoint = uint8_t(progress);
        NS_LOG_UNCOND("✈️ Simulation progress: " << progress << "%");
    }

    Simulator::Schedule(Seconds(5), &logCurrentProgress, stopTime);
}

// ===Main function===
int
main(int argc, char* argv[])
{
    LogComponentEnableAll(LOG_PREFIX_ALL);
    LogComponentEnable("FifoSimulation", LOG_LEVEL_LOGIC);
    // LogComponentEnable("PacketSink", LOG_LEVEL_LOGIC);
    // LogComponentEnable("RealtimeApplication", LOG_LEVEL_LOGIC);

    static const uint32_t NUM_ROUTERS = 2;
    static std::string BOTTLENECK_BW = "178Mbps";
    static const uint32_t BOTTLENECK_MTU = 3000;
    static const std::string BOTTLENECK_DELAY = "1us";

    static const uint32_t NUM_REALTIME_FLOWS = 5;
    static const uint32_t NUM_NON_REALTIME_FLOWS = 15;
    static const uint32_t NUM_OTHER_FLOWS = 10;

    static const uint32_t NUM_LEAFS = NUM_REALTIME_FLOWS + NUM_NON_REALTIME_FLOWS + NUM_OTHER_FLOWS;

    // Predefined AQM algorithm parameters
    static double ALPHA = 0.5;

    NS_LOG_UNCOND("===Get user inputs...===");
    CommandLine cmd;
    cmd.AddValue("bandwidth", "Bandwidth of the bottleneck link", BOTTLENECK_BW);
    cmd.AddValue("alpha",
                 "Alpha value for Buffer weight in the combined weight calculation",
                 ALPHA);
    cmd.Parse(argc, argv);

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
    for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX; ++i) // Realtime flows
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
    static uint32_t PACKET_HEADER_SIZE = 56; // PPP (2B) + Ethernet (14B) + IPv4 (20B) + TCP (20B)
    static uint32_t LEFT_LEAF_APP_PACKET_SIZES[NUM_LEAFS];
    for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX; ++i) // Realtime flows
    {
        LEFT_LEAF_APP_PACKET_SIZES[i] = 32 + PACKET_HEADER_SIZE;
    }
    for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX;
         ++i) // Non-realtime flows
    {
        LEFT_LEAF_APP_PACKET_SIZES[i] = 1500 + PACKET_HEADER_SIZE;
    }
    for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i) // AR/VR flows
    {
        LEFT_LEAF_APP_PACKET_SIZES[i] = 256 + PACKET_HEADER_SIZE;
    }

    // Leaf bandwidths
    static std::string LEAF_BWS[NUM_LEAFS];
    for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX; ++i) // Realtime flows
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
    for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX * 2;
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
    static std::string LEFT_LEAF_APP_BWS[NUM_LEAFS];
    for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX; ++i) // Realtime flows
    {
        LEFT_LEAF_APP_BWS[i] = "1Mbps";
    }
    for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX * 2;
         ++i) // Non-realtime flows
    {
        LEFT_LEAF_APP_BWS[i] = "20Mbps";
    }
    for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i) // AR/VR flows
    {
        LEFT_LEAF_APP_BWS[i] = "5Mbps";
    }

    // Interval working time (second).
    static std::tuple<double, double, double> LEFT_LEAF_APP_INTERVAL_TIMES[NUM_LEAFS];
    for (int i = REALTIME_FLOWS_START_INDEX; i < REALTIME_FLOWS_END_INDEX; ++i) // Realtime flows
    {
        LEFT_LEAF_APP_INTERVAL_TIMES[i] = {1.0, 0, 0};
    }
    for (int i = NON_REALTIME_FLOWS_START_INDEX; i < NON_REALTIME_FLOWS_END_INDEX * 2;
         ++i) // Non-realtime flows
    {
        LEFT_LEAF_APP_INTERVAL_TIMES[i] = {1.0, 0, 0};
    }
    for (int i = OTHER_FLOWS_START_INDEX; i < OTHER_FLOWS_END_INDEX; ++i) // AR/VR flows
    {
        LEFT_LEAF_APP_INTERVAL_TIMES[i] = {1.0, 0, 0};
    }

    static const uint32_t APP_STOP_TIME = 200; // seconds
    static const uint32_t STOP_TIME = 200;     // seconds

    static const std::filesystem::path CWD = std::filesystem::current_path();
    static const std::string OUTPUT_FOLDER = BOTTLENECK_BW;
    static const std::filesystem::path OUTPUT_DIR =
        CWD / "visualization" / "output" / "fifo" / OUTPUT_FOLDER;

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
        p2pLeftLeafs[i].SetQueue(DEFAULT_QUEUE_TYPE, "MaxSize", StringValue(DEFAULT_QUEUE_SIZE));

        p2pRightLeafs[i].SetDeviceAttribute("DataRate", StringValue(LEAF_BWS[i]));
        p2pRightLeafs[i].SetDeviceAttribute("Mtu", UintegerValue(LEAF_MTUS[i]));
        p2pRightLeafs[i].SetChannelAttribute("Delay", StringValue(RIGHT_LEAF_DELAYS[i]));
        p2pRightLeafs[i].SetQueue(DEFAULT_QUEUE_TYPE, "MaxSize", StringValue(DEFAULT_QUEUE_SIZE));
    }

    NS_LOG_UNCOND("===Creating network devices...===");
    NetDeviceContainer routersDevices = p2pBottleneck.Install(routers);

    NetDeviceContainer leftLeafDevices;
    NetDeviceContainer rightLeafDevices;
    NetDeviceContainer leftRouterDevices;
    NetDeviceContainer rightRouterDevices;

    for (uint32_t i = 0; i < NUM_LEAFS; i++)
    {
        NetDeviceContainer leftDevices = p2pLeftLeafs[i].Install(leftLeafs.Get(i), routers.Get(0));
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
    // Set the initial congestion window to 1000000 segments
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1000000));
    // Reset the default MSS ~500
    // IP MTU = IP header (20B-60B) + TCP header (20B-60B) + TCP MSS
    // Ethernet frame = Ethernet header (14B) + IP MTU + FCS (4B)
    // The additional header overhead for this instance is 20+20+14=54B, hence app_packet_size
    // <= 3000-54 = 2946
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(BOTTLENECK_MTU - 54));
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
        tcpL4Left->SetAttribute("SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));

        tcpL4Right = rightLeafs.Get(i)->GetObject<TcpL4Protocol>();
        tcpL4Right->SetAttribute("SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    }

    NS_LOG_UNCOND("===Create queuing mechanism for routers...===");
    TrafficControlHelper tcRouters;
    tcRouters.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("2500p"));

    QueueDiscContainer queueDiscContainer = tcRouters.Install(routersDevices.Get(0));
    Ptr<FifoQueueDisc> experimentQueueDisc = queueDiscContainer.Get(0)->GetObject<FifoQueueDisc>();

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
        Ipv4InterfaceContainer leftInterface =
            ipv4Left.Assign(NetDeviceContainer(leftLeafDevices.Get(i), leftRouterDevices.Get(i)));
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
        applicationHelper.SetAttribute("FlowType", UintegerValue(LEFT_LEAF_APP_TYPES[i].second));
        applicationHelper.SetAttribute("DataRate", DataRateValue(LEFT_LEAF_APP_BWS[i]));
        applicationHelper.SetAttribute("PacketSize",
                                       UintegerValue(LEFT_LEAF_APP_PACKET_SIZES[i] - 54));
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
        applications.Start(Seconds(std::get<2>(LEFT_LEAF_APP_INTERVAL_TIMES[i])));
        applications.Stop(Seconds(APP_STOP_TIME));

        clientApps.Add(applications);
    }

    for (uint32_t i = 0; i < clientApps.GetN(); i++)
    {
        Ptr<MyApplication> clientApplication = clientApps.Get(i)->GetObject<MyApplication>();
        clientApplication->TraceConnectWithoutContext(
            "SocketTrace",
            MakeBoundCallback(&configSocket, LEFT_LEAF_APP_PACKET_SIZES[i] - 54));
    }

    NS_LOG_UNCOND("===Config tracing...===");

    // Add tags before sending packet
    for (uint32_t i = 0; i < NUM_LEAFS; i++)
    {
        // Add tag for source packet
        leftLeafDevices.Get(i)->TraceConnectWithoutContext("PhyTxBegin",
                                                           MakeCallback(&addTagsForSource));

        // Add tag for ACK packet (Sink packet)
        rightLeafDevices.Get(i)->TraceConnectWithoutContext("PhyTxBegin",
                                                            MakeBoundCallback(&addTagsForACK, i));
    }

    // Receive source packet and add enqueue time tag for source packet
    for (uint32_t i = 0; i < NUM_LEAFS; i++)
    {
        leftRouterDevices.Get(i)->TraceConnectWithoutContext(
            "PhyRxEnd",
            MakeCallback(&addEnqueueTimeTagForSource));
    }

    // Tracing on experimental router & Write tracing files
    std::filesystem::create_directories(OUTPUT_DIR);
    AsciiTraceHelper ascii;

    // Enqueue tracing
    Ptr<OutputStreamWrapper> flowEnqueueStream =
        ascii.CreateFileStream(OUTPUT_DIR / "flow-enqueue.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Enqueue",
        MakeBoundCallback(&enqueueFlowTrace, flowEnqueueStream));

    // Dequeue tracing
    Ptr<OutputStreamWrapper> flowDequeueStream =
        ascii.CreateFileStream(OUTPUT_DIR / "flow-dequeue.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&dequeueFlowTrace, flowDequeueStream));

    // Drop tracing
    Ptr<OutputStreamWrapper> flowDropStream = ascii.CreateFileStream(OUTPUT_DIR / "flow-drop.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Drop",
        MakeBoundCallback(&dropFlowTrace, flowDropStream));

    // Throughput tracing
    Ptr<OutputStreamWrapper> routerThroughputStream =
        ascii.CreateFileStream(OUTPUT_DIR / "router-throughput.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&throughputRouterTrace, routerThroughputStream));

    Ptr<OutputStreamWrapper> flowThroughputStream =
        ascii.CreateFileStream(OUTPUT_DIR / "flow-throughput.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&throughputFlowTrace, flowThroughputStream));

    Ptr<OutputStreamWrapper> appSendingDataRateStream =
        ascii.CreateFileStream(OUTPUT_DIR / "app-sending-data-rate.csv");
    Ptr<OutputStreamWrapper> serverReceivingDataRateStream =
        ascii.CreateFileStream(OUTPUT_DIR / "server-receiving-data-rate.csv");
    for (uint8_t i = 0; i < serverApps.GetN(); i++)
    {
        Ptr<MyApplication> clientApplication = clientApps.Get(i)->GetObject<MyApplication>();
        clientApplication->TraceConnectWithoutContext(
            "Tx",
            MakeBoundCallback(&sendingDataRateAppTrace, appSendingDataRateStream));

        Ptr<PacketSink> serverApplication = serverApps.Get(i)->GetObject<PacketSink>();
        serverApplication->TraceConnectWithoutContext(
            "Rx",
            MakeBoundCallback(&receivingDataRateServerTrace, serverReceivingDataRateStream));
    }

    // Packets in queue tracing
    Ptr<OutputStreamWrapper> packetsInQueueStream =
        ascii.CreateFileStream(OUTPUT_DIR / "flow-packets-in-queue.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Enqueue",
        MakeBoundCallback(&packetsInQueueFlowTrace, packetsInQueueStream, true));
    experimentQueueDisc->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&packetsInQueueFlowTrace, packetsInQueueStream, false));

    // Delay tracing
    Ptr<OutputStreamWrapper> delayStream = ascii.CreateFileStream(OUTPUT_DIR / "flow-delay.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&delayFlowTrace, delayStream));

    // Application tracing
    ApplicationTrace applicationTrace;
    applicationTrace.qosStream = ascii.CreateFileStream(OUTPUT_DIR / "qos.csv");
    applicationTrace.rttStream = ascii.CreateFileStream(OUTPUT_DIR / "rtt.csv");

    applicationTrace.sentReceivedBytesStream =
        ascii.CreateFileStream(OUTPUT_DIR / "sent-received-bytes.csv");
    applicationTrace.retransmissionBytesStream =
        ascii.CreateFileStream(OUTPUT_DIR / "retransmission-bytes.csv");

    applicationTrace.APP_BWS = LEFT_LEAF_APP_BWS;
    applicationTrace.APP_TYPES = LEFT_LEAF_APP_TYPES;
    applicationTrace.APP_INTERVAL_TIMES = LEFT_LEAF_APP_INTERVAL_TIMES;
    applicationTrace.LEFT_LEAF_DELAYS = LEFT_LEAF_DELAYS;
    applicationTrace.BOTTLENECK_DELAY = BOTTLENECK_DELAY;
    applicationTrace.TOTAL_APPLICATIONS = NUM_LEAFS;

    for (uint32_t i = 0; i < clientApps.GetN(); i++)
    {
        leftLeafDevices.Get(i)->TraceConnectWithoutContext(
            "PhyTxBegin",
            MakeBoundCallback(&ApplicationTrace::ApplicationSentReceivedBytesTrace,
                              &applicationTrace,
                              i,
                              true));
        leftLeafDevices.Get(i)->TraceConnectWithoutContext(
            "PhyRxEnd",
            MakeBoundCallback(&ApplicationTrace::RTTTrace, &applicationTrace, i, false));

        rightLeafDevices.Get(i)->TraceConnectWithoutContext(
            "PhyRxEnd",
            MakeBoundCallback(&ApplicationTrace::ApplicationSentReceivedBytesTrace,
                              &applicationTrace,
                              i,
                              false));
        rightLeafDevices.Get(i)->TraceConnectWithoutContext(
            "PhyRxEnd",
            MakeBoundCallback(&ApplicationTrace::RTTTrace, &applicationTrace, i, true));

        Ptr<MyApplication> clientApplication = clientApps.Get(i)->GetObject<MyApplication>();
        clientApplication->TraceConnectWithoutContext(
            "SocketTrace",
            MakeBoundCallback(&ApplicationTrace::SocketTrace, &applicationTrace, i));

        Ptr<PacketSink> serverApplication = serverApps.Get(i)->GetObject<PacketSink>();
        serverApplication->TraceConnectWithoutContext(
            "RxWithAddresses",
            MakeBoundCallback(&ApplicationTrace::ApplicationThroughputTrace, &applicationTrace, i));
    }

    applicationTrace.delayStream = ascii.CreateFileStream(OUTPUT_DIR / "application-delay.csv");
    experimentQueueDisc->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&ApplicationTrace::delayTrace, &applicationTrace));

    applicationTrace.Initialize();

    // p2pBottleneck.EnablePcap(OUTPUT_DIR / "capture", routersDevices.Get(0), true);

    NS_LOG_UNCOND("===Starting simulation...===");
    Simulator::Schedule(Seconds(10), &logCurrentProgress, STOP_TIME);

    auto start = std::chrono::high_resolution_clock::now();

    Simulator::Stop(Seconds(STOP_TIME));
    Simulator::Run();
    Simulator::Destroy();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    NS_LOG_UNCOND("✅ Simulation finished -> Duration: " << duration.count() << "s");

    return 0;
}
