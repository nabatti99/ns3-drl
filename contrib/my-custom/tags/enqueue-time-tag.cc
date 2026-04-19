#include "enqueue-time-tag.h"

using namespace ns3;

EnqueueTimeTag::EnqueueTimeTag()
{
}

EnqueueTimeTag::~EnqueueTimeTag()
{
}

TypeId
EnqueueTimeTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::EnqueueTimeTag")
                            .SetParent<Tag>()
                            .SetGroupName("Tags")
                            .AddConstructor<EnqueueTimeTag>()
                            .AddAttribute("EnqueueTime",
                                          "Enqueue Time value",
                                          EmptyAttributeValue(),
                                          MakeTimeAccessor(&EnqueueTimeTag::m_time),
                                          MakeTimeChecker());
    return tid;
}

TypeId
EnqueueTimeTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
EnqueueTimeTag::GetSerializedSize() const
{
    return sizeof(int64_t);
}

void
EnqueueTimeTag::Serialize(TagBuffer i) const
{
    i.WriteU64(m_time.GetNanoSeconds());
}

void
EnqueueTimeTag::Deserialize(TagBuffer i)
{
    m_time = NanoSeconds(i.ReadU64());
}

void
EnqueueTimeTag::Print(std::ostream& os) const
{
    os << "SendTime=" << m_time.GetSeconds() << "s";
}

Time
EnqueueTimeTag::Get() const
{
    return m_time;
}