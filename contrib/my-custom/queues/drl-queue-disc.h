#ifndef DRL_QUEUE_DISC_H
#define DRL_QUEUE_DISC_H

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

enum DRLQueueId
{
    DRL_QUEUE,
};

static const std::string DRL_QUEUE_IDS[] = {"DRL_QUEUE"};

class DRLQueueDisc : public QueueDisc
{
  public:
    static TypeId GetTypeId();
    DRLQueueDisc();
    ~DRLQueueDisc() override;

    // Base function defined by class QueueDisc.
    virtual bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    virtual Ptr<QueueDiscItem> DoDequeue() override;
    virtual bool CheckConfig() override;
    virtual void InitializeParams() override;

    // ===QUEUE HYPER PARAMETERS===

    default_map<DRLQueueId, double> dropProbability{
        {DRLQueueId::DRL_QUEUE, 0.0},
    };

    // ===QUEUE BEHAVIOR===

    virtual Ptr<QueueDiscItem> DequeuePacket(DRLQueueId queueId);

    virtual void DropPacket(DRLQueueId queueId, std::string reason);

    // ===QUEUE STATUS===

    default_map<DRLQueueId, QueueSize> m_minQueueSize = {
        {DRLQueueId::DRL_QUEUE, QueueSize("50p")},
    };
    default_map<DRLQueueId, QueueSize> m_maxQueueSize = {
        {DRLQueueId::DRL_QUEUE, QueueSize("1000p")},
    };

    // Record the enqueue throughput for each queue
    default_map<DRLQueueId, std::queue<Time>> m_enqueueTimeRecord{0};
    default_map<DRLQueueId, std::queue<uint64_t>> m_enqueueBufferRecord{0};
    default_map<DRLQueueId, uint64_t> m_enqueueWaitingTimeSum{0};
    default_map<DRLQueueId, uint64_t> m_enqueueBufferSum{0};
    default_map<DRLQueueId, DataRate> m_enqueueThroughput{DataRate(0)}; // bps

    void UpdateEnqueueThroughput(DRLQueueId queueId, Ptr<QueueDiscItem> item);

    // Record the dequeue throughput for each queue
    default_map<DRLQueueId, std::queue<Time>> m_dequeueTimeRecord{0};
    default_map<DRLQueueId, std::queue<uint64_t>> m_dequeueBufferRecord{0};
    default_map<DRLQueueId, uint64_t> m_dequeueWaitingTimeSum{0};
    default_map<DRLQueueId, uint64_t> m_dequeueBufferSum{0};
    default_map<DRLQueueId, DataRate> m_dequeueThroughput{DataRate(0)}; // bps

    void UpdateDequeueThroughput(DRLQueueId queueId, Ptr<QueueDiscItem> item);

    // Record the delay time for each queue
    default_map<DRLQueueId, std::queue<Time>> m_delayRecord{0};
    default_map<DRLQueueId, Time> m_delaySum{0};
    default_map<DRLQueueId, Time> m_delay{0};

    void UpdateDelay(DRLQueueId queueId, Ptr<QueueDiscItem> item);

    // Record the bandwidth of this router
    std::queue<Time> m_bandwidthTimeRecord{};
    std::queue<uint64_t> m_bandwidthBufferRecord{};
    Time m_bandwidthWaitingTimeSum{0};
    uint64_t m_bandwidthBufferSum{0};
    DataRate m_bandwidth{0}; // bps

    void UpdateBandwidth(Ptr<QueueDiscItem> item);

    const uint32_t NUM_PACKETS_PER_WINDOW = 10000;

    const Time TIME_UNIT = Time("1ms");

    // ===TRACING===
    /// Traced callback: fired when a packet is enqueued
    TracedCallback<Ptr<DRLQueueDisc>, DRLQueueId, Ptr<QueueDiscItem>> m_traceDRLQueueEnqueue;
    /// Traced callback: fired when a packet is dequeued
    TracedCallback<Ptr<DRLQueueDisc>, DRLQueueId, Ptr<QueueDiscItem>> m_traceDRLQueueDequeue;
    /// Traced callback: fired when a packet is dropped
    TracedCallback<Ptr<DRLQueueDisc>, DRLQueueId, Ptr<QueueDiscItem>> m_traceDRLQueueDrop;
};

#endif // DRL_QUEUE_DISC_H