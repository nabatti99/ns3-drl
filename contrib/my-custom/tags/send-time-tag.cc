#include "send-time-tag.h"

using namespace ns3;

SendTimeTag::SendTimeTag()
{
}

SendTimeTag::~SendTimeTag()
{
}

TypeId
SendTimeTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SendTimeTag")
                            .SetParent<Tag>()
                            .SetGroupName("Tags")
                            .AddConstructor<SendTimeTag>()
                            .AddAttribute("SendTime",
                                          "Send Time value",
                                          EmptyAttributeValue(),
                                          MakeTimeAccessor(&SendTimeTag::m_time),
                                          MakeTimeChecker());
    return tid;
}

TypeId
SendTimeTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
SendTimeTag::GetSerializedSize() const
{
    return sizeof(int64_t);
}

void
SendTimeTag::Serialize(TagBuffer i) const
{
    i.WriteU64(m_time.GetNanoSeconds());
}

void
SendTimeTag::Deserialize(TagBuffer i)
{
    m_time = NanoSeconds(i.ReadU64());
}

void
SendTimeTag::Print(std::ostream& os) const
{
    os << "SendTime=" << m_time.GetSeconds() << "s";
}

Time
SendTimeTag::Get() const
{
    return m_time;
}