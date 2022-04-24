#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "bulk-send-pias-application.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BulkSendPiasApplication");

NS_OBJECT_ENSURE_REGISTERED (BulkSendPiasApplication);

TypeId
BulkSendPiasApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BulkSendPiasApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<BulkSendPiasApplication> ()
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&BulkSendPiasApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&BulkSendPiasApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendPiasApplication::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DelayThresh",
                   "How many packets can pass before we have delay, 0 for disable",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendPiasApplication::m_delayThresh),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DelayTime",
                   "The time for a delay",
                   TimeValue (MicroSeconds (100)),
                   MakeTimeAccessor (&BulkSendPiasApplication::m_delayTime),
                   MakeTimeChecker())
    .AddAttribute ("DelayClass",
                   "Which delay group the flow should be in",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendPiasApplication::m_delayClass),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&BulkSendPiasApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&BulkSendPiasApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


BulkSendPiasApplication::BulkSendPiasApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0),
    m_isDelay (false),
    m_accumPackets (0)
{
  NS_LOG_FUNCTION (this);
}

BulkSendPiasApplication::~BulkSendPiasApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
BulkSendPiasApplication::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
BulkSendPiasApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
BulkSendPiasApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void BulkSendPiasApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                          "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind ();
        }

      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&BulkSendPiasApplication::ConnectionSucceeded, this),
        MakeCallback (&BulkSendPiasApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&BulkSendPiasApplication::DataSend, this));
    }
  if (m_connected)
    {
      SendData ();
    }
}

void BulkSendPiasApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("BulkSendPiasApplication found null socket to close in StopApplication");
    }
}


// Private helpers

void BulkSendPiasApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      if (m_isDelay)
        {
          break;
        }

      // Time to send more
      uint32_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (m_sendSize, m_maxBytes - m_totBytes);
        }
      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      SocketIpTosTag tosTag;
      tosTag.SetTos (m_tos << 2);
      packet->AddPacketTag (tosTag);
      m_txTrace (packet);
      int actual = m_socket->Send (packet);
      if (actual > 0)
        {
          m_totBytes += actual;
          m_accumPackets ++;
        }

      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed ip.
      if ((unsigned)actual != toSend)
        {
          break;
        }

      if (m_delayThresh != 0 && m_accumPackets > m_delayThresh)
        {
          m_isDelay = true;
          Simulator::Schedule (m_delayTime, &BulkSendPiasApplication::ResumeSend, this);
        }
    }
  // Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && m_connected)
    {
      m_socket->Close ();
      m_connected = false;
    }
}

void BulkSendPiasApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendPiasApplication Connection succeeded");
  m_connected = true;
  SendData ();
}

void BulkSendPiasApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendPiasApplication, Connection Failed");
}

void BulkSendPiasApplication::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      SendData ();
    }
}

void BulkSendPiasApplication::ResumeSend (void)
{
    NS_LOG_FUNCTION (this);

    m_isDelay = false;
    m_accumPackets = 0;

    if (m_connected)
    {
        SendData ();
    }
}

} // Namespace ns3
