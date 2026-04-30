#ifndef MY_APPLICATION_H
#define MY_APPLICATION_H

#include "../tags/application-id-tag.h"
#include "../tags/flow-type-tag.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

class MyApplication : public OnOffApplication
{
  public:
    static TypeId GetTypeId();

    MyApplication() = default;
    ~MyApplication() = default;

    // Adjust the packet size based on the packet header size.
    void AdjustPacketSize(uint32_t packetHeaderSize);

    // Adjust the sending rate based on the packet header size.
    // This is to ensure that the actual sending rate of the application is consistent with the
    // bandwidth specified by the user, even when the packet header size changes.
    void AdjustSendingRate(uint32_t packetHeaderSize);

    // Override the SendPacket method to add the custom tag to the packet before sending it.
    void SendPacket() override;

  private:
    bool m_isRunning{false};

    uint32_t m_sourceid;
    FlowType m_flowType;

    // Tracing properties
    EventId m_sendEvent;
};

#endif // MY_APPLICATION_H
