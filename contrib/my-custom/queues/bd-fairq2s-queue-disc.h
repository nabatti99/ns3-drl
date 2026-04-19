#ifndef BD_FAIRQ2S_QUEUE_DISC_H
#define BD_FAIRQ2S_QUEUE_DISC_H

#include "../helper/my-mapping.h"
#include "../tags/application-id-tag.h"
#include "../tags/enqueue-time-tag.h"
#include "../tags/flow-type-tag.h"
#include "../tags/send-time-tag.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/queue-disc.h"

using namespace ns3;

enum BDQueueId
{
    BD_DETECTION_QUEUE,
    BD_REALTIME_QUEUE,
    BD_NON_REALTIME_QUEUE,
    BD_OTHER_QUEUE,
};

static const std::string BD_QUEUE_IDS[] = {"DETECTION_QUEUE",
                                           "REALTIME_QUEUE",
                                           "NON_REALTIME_QUEUE",
                                           "OTHER_QUEUE"};

class BDFairQ2SQueueDisc : public QueueDisc
{
  public:
    static TypeId GetTypeId();
    BDFairQ2SQueueDisc();
    ~BDFairQ2SQueueDisc() override;

    // Base function defined by class QueueDisc.
    virtual bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    virtual Ptr<QueueDiscItem> DoDequeue() override;
    virtual bool CheckConfig() override;
    virtual void InitializeParams() override;

    // Selects the packet from the front of specified queue.
    virtual Ptr<QueueDiscItem> DequeuePacket(BDQueueId queueId);

    // Drops a packet from the specified queue.
    virtual void DropPacket(BDQueueId queueId, std::string reason);

    // ===FLOW STATUS===
    enum FlowStatus
    {
        NOT_STARTED,
        STARTING,
        STABLE
    };

    // Record the status of each flow.
    // This value is updated when the first run for doenqueue for each flow (distingushed by
    // source_id).
    // The value of flow_status is: source_id -> FlowStatus. Flow status inclue three
    // position:
    // 0 - This flow is not start yet.
    // 1 - This flow is just started and may be going through the slow start phase of the congestion
    // control algorithm (not to do max flow reject).
    // 2 - This flow has been started for more than a time window and we consider it to have entered
    // a stable period (maximum flow reject can be performed).
    default_map<uint32_t, FlowStatus> m_flowStatus{FlowStatus::NOT_STARTED};

    void UpdateFlowStatus(uint32_t applicationId, FlowStatus newFlowStatus);

    // To determine which queue this flow should enter by source_id and type_id,
    BDQueueId DistributeToBDQueueId(uint32_t applicationId, FlowType flowType);

    // ===ALL QUEUES===

    default_map<BDQueueId, QueueSize> m_minQueueSize = {
        {BDQueueId::BD_DETECTION_QUEUE, QueueSize("50p")},
        {BDQueueId::BD_REALTIME_QUEUE, QueueSize("50p")},
        {BDQueueId::BD_NON_REALTIME_QUEUE, QueueSize("50p")},
        {BDQueueId::BD_OTHER_QUEUE, QueueSize("50p")},
    };
    default_map<BDQueueId, QueueSize> m_maxQueueSize = {
        {BDQueueId::BD_DETECTION_QUEUE, QueueSize("1000p")},
        {BDQueueId::BD_REALTIME_QUEUE, QueueSize("1000p")},
        {BDQueueId::BD_NON_REALTIME_QUEUE, QueueSize("1000p")},
        {BDQueueId::BD_OTHER_QUEUE, QueueSize("1000p")},
    };

    default_map<BDQueueId, QueueSize> m_queueSizeThreshold{
        {BDQueueId::BD_DETECTION_QUEUE, QueueSize("1000p")},
        {BDQueueId::BD_REALTIME_QUEUE, QueueSize("500p")},
        {BDQueueId::BD_NON_REALTIME_QUEUE, QueueSize("1000p")},
        {BDQueueId::BD_OTHER_QUEUE, QueueSize("1000p")}};

    // Update the buffer size for a specific queue
    // void SetQueueSizeThreshold(BDQueueId queueId, uint32_t newSize);

    // Record the current packets count for each queue, and further record the packets count
    // for each flow (distinguided by source_id).
    // This value is updated when packets enqueue (+1) and dequeue (-1).
    // The value of queue_pkt_count include two level map,
    // the first level is: queue_id -> unorder_map, the second level is source_id -> packets count.
    default_map<BDQueueId, default_map<uint32_t, uint32_t>> m_queuePacketCount{0};

    // Obtain current flow count for each queue based on the number of keys in enqueue_throughput,
    // called by setFairBandwidthShare, used to allocate fair bandwidth of different queues.
    uint32_t GetEnqueueFlowCount(BDQueueId queueId);

    // Record the enqueue throughput for each queue
    default_map<BDQueueId, std::queue<Time>> m_enqueueTimeRecord{0};
    default_map<BDQueueId, std::queue<uint64_t>> m_enqueueBufferRecord{0};
    default_map<BDQueueId, uint64_t> m_enqueueWaitingTimeSum{0};
    default_map<BDQueueId, uint64_t> m_enqueueBufferSum{0};
    default_map<BDQueueId, DataRate> m_enqueueThroughput{DataRate(uint64_t(1) << 20)}; // bps

    void UpdateEnqueueThroughput(BDQueueId queueId, Ptr<QueueDiscItem> item);

    // Record the dequeue throughput for each queue
    default_map<BDQueueId, std::queue<Time>> m_dequeueTimeRecord{0};
    default_map<BDQueueId, std::queue<uint64_t>> m_dequeueBufferRecord{0};
    default_map<BDQueueId, uint64_t> m_dequeueWaitingTimeSum{0};
    default_map<BDQueueId, uint64_t> m_dequeueBufferSum{0};
    default_map<BDQueueId, DataRate> m_dequeueThroughput{DataRate(uint64_t(1) << 20)}; // bps

    void UpdateDequeueThroughput(BDQueueId queueId, Ptr<QueueDiscItem> item);

    // Record the bandwidth of this router
    std::queue<Time> m_bandwidthTimeRecord{};
    std::queue<uint64_t> m_bandwidthBufferRecord{};
    uint64_t m_bandwidthWaitingTimeSum = 0;
    uint64_t m_bandwidthBufferSum = 0;
    DataRate m_bandwidth = DataRate(uint64_t(1) << 20); // bps

    void UpdateBandwidth(Ptr<QueueDiscItem> item);

    const uint32_t RECEIVE_PACKET_DELTA_T_WINDOW = 50;

    // const uint64_t MAX_SCHEDULED_DEQUEUE_BUFFER = 20 * 1e3; // 20KB
    const Time TIME_UNIT = Time("1ms");

    bool m_isInDequeuePhase = false;
    default_map<BDQueueId, uint64_t> m_overflowScheduledDequeueBuffer{0};
    default_map<BDQueueId, uint64_t> m_scheduledDequeueBuffer{0};
    default_map<BDQueueId, Time> m_estimatedDequeueTime{0};

    // Drop packet interval for non-realtime queues and other queue
    // default_map<BDQueueId, Time> m_dropPacketInterval{
    //     {BDQueueId::BD_REALTIME_QUEUE, Time("1000ms")},
    //     {BDQueueId::BD_NON_REALTIME_QUEUE, Time("200ms")},
    //     {BDQueueId::BD_OTHER_QUEUE, Time("200ms")}};
    default_map<BDQueueId, Time> m_maxEstimatedDequeueTime{
        {BDQueueId::BD_REALTIME_QUEUE, Time("1ms")},
        {BDQueueId::BD_NON_REALTIME_QUEUE, Time("10ms")},
        {BDQueueId::BD_OTHER_QUEUE, Time("5ms")}};
    default_map<uint32_t, std::queue<Time>> m_dropTimeRecords{};
    default_map<uint32_t, Time> m_nextDropTime{0};

    void UpdateNextDropTime(BDQueueId queueId, uint32_t applicationId, bool isDropped);

    double m_bufferWeightAlpha = 0.5;
    double m_delayWeightAlpha = 0.5;

    Time GetQueueWaitingTime(BDQueueId queueId);

    // Delay threshold for realtime queue
    const Time REALTIME_DELAY_THRESHOLD{Time("1000us")};

    // ===TRACING===
    /// Traced callback: fired when a packet is enqueued
    TracedCallback<Ptr<BDFairQ2SQueueDisc>, BDQueueId, Ptr<QueueDiscItem>> m_traceRtqsEnqueue;
    /// Traced callback: fired when a packet is dequeued
    TracedCallback<Ptr<BDFairQ2SQueueDisc>, BDQueueId, Ptr<QueueDiscItem>> m_traceRtqsDequeue;
    /// Traced callback: fired when a packet is dropped
    TracedCallback<Ptr<BDFairQ2SQueueDisc>, BDQueueId, Ptr<QueueDiscItem>> m_traceRtqsDrop;
};

#endif // BD_FAIRQ2S_QUEUE_DISC_H