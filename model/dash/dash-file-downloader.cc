/*
 * dash-file-downloader.cc
 *
 *  Created on: 15-Apr-2020
 *      Author: abhijit
 */

#include "dash-file-downloader.h"
#include <ns3/mobility-model.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DashFileDownloader");
NS_OBJECT_ENSURE_REGISTERED(DashFileDownloader);

TypeId
DashFileDownloader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DashFileDownloader")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<DashFileDownloader> ()
          .AddAttribute ("RemoteAddress",
                         "Server address",
                         AddressValue (),
                         MakeAddressAccessor (&DashFileDownloader::m_serverAddress),
                         MakeAddressChecker ())
          .AddAttribute ("RemotePort",
                         "Port on which we listen for incoming packets.",
                         UintegerValue (9),
                         MakeUintegerAccessor (&DashFileDownloader::m_serverPort),
                         MakeUintegerChecker<uint16_t> ())
          .AddAttribute ("NumberOfDownload",
                         "Number of HTTP downloads",
                         UintegerValue (1),
                         MakeUintegerAccessor (&DashFileDownloader::m_count),
                         MakeUintegerChecker<uint16_t> ())
          .AddAttribute ("Size",
                         "Download size",
                         UintegerValue (9),
                         MakeUintegerAccessor (&DashFileDownloader::m_clen),
                         MakeUintegerChecker<clen_t> ())
          .AddAttribute ("OnStartCB",
                         "Callback for start and stop",
                         CallbackValue (MakeNullCallback<void> ()),
                         MakeCallbackAccessor (&DashFileDownloader::m_onStartClient),
                         MakeCallbackChecker ())
          .AddAttribute ("OnStopCB",
                         "Callback for start and stop",
                         CallbackValue (MakeNullCallback<void> ()),
                         MakeCallbackAccessor (&DashFileDownloader::m_onStopClient),
                         MakeCallbackChecker ())
          .AddAttribute ("TracePath",
                         "File Path to store trace",
                         StringValue (),
                         MakeStringAccessor (&DashFileDownloader::m_tracePath),
                         MakeStringChecker ())
          .AddAttribute ("NodeTracePath",
                         "Node trace provides informations like current position, speed, cellData per line",
                         StringValue (),
                         MakeStringAccessor (&DashFileDownloader::m_nodeTracePath),
                         MakeStringChecker ())
          .AddAttribute ("NodeTraceInterval",
                         "Node trace interval",
                         TimeValue (Seconds (1)),
                         MakeTimeAccessor (&DashFileDownloader::m_nodeTraceInterval),
                         MakeTimeChecker ())
          .AddAttribute ("NodeTraceHelperCallBack",
                         "NodeTrace Helper callback function collect more data than usual",
                         CallbackValue (MakeNullCallback<std::string, Ptr<Node> > ()),
                         MakeCallbackAccessor (&DashFileDownloader::m_nodeTraceCB),
                         MakeCallbackChecker ())
          .AddAttribute ("Timeout",
                         "Timeout",
                         TimeValue (Seconds (30)),
                         MakeTimeAccessor (&DashFileDownloader::m_timeout),
                         MakeTimeChecker ());
  return tid;
}

DashFileDownloader::DashFileDownloader ()
    :
    m_running (false),
    m_serverPort (0),
    m_numDownloaded (0),
    m_count (1),
    m_clen (102400),
    m_traceId (0)
{
  NS_LOG_FUNCTION(this);

}

DashFileDownloader::~DashFileDownloader ()
{
  NS_LOG_FUNCTION(this);

}

void
DashFileDownloader::StopApplication ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running) return;

  if (m_httpDownloader != 0)
    {
      m_httpDownloader->StopConnection ();
    }
}

void
DashFileDownloader::EndApplication ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running) return;
  m_running = false;
  LogTrace ();
  if (!m_onStopClient.IsNull ())
    {
      m_onStopClient ();
    }
}

void
DashFileDownloader::StartApplication ()
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT(!m_running);
  if (!m_onStartClient.IsNull ())
    {
      m_onStartClient ();
    }
  m_running = true;
  StartDash ();
  PeriodicDataCollection ();
}

void
DashFileDownloader::StartDash ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running) return;

  Ptr<Ipv4L3Protocol> ipnode = GetNode ()->GetObject<Ipv4L3Protocol> ();
  ipnode->TraceConnectWithoutContext (
      "Tx", MakeCallback (&DashFileDownloader::TxTracedCallback, this));
  ipnode->TraceConnectWithoutContext (
      "Rx", MakeCallback (&DashFileDownloader::RxTracedCallback, this));

  m_httpDownloader = Create<HttpClientBasic> ();
  m_httpDownloader->SetTimeout (m_timeout);
  m_httpDownloader->SetCollectionCB (
      MakeCallback (&DashFileDownloader::DownloadedCB, this).Bind (Ptr<Object> ()),
      MakeCallback (&DashFileDownloader::DownloadFailedCB, this).Bind (Ptr<Object> ()),
      GetNode ());

  if (m_nodeTraceCB.IsNull ()) m_nodeTraceCB = MakeCallback (
      &DashFileDownloader::CollectNodeTrace, this);
  DownloadNextSegment ();
}

void
DashFileDownloader::DownloadNextSegment ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running) return;

  if (m_node)
    {
      m_nodeTraceFp.open (
          m_nodeTracePath + "_" + std::to_string (GetNode ()->GetId ()) + "_"
          + std::to_string (m_numDownloaded) + ".csv",
          std::ofstream::out | std::ofstream::trunc);
      m_nodeTraceFp << "#,Time,rxSpeed,txSpeed," << m_nodeTraceCB (GetNode (), true)
                    << std::endl;
      m_traceId = 0;
    }
  const clen_t expected = m_clen; //Arbit
  m_httpDownloader->InitConnection (m_serverAddress, m_serverPort,
                                    "/dl-" + std::to_string (m_clen));
  m_httpDownloader->AddReqHeader ("X-Require-Length", std::to_string (expected));
  m_httpDownloader->Connect ();
}

void
DashFileDownloader::Downloaded ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running) return;
  HttpTrace trace = m_httpDownloader->GetTrace ();
  m_httpTrace.push_back (trace);
  if (m_nodeTraceFp.is_open ())
    {
      m_nodeTraceFp.close ();
    }

  m_numDownloaded += 1;
  if (m_numDownloaded < m_count) DownloadNextSegment ();
  else
    {
//		std::cout << "Finished Downloading" << std::endl;
//		m_httpDownloader = nullptr;
      EndApplication ();
    }
}

void
DashFileDownloader::DownloadedCB (Ptr<Object> obj)
{
  NS_LOG_FUNCTION(this);
  Simulator::ScheduleNow (&DashFileDownloader::Downloaded, this);
}

void DashFileDownloader::DownloadFailedCB(Ptr<Object> obj)
{
  std::cout << "Ending application due to error" << std::endl;
  Simulator::ScheduleNow(&DashFileDownloader::EndApplication, this);
}

void
DashFileDownloader::PeriodicDataCollection ()
{
  NS_LOG_FUNCTION(this);
  if (!m_nodeTraceFp.is_open ()) return;

  if (!m_running) return;

  auto node = GetNode ();
  Time now = Simulator::Now ();

  double rxSpeed = 0;
  double txSpeed = 0;
  if (m_nodeTraceLastTime != Seconds (0))
    {
      auto diff = now - m_nodeTraceLastTime;
      for (auto i : m_txData)
        {
          auto tdiff = i.second - m_txNodeTraceLastData[i.first];
          m_txNodeTraceLastData[i.first] = i.second;
          txSpeed = tdiff * 8.0 / diff.GetSeconds ();
        }
      for (auto i : m_rxData)
        {
          auto rdiff = i.second - m_rxNodeTraceLastData[i.first];
          m_rxNodeTraceLastData[i.first] = i.second;
          rxSpeed = rdiff * 8.0 / diff.GetSeconds ();
        }
    }
  m_nodeTraceLastTime = now;
  m_nodeTraceFp << m_traceId << "," << now.GetSeconds () << "," << rxSpeed << ","
                << txSpeed << "," << m_nodeTraceCB (node, false) << std::endl;
  m_traceId++;
  Simulator::Schedule (m_nodeTraceInterval, &DashFileDownloader::PeriodicDataCollection,
                       this);
}

std::string
DashFileDownloader::CollectNodeTrace (Ptr<Node> node, bool firstLine)
{
  if (firstLine) return "nodeId";
  return std::to_string (node->GetId ());
}

void
DashFileDownloader::TxTracedCallback (Ptr<const Packet> packet, Ptr<Ipv4> ipv4,
                                      uint32_t interface)
{
  m_txData[interface] += packet->GetSize ();
}

void
DashFileDownloader::RxTracedCallback (Ptr<const Packet> packet, Ptr<Ipv4> ipv4,
                                      uint32_t interface)
{
  m_rxData[interface] += packet->GetSize ();
}

void
DashFileDownloader::LogTrace ()
{
  NS_LOG_FUNCTION(this);
  if (m_tracePath.empty ()) return;

  std::ofstream outFile;
  std::string fpath = m_tracePath + "-" + std::to_string (GetNode ()->GetId ()) + ".json";
  outFile.open (fpath.c_str (), std::ofstream::out | std::ofstream::trunc);
  if (!outFile.is_open ())
    {
      std::cerr << "Can't open file " << m_tracePath << std::endl;
      return;
    }
  std::string separator = "";
  outFile << "[" << std::endl;
  for (auto it : m_httpTrace)
    {
      outFile << separator;
      it.StoreInFile (outFile);
      outFile << std::endl;
      separator = ",";
    }
  outFile << "]" << std::endl;
}

} /* namespace ns3 */
