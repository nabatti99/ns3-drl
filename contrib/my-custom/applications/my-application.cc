#include "my-application.h"

#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MyApplication");
NS_OBJECT_ENSURE_REGISTERED(MyApplication);

TypeId
MyApplication::GetTypeId()
{
    static TypeId tid = TypeId("ns3::MyApplication")
                            .SetParent<OnOffApplication>()
                            .SetGroupName("Applications")
                            .AddConstructor<MyApplication>()
                            .AddAttribute("SourceId",
                                          "The ID of this application",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&MyApplication::m_sourceid),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("FlowType",
                                          "The type of flow for this application",
                                          UintegerValue(FlowType::REALTIME_FLOW),
                                          MakeUintegerAccessor(&MyApplication::m_flowType),
                                          MakeUintegerChecker<uint8_t>())
                            .AddAttribute("IsRunning",
                                          "Whether the application is running",
                                          BooleanValue(true),
                                          MakeBooleanAccessor(&MyApplication::m_isRunning),
                                          MakeBooleanChecker());

    return tid;
}

void
MyApplication::SendPacket()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT(m_sendEvent.IsExpired());

    Ptr<Packet> packet;
    if (m_unsentPacket)
    {
        packet = m_unsentPacket;
    }
    else
    {
        if (m_enableSeqTsSizeHeader)
        {
            Address from;
            Address to;
            m_socket->GetSockName(from);
            m_socket->GetPeerName(to);
            SeqTsSizeHeader header;
            header.SetSeq(m_seq++);
            header.SetSize(m_pktSize);
            NS_ABORT_IF(m_pktSize < header.GetSerializedSize());
            packet = Create<Packet>(m_pktSize - header.GetSerializedSize());
            // Trace before adding header, for consistency with PacketSink
            m_txTraceWithSeqTsSize(packet, from, to, header);
            packet->AddHeader(header);
        }
        else
        {
            packet = Create<Packet>(m_pktSize);
        }

        NS_LOG_LOGIC("New packet uid: " << packet->GetUid());

        // Add tags for application packet
        // NOTE: This is not apply for the handshake packets
        ApplicationIdTag applicationIdTag;
        applicationIdTag.SetAttribute("ApplicationId", UintegerValue(m_sourceid));
        packet->AddByteTag(applicationIdTag);

        FlowTypeTag flowTypeTag;
        flowTypeTag.SetAttribute("FlowType", UintegerValue(m_flowType));
        packet->AddByteTag(flowTypeTag);
    }

    int actual = 0;
    if (m_isRunning)
    {
        actual = m_socket->Send(packet);
    }

    // NS_LOG_UNCOND("Actual send size: " << actual << ", Packet uid: " << packet->GetUid());
    if ((unsigned)actual == m_pktSize)
    {
        m_txTrace(packet);
        m_totBytes += m_pktSize;
        m_unsentPacket = nullptr;
        Address localAddress;
        m_socket->GetSockName(localAddress);
        if (InetSocketAddress::IsMatchingType(m_peer))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " my application sent "
                                   << packet->GetSize() << " bytes to "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetPort()
                                   << " total Tx " << m_totBytes << " bytes");
            m_txTraceWithAddresses(packet, localAddress, InetSocketAddress::ConvertFrom(m_peer));
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peer))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " my application sent "
                                   << packet->GetSize() << " bytes to "
                                   << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6() << " port "
                                   << Inet6SocketAddress::ConvertFrom(m_peer).GetPort()
                                   << " total Tx " << m_totBytes << " bytes");
            m_txTraceWithAddresses(packet, localAddress, Inet6SocketAddress::ConvertFrom(m_peer));
        }
    }
    else
    {
        NS_LOG_DEBUG("Unable to send packet; actual " << actual << " size " << m_pktSize
                                                      << "; caching for later attempt");
        m_unsentPacket = packet;
    }
    m_residualBits = 0;
    m_lastStartTime = Simulator::Now();
    ScheduleNextTx();

} // namespace ns3
