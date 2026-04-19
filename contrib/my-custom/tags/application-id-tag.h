#ifndef APPLICATION_ID_TAG_H
#define APPLICATION_ID_TAG_H

#include "ns3/packet.h"
#include "ns3/tag.h"
#include "ns3/uinteger.h"

using namespace ns3;

class ApplicationIdTag : public Tag
{
  public:
    ApplicationIdTag();
    ~ApplicationIdTag();

    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const;
    virtual uint32_t GetSerializedSize() const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream& os) const;

    uint32_t Get() const;

    // Indicate the packet not belong to any application.
    // E.g. It can be a handshake packet.
    // Value: 999999999
    static const uint32_t NOT_APPLICATION_ID = 1e9 - 1; 

  private:
    uint32_t m_id = NOT_APPLICATION_ID;
};

#endif // APPLICATION_ID_TAG_H