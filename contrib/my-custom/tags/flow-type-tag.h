#ifndef FLOW_TYPE_TAG_H
#define FLOW_TYPE_TAG_H

#include "ns3/packet.h"
#include "ns3/tag.h"
#include "ns3/uinteger.h"

using namespace ns3;

enum FlowType
{
    HANDSHAKE_FLOW,
    REALTIME_FLOW,
    NON_REALTIME_FLOW,
    OTHER_FLOW
};

static const std::string FLOW_TYPES[] = {"HANDSHAKE_FLOW",
                                         "REALTIME_FLOW",
                                         "NON_REALTIME_FLOW",
                                         "OTHER_FLOW"};

class FlowTypeTag : public Tag
{
  public:
    FlowTypeTag();
    ~FlowTypeTag();

    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const;
    virtual uint32_t GetSerializedSize() const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream& os) const;

    FlowType Get() const;
    void Set(FlowType flowType);

  private:
    FlowType m_type = FlowType::HANDSHAKE_FLOW;
};

#endif // FLOW_TYPE_TAG_H