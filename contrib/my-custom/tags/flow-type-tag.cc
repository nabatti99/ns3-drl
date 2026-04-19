#include "flow-type-tag.h"

using namespace ns3;

FlowTypeTag::FlowTypeTag()
{
}

FlowTypeTag::~FlowTypeTag()
{
}

TypeId
FlowTypeTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::FlowTypeTag")
                            .SetParent<Tag>()
                            .SetGroupName("Applications")
                            .AddConstructor<FlowTypeTag>()
                            .AddAttribute("FlowType",
                                          "Flow Type value",
                                          EmptyAttributeValue(),
                                          MakeUintegerAccessor(&FlowTypeTag::m_type),
                                          MakeUintegerChecker<uint32_t>());
    return tid;
}

TypeId
FlowTypeTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
FlowTypeTag::GetSerializedSize() const
{
    return sizeof(uint32_t);
}

void
FlowTypeTag::Serialize(TagBuffer i) const
{
    i.WriteU32(static_cast<uint32_t>(m_type));
}

void
FlowTypeTag::Deserialize(TagBuffer i)
{
    m_type = static_cast<FlowType>(i.ReadU32());
}

void
FlowTypeTag::Print(std::ostream& os) const
{
    os << "Flow Type=" << FLOW_TYPES[static_cast<uint32_t>(m_type)];
}

FlowType
FlowTypeTag::Get(void) const
{
    return m_type;
}

void
FlowTypeTag::Set(FlowType flowType)
{
    m_type = flowType;
}