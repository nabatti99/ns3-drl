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

    void SendPacket() override;

  private:
    bool m_isRunning{false};

    uint32_t m_sourceid;
    FlowType m_flowType;

    // Tracing properties
    EventId m_sendEvent;
};

#endif // MY_APPLICATION_H
