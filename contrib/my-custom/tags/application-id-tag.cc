#include "application-id-tag.h"

using namespace ns3;

ApplicationIdTag::ApplicationIdTag()
{
}

ApplicationIdTag::~ApplicationIdTag()
{
}

TypeId
ApplicationIdTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ApplicationIdTag")
                            .SetParent<Tag>()
                            .SetGroupName("Applications")
                            .AddConstructor<ApplicationIdTag>()
                            .AddAttribute("ApplicationId",
                                          "Application Id value",
                                          EmptyAttributeValue(),
                                          MakeUintegerAccessor(&ApplicationIdTag::m_id),
                                          MakeUintegerChecker<uint32_t>());
    return tid;
}

TypeId
ApplicationIdTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
ApplicationIdTag::GetSerializedSize() const
{
    return sizeof(uint32_t);
}

void
ApplicationIdTag::Serialize(TagBuffer i) const
{
    i.WriteU32(m_id);
}

void
ApplicationIdTag::Deserialize(TagBuffer i)
{
    m_id = i.ReadU32();
}

void
ApplicationIdTag::Print(std::ostream& os) const
{
    os << "Id=" << (uint32_t)m_id;
}

uint32_t
ApplicationIdTag::Get() const
{
    return m_id;
}