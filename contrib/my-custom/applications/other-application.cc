#include "other-application.h"

#include "../tags/application-id-tag.h"
#include "../tags/flow-type-tag.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OtherApplication");
NS_OBJECT_ENSURE_REGISTERED(OtherApplication);

TypeId
OtherApplication::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::OtherApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<OtherApplication>()
            .AddAttribute("Socket",
                          "current socket to send packet",
                          PointerValue(nullptr),
                          MakePointerAccessor(&OtherApplication::m_socket),
                          MakePointerChecker<Socket>())
            .AddAttribute("TargetAddress",
                          "The Ip address this application send packet to",
                          AddressValue(InetSocketAddress("0.0.0.0", 0)),
                          MakeAddressAccessor(&OtherApplication::m_peer),
                          MakeAddressChecker())
            .AddAttribute("PacketSize",
                          "The packet size for each packet send",
                          UintegerValue(1500),
                          MakeUintegerAccessor(&OtherApplication::m_packetSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("DataRate",
                          "The send packet rate of this application",
                          DataRateValue(DataRate("5Mbps")),
                          MakeDataRateAccessor(&OtherApplication::m_dataRate),
                          MakeDataRateChecker())
            .AddAttribute("SourceId",
                          "The ID of this application",
                          UintegerValue(0),
                          MakeUintegerAccessor(&OtherApplication::m_sourceid),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

OtherApplication::OtherApplication()
    : m_running(false),
      m_packetsSent(0)
{
    NS_LOG_FUNCTION(this);
}

OtherApplication::~OtherApplication()
{
    NS_LOG_FUNCTION(this);
}

void
OtherApplication::Setup(Ptr<Socket> socket,
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
OtherApplication::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    m_running = true;
    m_packetsSent = 0;

    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void
OtherApplication::StopApplication(void)
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
OtherApplication::SendPacket(void)
{
    NS_LOG_FUNCTION(this);

    Ptr<Packet> packet = Create<Packet>(m_packetSize);

    ApplicationIdTag applicationIdTag;
    applicationIdTag.SetAttribute("ApplicationId", UintegerValue(m_sourceid));
    packet->AddByteTag(applicationIdTag);


    FlowTypeTag flowTypeTag;
    flowTypeTag.SetAttribute("FlowType", UintegerValue(OTHER_FLOW));
    packet->AddByteTag(flowTypeTag);

    m_socket->Send(packet);

    ++m_packetsSent;
    ScheduleTx();
}

void
OtherApplication::ScheduleTx(void)
{
    NS_LOG_FUNCTION(this);

    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &OtherApplication::SendPacket, this);
    }
}

uint32_t
OtherApplication::GetPacketsSent(void)
{
    NS_LOG_FUNCTION(this);

    return m_packetsSent;
}
