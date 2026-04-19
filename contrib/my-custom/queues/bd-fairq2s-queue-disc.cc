#include "bd-fairq2s-queue-disc.h"

#include "ns3/simulator.h"

#include <chrono>
#include <thread>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BDFairQ2SQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(BDFairQ2SQueueDisc);

TypeId
BDFairQ2SQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::BDFairQ2SQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<BDFairQ2SQueueDisc>()
            .AddAttribute("BufferWeightAlpha",
                          "The alpha value for buffer weight in the combined weight calculation.",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&BDFairQ2SQueueDisc::m_bufferWeightAlpha),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddTraceSource("RtqsEnqueue",
                            "Enqueue a packet in the queue.",
                            MakeTraceSourceAccessor(&BDFairQ2SQueueDisc::m_traceRtqsEnqueue),
                            "ns3::BDFairQ2SQueueDisc::TracedCallback")
            .AddTraceSource("RtqsDequeue",
                            "Dequeue a packet from the queue.",
                            MakeTraceSourceAccessor(&BDFairQ2SQueueDisc::m_traceRtqsDequeue),
                            "ns3::BDFairQ2SQueueDisc::TracedCallback")
            .AddTraceSource("RtqsDrop",
                            "Drop a packet (for whatever reason).",
                            MakeTraceSourceAccessor(&BDFairQ2SQueueDisc::m_traceRtqsDrop),
                            "ns3::BDFairQ2SQueueDisc::TracedCallback");

    return tid;
}

BDFairQ2SQueueDisc::BDFairQ2SQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES)
{
    NS_LOG_FUNCTION(this);
}

BDFairQ2SQueueDisc::~BDFairQ2SQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
BDFairQ2SQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
}

bool
BDFairQ2SQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(GetNQueueDiscClasses() == 0,
                  "BDFairQ2SQueueDisc does not support queue disc classes");
    NS_ASSERT_MSG(GetNPacketFilters() == 0, "BDFairQ2SQueueDisc does not support packet filters");

    // The MaxSize set here is the maximum length of the physical device,
    // while the actual queue length is dynamic and obtained by getBuffer.
    if (GetNInternalQueues() == 0)
    {
        // Detection queue
        AddInternalQueue(CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>(
            "MaxSize",
            QueueSizeValue(m_maxQueueSize[BDQueueId::BD_DETECTION_QUEUE])));
        // Realtime queue
        AddInternalQueue(CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>(
            "MaxSize",
            QueueSizeValue(m_maxQueueSize[BDQueueId::BD_REALTIME_QUEUE])));
        // NonRealtime queue
        AddInternalQueue(CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>(
            "MaxSize",
            QueueSizeValue(m_maxQueueSize[BDQueueId::BD_NON_REALTIME_QUEUE])));
        // other queue
        AddInternalQueue(CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>(
            "MaxSize",
            QueueSizeValue(m_maxQueueSize[BDQueueId::BD_OTHER_QUEUE])));
    }

    NS_ASSERT_MSG(GetNInternalQueues() == 4, "BDFairQ2SQueueDisc needs 4 internal queues");

    // Set weights
    m_delayWeightAlpha = 1 - m_bufferWeightAlpha;
    NS_LOG_UNCOND("⚙️ m_bufferWeightAlpha: " << m_bufferWeightAlpha
                                            << ", m_delayWeightAlpha: " << m_delayWeightAlpha);

    return true;
}

BDQueueId
BDFairQ2SQueueDisc::DistributeToBDQueueId(uint32_t applicationId, FlowType flowType)
{
    NS_LOG_FUNCTION(this << applicationId << flowType);

    if (flowType == FlowType::HANDSHAKE_FLOW)
    {
        return BDQueueId::BD_DETECTION_QUEUE;
    }
    else if (flowType == FlowType::REALTIME_FLOW)
    {
        return BDQueueId::BD_REALTIME_QUEUE;
    }
    else if (flowType == FlowType::NON_REALTIME_FLOW)
    {
        return BDQueueId::BD_NON_REALTIME_QUEUE;
    }
    else if (flowType == FlowType::OTHER_FLOW)
    {
        return BDQueueId::BD_OTHER_QUEUE;
    }

    return BDQueueId::BD_OTHER_QUEUE;
}

// void
// BDFairQ2SQueueDisc::SetQueueSizeThreshold(BDQueueId queueId, uint32_t newSize)
// {
//     const uint32_t minSize = m_minQueueSize[queueId].GetValue();
//     const uint32_t maxSize = m_maxQueueSize[queueId].GetValue();

//     uint32_t targetNewSize = std::min(maxSize, newSize);
//     targetNewSize = std::max(minSize, targetNewSize);

//     m_queueSizeThreshold[queueId] = QueueSize(QueueSizeUnit::PACKETS, targetNewSize);
// }

uint32_t
BDFairQ2SQueueDisc::GetEnqueueFlowCount(BDQueueId queueId)
{
    // Get current enqueue throughput recorder for Queue: queueId.
    default_map<uint32_t, uint32_t>& currentQueuePacketCount = m_queuePacketCount[queueId];

    // Count the number of flow in this queue.
    uint32_t flowCount = 0;
    for (auto it = currentQueuePacketCount.begin(); it != currentQueuePacketCount.end(); it++)
    {
        if (it->first != ApplicationIdTag::NOT_APPLICATION_ID
            // && it->second > 0
        )
        {
            flowCount++;
        }
    }
    return flowCount;
}

void
BDFairQ2SQueueDisc::UpdateFlowStatus(uint32_t applicationId, FlowStatus newFlowStatus)
{
    NS_LOG_FUNCTION(this << applicationId);

    m_flowStatus[applicationId] = newFlowStatus;
}

void
BDFairQ2SQueueDisc::UpdateEnqueueThroughput(BDQueueId queueId, Ptr<QueueDiscItem> item)
{
    Ptr<Packet> packet = item->GetPacket();

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    if (flowTypeTag.Get() == FlowType::HANDSHAKE_FLOW)
    {
        return;
    }

    if (!m_enqueueTimeRecord.contains(queueId))
    {
        m_enqueueTimeRecord[queueId].push(Simulator::Now());
        m_enqueueBufferRecord[queueId].push(item->GetSize());
    }

    uint64_t waitingTime =
        Simulator::Now().GetNanoSeconds() - m_enqueueTimeRecord[queueId].back().GetNanoSeconds();
    if (waitingTime > 0)
    {
        if (m_enqueueBufferRecord[queueId].size() < RECEIVE_PACKET_DELTA_T_WINDOW)
        {
            m_enqueueTimeRecord[queueId].push(Simulator::Now());
            m_enqueueBufferRecord[queueId].push(item->GetSize());
            m_enqueueWaitingTimeSum[queueId] += waitingTime;
            m_enqueueBufferSum[queueId] += item->GetSize();
        }
        else
        {
            Time frontEnqueueTime = m_enqueueTimeRecord[queueId].front();
            uint64_t frontEnqueueBuffer = m_enqueueBufferRecord[queueId].front();

            m_enqueueTimeRecord[queueId].pop();
            m_enqueueBufferRecord[queueId].pop();

            m_enqueueWaitingTimeSum[queueId] -=
                (m_enqueueTimeRecord[queueId].front() - frontEnqueueTime).GetNanoSeconds();
            m_enqueueBufferSum[queueId] -= frontEnqueueBuffer;

            m_enqueueTimeRecord[queueId].push(Simulator::Now());
            m_enqueueBufferRecord[queueId].push(item->GetSize());
            m_enqueueWaitingTimeSum[queueId] += waitingTime;
            m_enqueueBufferSum[queueId] += item->GetSize();
        }

        m_enqueueThroughput[queueId] =
            DataRate((m_enqueueBufferSum[queueId]) * 8 * 1e9 / m_enqueueWaitingTimeSum[queueId]);
    }
}

bool
BDFairQ2SQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    ApplicationIdTag applicationIdTag;
    bool hasApplicationIdTag = item->GetPacket()->FindFirstMatchingByteTag(applicationIdTag);

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    SendTimeTag sendTimeTag;
    item->GetPacket()->FindFirstMatchingByteTag(sendTimeTag);

    // ===Detect new flow===
    if (hasApplicationIdTag && m_flowStatus[applicationIdTag.Get()] == FlowStatus::NOT_STARTED)
    {
        m_flowStatus[applicationIdTag.Get()] = FlowStatus::STARTING;

        // Change the new flow to STABLE after a specific time
        Simulator::Schedule(Seconds(5),
                            &BDFairQ2SQueueDisc::UpdateFlowStatus,
                            this,
                            applicationIdTag.Get(),
                            FlowStatus::STABLE);
    }

    // Get Queue Id for do enqueue
    BDQueueId queueId = DistributeToBDQueueId(applicationIdTag.Get(), flowTypeTag.Get());

    // The realtime packet may also be assigned to Other Queue if the irrelevant delay is too large.
    // In this case, change the flow type to Other Flow (abandoned flow).
    if (queueId == BDQueueId::BD_OTHER_QUEUE)
    {
        flowTypeTag.Set(FlowType::OTHER_FLOW);
    }

    // ===Check and Resize Realtime Queues===
    // if (queueId == BDQueueId::BD_REALTIME_QUEUE)
    // {
    //     uint32_t onePacketBuffer =
    //         GetInternalQueue(queueId)->GetNBytes() * 8 /
    //         GetInternalQueue(queueId)->GetNPackets();
    //     uint32_t newBuffer = m_dequeueThroughput[queueId].GetBitRate() *
    //                          REALTIME_DELAY_THRESHOLD.GetMilliSeconds() / 1000;
    //     uint32_t newQueueSize = newBuffer / onePacketBuffer;
    //     SetQueueSizeThreshold(queueId, newQueueSize);
    // }

    // ===Judge the packet that can enter the queue===
    // If the packet count after enqueue exceeds the max queue size, drop the packet.
    if (GetInternalQueue(queueId)->GetNPackets() + 1 > m_maxQueueSize[queueId].GetValue())
    {
        DropBeforeEnqueue(item, "Queue is full");
        // NS_LOG_UNCOND("❌ Drop packet before enqueue: Queue: "
        //               << BD_QUEUE_IDS[queueId]
        //               << ", NPackets: " << GetInternalQueue(queueId)->GetNPackets());
        m_traceRtqsDrop(this, queueId, item);

        return false;
    }

    // ===Enqueue packet===
    bool isEnqueued = GetInternalQueue(queueId)->Enqueue(item);
    if (isEnqueued)
    {
        NS_LOG_LOGIC("Enqueue packet to " << BD_QUEUE_IDS[queueId] << ", current queue size: "
                                          << GetInternalQueue(queueId)->GetCurrentSize()
                                          << ", max queue size: " << m_maxQueueSize[queueId]);

        m_traceRtqsEnqueue(this, queueId, item);

        // Update variables
        m_queuePacketCount[queueId][applicationIdTag.Get()]++;

        // Update enqueue throughput
        UpdateEnqueueThroughput(queueId, item);
    }
    else
    {
        m_traceRtqsDrop(this, queueId, item);
    }

    return isEnqueued;
}

void
BDFairQ2SQueueDisc::UpdateBandwidth(Ptr<QueueDiscItem> item)
{
    Ptr<Packet> packet = item->GetPacket();

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    if (flowTypeTag.Get() == FlowType::HANDSHAKE_FLOW)
    {
        return;
    }

    if (m_bandwidthTimeRecord.empty())
    {
        m_bandwidthTimeRecord.push(Simulator::Now());
        m_bandwidthBufferRecord.push(item->GetSize());
    }

    uint64_t waitingTime =
        Simulator::Now().GetNanoSeconds() - m_bandwidthTimeRecord.back().GetNanoSeconds();
    if (waitingTime > 0)
    {
        if (m_bandwidthBufferRecord.size() < RECEIVE_PACKET_DELTA_T_WINDOW)
        {
            m_bandwidthTimeRecord.push(Simulator::Now());
            m_bandwidthBufferRecord.push(item->GetSize());
            m_bandwidthWaitingTimeSum += waitingTime;
            m_bandwidthBufferSum += item->GetSize();
        }
        else
        {
            Time frontBandwidthTime = m_bandwidthTimeRecord.front();
            uint64_t frontBandwidthBuffer = m_bandwidthBufferRecord.front();

            m_bandwidthTimeRecord.pop();
            m_bandwidthBufferRecord.pop();

            m_bandwidthWaitingTimeSum -=
                (m_bandwidthTimeRecord.front() - frontBandwidthTime).GetNanoSeconds();
            m_bandwidthBufferSum -= frontBandwidthBuffer;

            m_bandwidthTimeRecord.push(Simulator::Now());
            m_bandwidthBufferRecord.push(item->GetSize());
            m_bandwidthWaitingTimeSum += waitingTime;
            m_bandwidthBufferSum += item->GetSize();
        }

        m_bandwidth = DataRate((m_bandwidthBufferSum) * 8 * 1e9 / m_bandwidthWaitingTimeSum);
    }
}

void
BDFairQ2SQueueDisc::UpdateDequeueThroughput(BDQueueId queueId, Ptr<QueueDiscItem> item)
{
    Ptr<Packet> packet = item->GetPacket();

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    if (flowTypeTag.Get() == FlowType::HANDSHAKE_FLOW)
    {
        return;
    }

    if (!m_dequeueTimeRecord.contains(queueId))
    {
        m_dequeueTimeRecord[queueId].push(Simulator::Now());
        m_dequeueBufferRecord[queueId].push(item->GetSize());
    }

    uint64_t waitingTime =
        Simulator::Now().GetNanoSeconds() - m_dequeueTimeRecord[queueId].back().GetNanoSeconds();
    if (waitingTime > 0)
    {
        if (m_dequeueBufferRecord[queueId].size() < RECEIVE_PACKET_DELTA_T_WINDOW)
        {
            m_dequeueTimeRecord[queueId].push(Simulator::Now());
            m_dequeueBufferRecord[queueId].push(item->GetSize());
            m_dequeueWaitingTimeSum[queueId] += waitingTime;
            m_dequeueBufferSum[queueId] += item->GetSize();
        }
        else
        {
            Time frontDequeueTime = m_dequeueTimeRecord[queueId].front();
            uint64_t frontDequeueBuffer = m_dequeueBufferRecord[queueId].front();

            m_dequeueTimeRecord[queueId].pop();
            m_dequeueBufferRecord[queueId].pop();

            m_dequeueWaitingTimeSum[queueId] -=
                (m_dequeueTimeRecord[queueId].front() - frontDequeueTime).GetNanoSeconds();
            m_dequeueBufferSum[queueId] -= frontDequeueBuffer;

            m_dequeueTimeRecord[queueId].push(Simulator::Now());
            m_dequeueBufferRecord[queueId].push(item->GetSize());
            m_dequeueWaitingTimeSum[queueId] += waitingTime;
            m_dequeueBufferSum[queueId] += item->GetSize();
        }

        m_dequeueThroughput[queueId] =
            DataRate((m_dequeueBufferSum[queueId]) * 8 * 1e9 / m_dequeueWaitingTimeSum[queueId]);
    }
}

Time
BDFairQ2SQueueDisc::GetQueueWaitingTime(BDQueueId queueId)
{
    if (GetInternalQueue(queueId)->GetNPackets() > 0)
    {
        Ptr<const QueueDiscItem> lastQueueDiscItem = GetInternalQueue(queueId)->Peek();
        Ptr<Packet> lastPacket = lastQueueDiscItem->GetPacket();

        EnqueueTimeTag enqueueTimeTag;
        lastPacket->FindFirstMatchingByteTag(enqueueTimeTag);

        Time sojournTime = Simulator::Now() - enqueueTimeTag.Get();
        return sojournTime;
    }
    else
    {
        return Time(0);
    }
}

void
BDFairQ2SQueueDisc::UpdateNextDropTime(BDQueueId queueId, uint32_t applicationId, bool isDropped)
{
    if (isDropped)
    {
        m_dropTimeRecords[applicationId].push(Simulator::Now());
    }
    else if (m_nextDropTime[applicationId] + TIME_UNIT < Simulator::Now())
    {
        // Clear the drop time records if the last drop time is too old (greater than TIME_UNIT)
        m_dropTimeRecords[applicationId] = std::queue<Time>();
    }

    uint32_t numFlows = GetEnqueueFlowCount(queueId);
    if (numFlows == 0)
    {
        // m_nextDropTime[applicationId] = Simulator::Now() + TIME_UNIT;
        // return;
    }

    uint32_t dropCount = m_dropTimeRecords[applicationId].size();
    if (dropCount == 0)
    {
        // To avoid division by zero, set dropCount to 1 if there is no drop record.
        dropCount = 1;
    }

    m_nextDropTime[applicationId] =
        Simulator::Now() + TIME_UNIT / std::sqrt(dropCount);
}

Ptr<QueueDiscItem>
BDFairQ2SQueueDisc::DequeuePacket(BDQueueId queueId)
{
    NS_LOG_FUNCTION(this << queueId);

    Ptr<QueueDiscItem> item = GetInternalQueue(queueId)->Dequeue();
    NS_ASSERT(item != nullptr);

    // NS_LOG_UNCOND("Dequeue packet from " << BD_QUEUE_IDS[queueId]
    //                              << ", current queue size: " <<
    //                              GetInternalQueue(queueId)->GetCurrentSize()
    //                              << ", max queue size: " << m_queueBufferSize[queueId]);
    m_traceRtqsDequeue(this, queueId, item);

    // Update dequeue throughput
    UpdateDequeueThroughput(queueId, item);

    // Update bandwidth
    UpdateBandwidth(item);

    ApplicationIdTag applicationIdTag;
    item->GetPacket()->FindFirstMatchingByteTag(applicationIdTag);

    // Update queue record
    m_queuePacketCount[queueId][applicationIdTag.Get()]--;

    NS_LOG_LOGIC("Dequeued item from queue " << BD_QUEUE_IDS[queueId]);
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Update next drop time
    UpdateNextDropTime(queueId, applicationIdTag.Get(), false);

    return item;
}

void
BDFairQ2SQueueDisc::DropPacket(BDQueueId queueId, std::string reason)
{
    NS_LOG_FUNCTION(this << queueId);

    Ptr<QueueDiscItem> item = GetInternalQueue(queueId)->Dequeue();
    NS_ASSERT(item != nullptr);

    ApplicationIdTag applicationIdTag;
    item->GetPacket()->FindFirstMatchingByteTag(applicationIdTag);

    DropAfterDequeue(item, reason.c_str());
    m_traceRtqsDrop(this, queueId, item);

    // Update queue record
    m_queuePacketCount[queueId][applicationIdTag.Get()]--;

    // Update dequeue throughput
    UpdateDequeueThroughput(queueId, item);

    // Update bandwidth
    UpdateBandwidth(item);

    // Update next drop time
    UpdateNextDropTime(queueId, applicationIdTag.Get(), true);
}

Ptr<QueueDiscItem>
BDFairQ2SQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    // m_isInDequeuePhase = false;
    // for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
    // {
    //     BDQueueId queueId = static_cast<BDQueueId>(i);
    //     if (m_scheduledDequeueBuffer[queueId] > 0 && GetInternalQueue(queueId)->GetNPackets() >
    //     0)
    //     {
    //         m_isInDequeuePhase = true;
    //         break;
    //     }
    // }

    if (!m_isInDequeuePhase)
    {
        m_isInDequeuePhase = true;
        m_overflowScheduledDequeueBuffer.clear();
        m_scheduledDequeueBuffer.clear();

        // Step 1: Prepare parameters for scheduling
        Time realtimeQueueSoujournTime = GetQueueWaitingTime(BDQueueId::BD_REALTIME_QUEUE);
        Time remainingDelayTimeToThreshold = Time(0);
        if (realtimeQueueSoujournTime < REALTIME_DELAY_THRESHOLD)
        {
            remainingDelayTimeToThreshold = REALTIME_DELAY_THRESHOLD - realtimeQueueSoujournTime;
        }

        uint64_t maxBufferReatimeQueue =
            m_dequeueThroughput[BDQueueId::BD_REALTIME_QUEUE].GetBitRate() *
            remainingDelayTimeToThreshold.GetMilliSeconds() / 1000;
        uint64_t overflowBufferReatimeQueue =
            std::max(int64_t(0),
                     int64_t(GetInternalQueue(BDQueueId::BD_REALTIME_QUEUE)->GetNBytes()) -
                         int64_t(maxBufferReatimeQueue));

        default_map<BDQueueId, uint64_t> queueId_normalBuffer{0};
        queueId_normalBuffer[BDQueueId::BD_REALTIME_QUEUE] =
            GetInternalQueue(BDQueueId::BD_REALTIME_QUEUE)->GetNBytes() -
            overflowBufferReatimeQueue;
        queueId_normalBuffer[BDQueueId::BD_NON_REALTIME_QUEUE] =
            GetInternalQueue(BDQueueId::BD_NON_REALTIME_QUEUE)->GetNBytes();
        queueId_normalBuffer[BDQueueId::BD_OTHER_QUEUE] =
            GetInternalQueue(BDQueueId::BD_OTHER_QUEUE)->GetNBytes();

        uint64_t remainingScheduledDequeueBuffer =
            m_bandwidth.GetBitRate() / 8 * TIME_UNIT.GetMilliSeconds() / 1000;

        // Step 2: Calculate overflow buffer for realtime queue
        m_overflowScheduledDequeueBuffer[BDQueueId::BD_REALTIME_QUEUE] =
            std::min(overflowBufferReatimeQueue, remainingScheduledDequeueBuffer);
        remainingScheduledDequeueBuffer -=
            m_overflowScheduledDequeueBuffer[BDQueueId::BD_REALTIME_QUEUE];

        // Step 3: Normal dequeue buffer allocation
        default_map<BDQueueId, uint64_t> queueId_BufferWeight{0};
        uint64_t totalBufferWeight = 0;

        default_map<BDQueueId, double> queueId_DelayWeight{0};
        double totalDelayWeight = 0;

        for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
        {
            BDQueueId queueId = static_cast<BDQueueId>(i);

            // Calculate queue buffer weight
            uint64_t futureBuffer =
                queueId_normalBuffer[queueId] +
                m_enqueueThroughput[queueId].GetBitRate() / 8 * TIME_UNIT.GetMilliSeconds() / 1000;

            uint32_t numFlows = GetEnqueueFlowCount(queueId);
            if (numFlows == 0)
            {
                numFlows = 1;
            }

            queueId_BufferWeight[queueId] = futureBuffer / numFlows;
            totalBufferWeight += queueId_BufferWeight[queueId];

            // Calculate queue delay weight
            uint64_t futureWaitingTime =
                GetQueueWaitingTime(queueId).GetMicroSeconds() + TIME_UNIT.GetMicroSeconds();

            queueId_DelayWeight[queueId] = static_cast<double>(futureWaitingTime) /
                                           m_maxEstimatedDequeueTime[queueId].GetMicroSeconds();
            totalDelayWeight += queueId_DelayWeight[queueId];
        }

        // Normalization for buffer weight and delay weight
        default_map<BDQueueId, double> queueId_CombinationWeight{0};
        double totalCombinationWeight = 0;
        for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
        {
            BDQueueId queueId = static_cast<BDQueueId>(i);

            double normalizedBufferWeight =
                static_cast<double>(queueId_BufferWeight[queueId]) / totalBufferWeight;

            double normalizedDelayWeight = queueId_DelayWeight[queueId] / totalDelayWeight;

            queueId_CombinationWeight[queueId] = m_bufferWeightAlpha * normalizedBufferWeight +
                                                 m_delayWeightAlpha * normalizedDelayWeight;
            totalCombinationWeight += queueId_CombinationWeight[queueId];
        }

        static Time lastTime0 = Time(0);
        if (Simulator::Now() - lastTime0 >= Time("200ms"))
        {
            NS_LOG_UNCOND("---At " << Simulator::Now().GetSeconds() << "s");

            for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
            {
                BDQueueId queueId = static_cast<BDQueueId>(i);

                NS_LOG_UNCOND("Queue " << BD_QUEUE_IDS[queueId]
                                       << " queueId_BufferWeight: " << queueId_BufferWeight[queueId]
                                       << ", totalBufferWeight: " << totalBufferWeight
                                       << ", queueId_DelayWeight: " << queueId_DelayWeight[queueId]
                                       << ", totalDelayWeight: " << totalDelayWeight
                                       << ", queueId_CombinationWeight: "
                                       << queueId_CombinationWeight[queueId]);
            }
            lastTime0 = Simulator::Now();
        }

        for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
        {
            BDQueueId queueId = static_cast<BDQueueId>(i);
            m_scheduledDequeueBuffer[queueId] = remainingScheduledDequeueBuffer *
                                                queueId_CombinationWeight[queueId] /
                                                totalCombinationWeight;
        }

        // Log the scheduling result
        static Time lastTime1 = Time(0);
        if (Simulator::Now() - lastTime1 >= Time("200ms"))
        {
            NS_LOG_UNCOND("---At " << Simulator::Now().GetSeconds() << "s");

            for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
            {
                BDQueueId queueId = static_cast<BDQueueId>(i);

                NS_LOG_UNCOND("Queue " << BD_QUEUE_IDS[queueId]
                                       << " m_overflowScheduledDequeueBuffer: "
                                       << m_overflowScheduledDequeueBuffer[queueId]
                                       << " B, m_scheduledDequeueBuffer: "
                                       << m_scheduledDequeueBuffer[queueId] << " B");
            }
            lastTime1 = Simulator::Now();
        }
    }
    else
    {
        NS_LOG_DEBUG("In dequeue phase");
    }

    // Log the current status of each queue
    static Time lastTime = Time(0);
    if (Simulator::Now() - lastTime >= Time("200ms"))
    {
        NS_LOG_UNCOND("---At " << Simulator::Now().GetSeconds() << "s");

        for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
        {
            BDQueueId queueId = static_cast<BDQueueId>(i);

            NS_LOG_UNCOND("Queue " << BD_QUEUE_IDS[queueId] << " m_enqueueThroughput: "
                                   << m_enqueueThroughput[queueId].GetBitRate() / 1e6
                                   << ", m_dequeueThroughput: "
                                   << m_dequeueThroughput[queueId].GetBitRate() / 1e6);

            NS_LOG_UNCOND("Number of flows: " << GetEnqueueFlowCount(queueId) << " flows");

            NS_LOG_UNCOND("Current queue size: " << GetInternalQueue(queueId)->GetNPackets()
                                                 << " packets");

            NS_LOG_UNCOND("m_overflowScheduledDequeueBuffer[queueId]: "
                          << m_overflowScheduledDequeueBuffer[queueId]
                          << " B, m_scheduledDequeueBuffer[queueId]: "
                          << m_scheduledDequeueBuffer[queueId] << " B");

            NS_LOG_UNCOND("m_estimatedDequeueTime[queueId]: "
                          << m_estimatedDequeueTime[queueId].GetMilliSeconds() << " ms");

            NS_LOG_UNCOND("m_nextDropTime: " << m_nextDropTime[queueId].GetMilliSeconds() << " ms");
        }
        lastTime = Simulator::Now();
    }

    // The detection queue always takes precedence over the other queues.
    if (GetInternalQueue(BDQueueId::BD_DETECTION_QUEUE)->GetNPackets() > 0)
    {
        return DequeuePacket(BDQueueId::BD_DETECTION_QUEUE);
    }

    if (m_overflowScheduledDequeueBuffer[BDQueueId::BD_REALTIME_QUEUE] > 0)
    {
        Ptr<const QueueDiscItem> item = GetInternalQueue(BDQueueId::BD_REALTIME_QUEUE)->Peek();
        m_overflowScheduledDequeueBuffer[BDQueueId::BD_REALTIME_QUEUE] =
            std::max(int64_t(0),
                     int64_t(m_overflowScheduledDequeueBuffer[BDQueueId::BD_REALTIME_QUEUE]) -
                         item->GetSize());

        return DequeuePacket(BDQueueId::BD_REALTIME_QUEUE);
    }

    // Queue Weight = Buffer Size / Number of Flows in the Queue
    default_map<BDQueueId, uint64_t> queueId_DequeueWeight{0};

    std::vector<BDQueueId> activeBDQueueIds;
    for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <= BDQueueId::BD_OTHER_QUEUE; i++)
    {
        BDQueueId queueId = static_cast<BDQueueId>(i);

        if (m_scheduledDequeueBuffer[queueId] > 0)
        {
            queueId_DequeueWeight[queueId] = m_scheduledDequeueBuffer[queueId];

            activeBDQueueIds.push_back(queueId);
        }
    }

    if (activeBDQueueIds.size() == 0)
    {
        NS_LOG_UNCOND("No active queue to dequeue");
        m_isInDequeuePhase = false;
        return nullptr;
    }

    // Decreasing sort by weight
    if (queueId_DequeueWeight.size() > 1)
    {
        // NS_LOG_UNCOND(queueId_CombinationWeight.size() << " active queues after weight
        // calculation:"); for (uint8_t i = BDQueueId::BD_REALTIME_QUEUE; i <=
        // BDQueueId::BD_OTHER_QUEUE; i++)
        // {
        //     BDQueueId queueId = static_cast<BDQueueId>(i);
        //     NS_LOG_UNCOND("Queue " << BD_QUEUE_IDS[queueId]
        //                         << ", BufferWeight: " << queueId_BufferWeight[queueId]
        //                         << ", DelayWeight: " << queueId_DelayWeight[queueId]
        //                         << ", NormalizedWeight: " << queueId_CombinationWeight[queueId]);
        // }
        std::sort(activeBDQueueIds.begin(),
                  activeBDQueueIds.end(),
                  [&queueId_DequeueWeight](const BDQueueId& a, const BDQueueId& b) {
                      return queueId_DequeueWeight[a] > queueId_DequeueWeight[b];
                  });
    }

    // Log the queue weight
    static Time lastTime2 = Time(0);
    if (Simulator::Now() - lastTime2 >= Time("200ms"))
    {
        NS_LOG_UNCOND("---At " << Simulator::Now().GetSeconds() << "s");

        for (const auto& queueId : activeBDQueueIds)
        {
            NS_LOG_UNCOND("Queue " << BD_QUEUE_IDS[queueId]
                                   << ", dequeueWeight: " << queueId_DequeueWeight[queueId]);
        }
        lastTime2 = Simulator::Now();
    }

    for (const auto& queueId : activeBDQueueIds)
    {
        if (GetInternalQueue(queueId)->GetNPackets() > 0)
        {
            Ptr<const QueueDiscItem> item = GetInternalQueue(queueId)->Peek();

            ApplicationIdTag applicationIdTag;
            item->GetPacket()->FindFirstMatchingByteTag(applicationIdTag);

            Time waitingTime = GetQueueWaitingTime(queueId);
            if (waitingTime > m_maxEstimatedDequeueTime[queueId])
            {
                if (Simulator::Now() >= m_nextDropTime[applicationIdTag.Get()])
                {
                    m_scheduledDequeueBuffer[queueId] = std::max(
                        int64_t(0),
                        int64_t(m_scheduledDequeueBuffer[queueId] - int64_t(item->GetSize())));
                    DropPacket(queueId, "Drop packet because of delay");

                    NS_LOG_UNCOND("❌ At " << Simulator::Now().GetSeconds()
                                           << "s, Drop packet from queue: " << BD_QUEUE_IDS[queueId]
                                           << ", applicationId: " << applicationIdTag.Get()
                                           << ", waiting time: " << waitingTime.GetMicroSeconds()
                                           << " μs, max estimated dequeue time: "
                                           << m_maxEstimatedDequeueTime[queueId].GetMicroSeconds()
                                           << " μs, next drop time: "
                                           << m_nextDropTime[applicationIdTag.Get()].GetSeconds()
                                           << " s");

                    return nullptr;
                }
            }

            m_scheduledDequeueBuffer[queueId] =
                std::max(int64_t(0),
                         int64_t(m_scheduledDequeueBuffer[queueId] - int64_t(item->GetSize())));

            return DequeuePacket(queueId);
        }
    }

    m_isInDequeuePhase = false;

    return nullptr;
}
