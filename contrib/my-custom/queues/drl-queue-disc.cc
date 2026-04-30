#include "drl-queue-disc.h"

#include "ns3/simulator.h"

#include <chrono>
#include <thread>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DRLQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(DRLQueueDisc);

TypeId
DRLQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DRLQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<DRLQueueDisc>()
            .AddTraceSource("DRLQueueEnqueue",
                            "Enqueue a packet in the queue.",
                            MakeTraceSourceAccessor(&DRLQueueDisc::m_traceDRLQueueEnqueue),
                            "ns3::DRLQueueDisc::TracedCallback")
            .AddTraceSource("DRLQueueDequeue",
                            "Dequeue a packet from the queue.",
                            MakeTraceSourceAccessor(&DRLQueueDisc::m_traceDRLQueueDequeue),
                            "ns3::DRLQueueDisc::TracedCallback")
            .AddTraceSource("DRLQueueDrop",
                            "Drop a packet (for whatever reason).",
                            MakeTraceSourceAccessor(&DRLQueueDisc::m_traceDRLQueueDrop),
                            "ns3::DRLQueueDisc::TracedCallback");

    return tid;
}

DRLQueueDisc::DRLQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES)
{
    NS_LOG_FUNCTION(this);
}

DRLQueueDisc::~DRLQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
DRLQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
}

bool
DRLQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(GetNQueueDiscClasses() == 0,
                  "BDFairQ2SQueueDisc does not support queue disc classes");
    NS_ASSERT_MSG(GetNPacketFilters() == 0, "BDFairQ2SQueueDisc does not support packet filters");

    // The MaxSize set here is the maximum length of the physical device,
    // while the actual queue length is dynamic and obtained by getBuffer.
    if (GetNInternalQueues() == 0)
    {
        AddInternalQueue(CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>(
            "MaxSize",
            QueueSizeValue(m_maxQueueSize[DRLQueueId::DRL_QUEUE])));
    }

    return true;
}

void
DRLQueueDisc::UpdateEnqueueThroughput(DRLQueueId queueId, Ptr<QueueDiscItem> item)
{
    Ptr<Packet> packet = item->GetPacket();

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    // if (flowTypeTag.Get() == FlowType::HANDSHAKE_FLOW)
    // {
    //     return;
    // }

    if (!m_enqueueTimeRecord.contains(queueId))
    {
        m_enqueueTimeRecord[queueId].push(Simulator::Now());
        m_enqueueBufferRecord[queueId].push(item->GetSize());
    }

    uint64_t waitingTime =
        Simulator::Now().GetNanoSeconds() - m_enqueueTimeRecord[queueId].back().GetNanoSeconds();
    if (waitingTime > 0)
    {
        if (m_enqueueBufferRecord[queueId].size() < NUM_PACKETS_PER_WINDOW)
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
DRLQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    ApplicationIdTag applicationIdTag;
    bool hasApplicationIdTag = item->GetPacket()->FindFirstMatchingByteTag(applicationIdTag);

    FlowTypeTag flowTypeTag;
    item->GetPacket()->FindFirstMatchingByteTag(flowTypeTag);

    SendTimeTag sendTimeTag;
    item->GetPacket()->FindFirstMatchingByteTag(sendTimeTag);

    DRLQueueId queueId = DRL_QUEUE;

    // ===Judge the packet that can enter the queue===
    // If the packet count after enqueue exceeds the max queue size, drop the packet.
    if (GetInternalQueue(queueId)->GetNPackets() + 1 > m_maxQueueSize[queueId].GetValue())
    {
        DropBeforeEnqueue(item, "Queue is full");
        // NS_LOG_UNCOND("❌ Drop packet before enqueue: Queue: "
        //               << BD_QUEUE_IDS[queueId]
        //               << ", NPackets: " << GetInternalQueue(queueId)->GetNPackets());
        m_traceDRLQueueDrop(this, queueId, item);

        return false;
    }

    // ===Enqueue packet===
    bool isEnqueued = GetInternalQueue(queueId)->Enqueue(item);
    if (isEnqueued)
    {
        NS_LOG_LOGIC("Enqueue packet to " << DRL_QUEUE_IDS[queueId] << ", current queue size: "
                                          << GetInternalQueue(queueId)->GetCurrentSize()
                                          << ", max queue size: " << m_maxQueueSize[queueId]);

        m_traceDRLQueueEnqueue(this, queueId, item);

        // Update enqueue throughput
        UpdateEnqueueThroughput(queueId, item);
    }
    else
    {
        m_traceDRLQueueDrop(this, queueId, item);
    }

    return isEnqueued;
}

void
DRLQueueDisc::UpdateBandwidth(Ptr<QueueDiscItem> item)
{
    Ptr<Packet> packet = item->GetPacket();

    // The packet here already remove 2 bytes from P2P header
    // So we add 2 bytes back to get the original packet size.
    uint32_t packetSize = item->GetSize() + 2;

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    // if (flowTypeTag.Get() == FlowType::HANDSHAKE_FLOW)
    // {
    //     return;
    // }

    if (m_bandwidthTimeRecord.empty())
    {
        m_bandwidthTimeRecord.push(Simulator::Now());
        m_bandwidthBufferRecord.push(packetSize);
    }

    Time waitingTime = Simulator::Now() - m_bandwidthTimeRecord.back();
    if (waitingTime > Time(0))
    {
        if (m_bandwidthBufferRecord.size() < NUM_PACKETS_PER_WINDOW)
        {
            m_bandwidthTimeRecord.push(Simulator::Now());
            m_bandwidthBufferRecord.push(packetSize);
            m_bandwidthWaitingTimeSum += waitingTime;
            m_bandwidthBufferSum += packetSize;
        }
        else
        {
            Time frontBandwidthTime = m_bandwidthTimeRecord.front();
            uint64_t frontBandwidthBuffer = m_bandwidthBufferRecord.front();

            m_bandwidthTimeRecord.pop();
            m_bandwidthBufferRecord.pop();

            m_bandwidthWaitingTimeSum -= (m_bandwidthTimeRecord.front() - frontBandwidthTime);
            m_bandwidthBufferSum -= frontBandwidthBuffer;

            m_bandwidthTimeRecord.push(Simulator::Now());
            m_bandwidthBufferRecord.push(packetSize);
            m_bandwidthWaitingTimeSum += waitingTime;
            m_bandwidthBufferSum += packetSize;
        }

        m_bandwidth =
            DataRate((m_bandwidthBufferSum) * 8 * 1e9 / m_bandwidthWaitingTimeSum.GetNanoSeconds());
    }
}

void
DRLQueueDisc::UpdateDequeueThroughput(DRLQueueId queueId, Ptr<QueueDiscItem> item)
{
    Ptr<Packet> packet = item->GetPacket();

    FlowTypeTag flowTypeTag;
    packet->FindFirstMatchingByteTag(flowTypeTag);

    // if (flowTypeTag.Get() == FlowType::HANDSHAKE_FLOW)
    // {
    //     return;
    // }

    if (!m_dequeueTimeRecord.contains(queueId))
    {
        m_dequeueTimeRecord[queueId].push(Simulator::Now());
        m_dequeueBufferRecord[queueId].push(item->GetSize());
    }

    uint64_t waitingTime =
        Simulator::Now().GetNanoSeconds() - m_dequeueTimeRecord[queueId].back().GetNanoSeconds();
    if (waitingTime > 0)
    {
        if (m_dequeueBufferRecord[queueId].size() < NUM_PACKETS_PER_WINDOW)
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

void
DRLQueueDisc::UpdateDelay(DRLQueueId queueId, Ptr<QueueDiscItem> item)
{
    // if (flowTypeTag.Get() == FlowType::HANDSHAKE_FLOW)
    // {
    //     return;
    // }

    for (uint8_t i = DRLQueueId::DRL_QUEUE; i <= DRLQueueId::DRL_QUEUE; i++)
    {
        DRLQueueId currentQueueId = static_cast<DRLQueueId>(i);

        // Get current delay for current queue
        Time currentDelay = Time(0);

        if (currentQueueId == queueId)
        {
            Ptr<Packet> packet = item->GetPacket();

            EnqueueTimeTag enqueueTimeTag;
            packet->FindFirstMatchingByteTag(enqueueTimeTag);
            currentDelay = Simulator::Now() - enqueueTimeTag.Get();
        }
        else if (GetInternalQueue(currentQueueId)->GetNPackets() > 0)
        {
            Ptr<const QueueDiscItem> lastQueueDiscItem = GetInternalQueue(currentQueueId)->Peek();
            Ptr<Packet> lastPacket = lastQueueDiscItem->GetPacket();

            EnqueueTimeTag enqueueTimeTag;
            lastPacket->FindFirstMatchingByteTag(enqueueTimeTag);
            currentDelay = Simulator::Now() - enqueueTimeTag.Get();
        }

        // Update delay status for current queue
        if (m_delayRecord[currentQueueId].size() < NUM_PACKETS_PER_WINDOW)
        {
            m_delayRecord[currentQueueId].push(currentDelay);
            m_delaySum[currentQueueId] += currentDelay;
        }
        else
        {
            Time frontDelay = m_delayRecord[currentQueueId].front();

            m_delayRecord[currentQueueId].pop();
            m_delaySum[currentQueueId] -= frontDelay;
        }

        m_delay[currentQueueId] = m_delaySum[currentQueueId] / m_delayRecord[currentQueueId].size();
    }
}

Ptr<QueueDiscItem>
DRLQueueDisc::DequeuePacket(DRLQueueId queueId)
{
    NS_LOG_FUNCTION(this << queueId);

    Ptr<QueueDiscItem> item = GetInternalQueue(queueId)->Dequeue();
    NS_ASSERT(item != nullptr);

    // NS_LOG_UNCOND("Dequeue packet from " << BD_QUEUE_IDS[queueId]
    //                              << ", current queue size: " <<
    //                              GetInternalQueue(queueId)->GetCurrentSize()
    //                              << ", max queue size: " << m_queueBufferSize[queueId]);
    m_traceDRLQueueDequeue(this, queueId, item);

    // Update parameters
    UpdateDequeueThroughput(queueId, item);
    UpdateDelay(queueId, item);
    UpdateBandwidth(item);

    NS_LOG_LOGIC("Dequeued item from queue " << DRL_QUEUE_IDS[queueId]);
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return item;
}

void
DRLQueueDisc::DropPacket(DRLQueueId queueId, std::string reason)
{
    NS_LOG_FUNCTION(this << queueId);

    Ptr<QueueDiscItem> item = GetInternalQueue(queueId)->Dequeue();
    NS_ASSERT(item != nullptr);

    ApplicationIdTag applicationIdTag;
    item->GetPacket()->FindFirstMatchingByteTag(applicationIdTag);

    DropAfterDequeue(item, reason.c_str());
    m_traceDRLQueueDrop(this, queueId, item);

    // Update parameters
    UpdateDelay(queueId, item);

    NS_LOG_LOGIC("Dropped item from queue " << DRL_QUEUE_IDS[queueId]);
}

Ptr<QueueDiscItem>
DRLQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    DRLQueueId queueId = DRL_QUEUE;

    // Check if the queue is empty
    if (GetInternalQueue(queueId)->GetNPackets() == 0)
    {
        return nullptr;
    }

    // Dequeue/drop packet based on the drop probability
    double randomValue = (double)rand() / RAND_MAX;
    if (randomValue > dropProbability[queueId])
    {
        return DequeuePacket(queueId);
    }
    else
    {
        DropPacket(queueId, "Random drop by DRLQueueDisc");
        return nullptr;
    }
}
