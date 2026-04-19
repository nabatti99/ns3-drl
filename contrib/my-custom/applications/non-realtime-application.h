#ifndef NON_REALTIME_APPLICATION_H
#define NON_REALTIME_APPLICATION_H

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

class NonRealtimeApplication : public Application
{
  public:
    static TypeId GetTypeId(void);

    NonRealtimeApplication();
    ~NonRealtimeApplication() override;

    // Default OnOffApp creates socket until app start time
    // Can't access and configure the tracing externally
    // => Need a new application for customize tracing
    void Setup(Ptr<Socket> socket,
               Address address,
               uint32_t packetSize,
               DataRate dataRate,
               uint32_t appid,
               bool poisson);

    uint32_t GetPacketsSent();

  private:
    void StartApplication(void) override;
    void StopApplication(void) override;

    void ScheduleTx(void);
    void SendPacket(void);

    bool m_running;
    uint32_t m_sourceid;
    Ptr<ExponentialRandomVariable> m_var;

    Ptr<Socket> m_socket;  // Current socket to send packet
    Address m_peer;        // The Ip address this application send packet to
    uint32_t m_packetSize; // The packet size for each packet send
    DataRate m_dataRate;   // The send packet rate of this application

    // Tracing properties
    EventId m_sendEvent;
    uint32_t m_packetsSent;
};

#endif // NON_REALTIME_APPLICATION_H
