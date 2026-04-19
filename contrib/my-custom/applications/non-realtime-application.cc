#include "non-realtime-application.h"

#include "../tags/application-id-tag.h"
#include "../tags/flow-type-tag.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NonRealtimeApplication");
NS_OBJECT_ENSURE_REGISTERED(NonRealtimeApplication);

TypeId
NonRealtimeApplication::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::NonRealtimeApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<NonRealtimeApplication>()
            .AddAttribute("Socket",
                          "current socket to send packet",
                          PointerValue(nullptr),
                          MakePointerAccessor(&NonRealtimeApplication::m_socket),
                          MakePointerChecker<Socket>())
            .AddAttribute("TargetAddress",
                          "The Ip address this application send packet to",
                          AddressValue(InetSocketAddress("0.0.0.0", 0)),
                          MakeAddressAccessor(&NonRealtimeApplication::m_peer),
                          MakeAddressChecker())
            .AddAttribute("PacketSize",
                          "The packet size for each packet send",
                          UintegerValue(1500),
                          MakeUintegerAccessor(&NonRealtimeApplication::m_packetSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("DataRate",
                          "The send packet rate of this application",
                          DataRateValue(DataRate("5Mbps")),
                          MakeDataRateAccessor(&NonRealtimeApplication::m_dataRate),
                          MakeDataRateChecker())
            .AddAttribute("SourceId",
                          "The ID of this application",
                          UintegerValue(0),
                          MakeUintegerAccessor(&NonRealtimeApplication::m_sourceid),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

NonRealtimeApplication::NonRealtimeApplication()
    : m_running(false),
      m_packetsSent(0)
{
    NS_LOG_FUNCTION(this);
}

NonRealtimeApplication::~NonRealtimeApplication()
{
    NS_LOG_FUNCTION(this);
}

void
NonRealtimeApplication::Setup(Ptr<Socket> socket,
                                    Address address,
                                    uint32_t packetSize,
                                    DataRate dataRate,
                                    uint32_t sourceid,
                                    bool poisson)
{
    NS_LOG_FUNCTION(this);

    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_dataRate = dataRate;
    m_sourceid = sourceid;

    m_var = CreateObject<ExponentialRandomVariable>();
    // No need for poisson as the demand is infinite anyway and the packet gap pattern is determined
    // by the underlying transport
}

void
NonRealtimeApplication::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    m_running = true;
    m_packetsSent = 0;

    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void
NonRealtimeApplication::StopApplication(void)
{
    NS_LOG_FUNCTION(this);

    m_running = false;

    if (m_sendEvent.IsPending())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void
NonRealtimeApplication::SendPacket(void)
{
    NS_LOG_FUNCTION(this);

    Ptr<Packet> packet = Create<Packet>(m_packetSize);

    ApplicationIdTag applicationIdTag;
    applicationIdTag.SetAttribute("ApplicationId", UintegerValue(m_sourceid));
    packet->AddByteTag(applicationIdTag);


    FlowTypeTag flowTypeTag;
    flowTypeTag.SetAttribute("FlowType", UintegerValue(NON_REALTIME_FLOW));
    packet->AddByteTag(flowTypeTag);

    m_socket->Send(packet);

    ++m_packetsSent;
    ScheduleTx();
}

void
NonRealtimeApplication::ScheduleTx(void)
{
    NS_LOG_FUNCTION(this);

    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &NonRealtimeApplication::SendPacket, this);
    }
}

uint32_t
NonRealtimeApplication::GetPacketsSent(void)
{
    NS_LOG_FUNCTION(this);

    return m_packetsSent;
}
