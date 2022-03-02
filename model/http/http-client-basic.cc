/*
 * basic-http-client.cc
 *
 *  Created on: 06-Apr-2020
 *      Author: abhijit
 */

#include "http-client-basic.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/tcp-socket.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/uinteger.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE("HttpClientBasic");
NS_OBJECT_ENSURE_REGISTERED(HttpClientBasic);

#define DOWNLOADSIZE_EXP 34567

TypeId HttpClientBasic::GetTypeId(void)
{
	static TypeId tid = TypeId("ns3::HttpClientBasic")
				.SetParent<Application>()
				.SetGroupName("Applications")
				.AddConstructor<HttpClientBasic>()
				.AddAttribute("Timeout",
						"Timeout",
						TimeValue(Seconds(30)),
						MakeTimeAccessor(&HttpClientBasic::m_timeout),
						MakeTimeChecker())
        .AddAttribute("Retries",
            "Number of retry",
            UintegerValue(3),
            MakeUintegerAccessor(&HttpClientBasic::m_retries),
            MakeUintegerChecker<uint16_t>());
	return tid;
}

HttpClientBasic::HttpClientBasic ()
    :
    m_peerPort (0), m_retries(3), m_numTried(0)
{
  m_method = "GET";
  m_path = "/";
}

HttpClientBasic::~HttpClientBasic ()
{
}

void
HttpClientBasic::RecvResponseData (uint8_t *data, uint32_t len)
{
}

void
HttpClientBasic::RecvResponseHeader ()
{
}

void
HttpClientBasic::InitConnection (Address peerAddress, uint16_t peerPort, std::string path)
{
  NS_LOG_FUNCTION(this);
  m_peerAddress = peerAddress;
  m_peerPort = peerPort;
  m_path = path;
  InitConnection ();
}

void
HttpClientBasic::InitConnection (std::string path)
{
  NS_LOG_FUNCTION(this);

  if (m_socket != 0)
    {
      m_socket = 0;
    }

  if (path.length () != 0)
    {
      m_path = path;
    }

  m_request = Create<HttpRequest> (m_method, m_path, "http1.1");
  m_response = Create<HttpResponse> ();
}

void
HttpClientBasic::Connect ()
{
  m_trace = HttpTrace ();
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (m_node, tid);

  m_socket->SetConnectCallback (
      MakeCallback (&HttpClientBasic::EvConnectionSucceeded, this),
      MakeCallback (&HttpClientBasic::EvConnectionFailed, this));
  m_socket->SetRecvCallback (MakeCallback (&HttpClientBasic::EvHandleRecv, this));
  m_socket->SetSendCallback (MakeCallback (&HttpClientBasic::EvHandleSend, this));
  m_socket->SetCloseCallbacks (MakeCallback (&HttpClientBasic::EvSocketClosed, this),
                               MakeCallback (&HttpClientBasic::EvErrorClosed, this));

  if (Ipv4Address::IsMatchingType (m_peerAddress) == true)
    {
      m_socket->Connect (
          InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress) == true)
    {
      m_socket->Connect (
          Inet6SocketAddress (Ipv6Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
}

void
HttpClientBasic::StopConnection ()
{
  StopConnectionPriv ();
  EndErrorConnection ();
}

void
HttpClientBasic::RetryConnection ()
{
  Address addr;
  m_socket->GetSockName(addr);
  InetSocketAddress iaddr = InetSocketAddress::ConvertFrom(addr);
  StopConnectionPriv ();
  m_numTried += 1;
  if (m_retries <= m_numTried)
    {
      EndErrorConnection ();
      return;
    }
  std::cout << "########## " << m_node->GetId() << " RETRYING " << m_numTried << "/" << m_retries << " " << iaddr.GetIpv4() << ":" << iaddr.GetPort() << " ##########" << std::endl;
  Connect();
}

void
HttpClientBasic::StopConnectionPriv ()
{
  if (m_socket)
    {
      m_socket->SetConnectCallback (MakeNullCallback<void, Ptr<Socket> > (),
                                    MakeNullCallback<void, Ptr<Socket> > ());
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket->SetSendCallback (MakeNullCallback<void, Ptr<Socket>, uint32_t> ());
      m_socket->SetCloseCallbacks (MakeNullCallback<void, Ptr<Socket> > (),
                                   MakeNullCallback<void, Ptr<Socket> > ());
      m_socket->Close ();
    }
}

void
HttpClientBasic::SetCollectionCB (Callback<void> onConnectionClosed,
                                  Callback<void> onConnectionErrorClosed, Ptr<Node> node)
{
  m_onConnectionClosed = onConnectionClosed;
  m_onConnectionErrorClosed = onConnectionErrorClosed;
  m_node = node;
}

void
HttpClientBasic::AddReqHeader (std::string name, std::string value)
{
  m_request->AddHeader (name, value);
}

void
HttpClientBasic::SetRetries (uint16_t numRetries)
{
  m_retries = numRetries;
}

void
HttpClientBasic::EndConnection ()
{
  if (!m_onConnectionClosed.IsNull ())
    {
      m_onConnectionClosed ();
    }
}

void
HttpClientBasic::EndErrorConnection ()
{
  //  StopConnectionPriv();
  if (!m_onConnectionErrorClosed.IsNull ())
    {
      m_onConnectionErrorClosed ();
    }
}

const Ptr<HttpResponse>&
HttpClientBasic::GetResponse () const
{
  return m_response;
}

const HttpTrace&
HttpClientBasic::GetTrace () const
{
  return m_trace;
}

//===================
void
HttpClientBasic::EvConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  NS_LOG_LOGIC("Http Client connection failed");
  std::cout << "Connection Failed" << std::endl;
  m_timeoutEvent.Cancel ();
  Simulator::ScheduleNow(&HttpClientBasic::RetryConnection, this);
}

void
HttpClientBasic::EvConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  EvHandleSend (socket, socket->GetTxAvailable ());
  //  std::cout << "Connection Succeeded" << std::endl;
  m_timeoutEvent.Cancel ();
}

void
HttpClientBasic::EvErrorClosed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  NS_LOG_LOGIC("Http Client connection error closed");
  std::cout << "Error Closed" << std::endl;
  m_timeoutEvent.Cancel ();
  Simulator::ScheduleNow(&HttpClientBasic::RetryConnection, this);
}

void
HttpClientBasic::EvHandleRecv (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);

  uint32_t rxAvailable = m_socket->GetRxAvailable ();
  if (rxAvailable == 0) return;

  uint8_t *buf = new uint8_t[rxAvailable];
  uint32_t rlen = m_socket->Recv (buf, rxAvailable, 0);
  NS_ASSERT(rxAvailable == rlen);
  m_trace.ResponseRecv (rlen);

  if (!m_response->IsHeaderReceived ())
    {
      m_response->ParseHeader (buf, rlen);

      if (!m_response->IsHeaderReceived ())
        {
          goto cleanup;
        }
      RecvResponseHeader ();
      rlen = m_response->ReadBody (buf, sizeof(buf));

    }
  if (rlen > 0) RecvResponseData (buf, rlen);
  cleanup: delete[] buf;
  m_timeoutEvent.Cancel ();
  if (m_timeout > Seconds (0)) m_timeoutEvent = Simulator::Schedule (
      m_timeout, &HttpClientBasic::EvTimeoutOccur, this);
}

void
HttpClientBasic::EvHandleSend (Ptr<Socket> socket, uint32_t bufAvailable)
{
  NS_LOG_FUNCTION(this << socket);

  m_trace.RequestSent ();

  uint8_t buf[2048];
  uint32_t toSend = std::min (bufAvailable, (uint32_t) sizeof(buf));

  uint32_t len = m_request->ReadHeader (buf, toSend);
  if (len > 0)
    {
      uint32_t sent = m_socket->Send (buf, len, 0);
      NS_ASSERT(sent == len);
    }
  m_timeoutEvent.Cancel ();
  if (m_timeout > Seconds (0)) m_timeoutEvent = Simulator::Schedule (
      m_timeout, &HttpClientBasic::EvTimeoutOccur, this);
}

void
HttpClientBasic::EvSocketClosed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  NS_LOG_LOGIC("Http Client connection closed");
  //	std::cout << "Success Closed" << std::endl;
  m_timeoutEvent.Cancel ();
  EndConnection ();
}

void
HttpClientBasic::EvTimeoutOccur ()
{
  Simulator::ScheduleNow(&HttpClientBasic::RetryConnection, this);
  std::cout << "Timeout occured" << std::endl;
  //  EndErrorConnection();
}

//============================================================

void
HttpTrace::RequestSent ()
{
  m_reqSentAt = Simulator::Now ();
}

void
HttpTrace::ResponseRecv (uint32_t len)
{

  if (m_firstByteAt.IsZero ()) m_firstByteAt = Simulator::Now ();

  auto now = Simulator::Now ();
  if (m_trace.size () > 1)
    {
      m_speed = (len * 8 * 1.0e6)
          / (now.GetMicroSeconds () - m_lastByteAt.GetMicroSeconds ());
    }
  m_lastByteAt = now;
  m_resLen += len;
  std::pair<Time, uint64_t> entry (Simulator::Now (), m_resLen);

  m_trace.push_back (entry);
}

void
HttpTrace::StoreInFile (std::ostream &outFile)
{
  outFile << "{";
  outFile << "\"reqSentAt\":" << m_reqSentAt.GetSeconds () << ",";
  outFile << "\"firstByteAt\":" << m_firstByteAt.GetSeconds () << ",";
  outFile << "\"lastByteAt\":" << m_lastByteAt.GetSeconds () << ",";
  outFile << "\"trace\": [";
  std::string separator = "";
  for (auto it : m_trace)
    {
      outFile << separator << "[";
      outFile << it.first.GetSeconds () << "," << it.second;
      outFile << "]";
      separator = ",";
    }
  outFile << "]";
  outFile << "}";
}
double
HttpTrace::GetDownloadSpeed () const
{
  return m_speed;
}

const Time&
HttpClientBasic::GetTimeout () const
{
  return m_timeout;
}

void
HttpClientBasic::SetTimeout (const Time &mTimeout)
{
  m_timeout = mTimeout;
}

} /* namespace ns3 */
