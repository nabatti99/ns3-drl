#ifndef ENQUEUE_TIME_TAG_H
#define ENQUEUE_TIME_TAG_H

#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/tag.h"

using namespace ns3;

class EnqueueTimeTag : public Tag
{
  public:
    EnqueueTimeTag();
    ~EnqueueTimeTag();

    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const;
    virtual uint32_t GetSerializedSize() const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream& os) const;

    Time Get() const;

  private:
    Time m_time{0};
};

#endif // ENQUEUE_TIME_TAG_H