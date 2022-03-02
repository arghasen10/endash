/*
 * dash-video-player.cc
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#include "dash-video-player.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "ns3/nlohmann_json.h"

using json = nlohmann::json;
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DashVideoPlayer");
NS_OBJECT_ENSURE_REGISTERED(DashVideoPlayer);

TypeId
DashVideoPlayer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DashVideoPlayer")
    .SetParent<Application> ()
    .SetGroupName ("Applications").AddConstructor<DashVideoPlayer> ()
    .AddAttribute ("RemoteAddress",
                   "Server address",
                   AddressValue (),
                   MakeAddressAccessor (&DashVideoPlayer::m_serverAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&DashVideoPlayer::m_serverPort),
                   MakeUintegerChecker<uint16_t> ())
//    .AddAttribute ("VideoFilePath",
//                   "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes",
//                   StringValue ("vid.txt"),
//                   MakeStringAccessor (&DashVideoPlayer::m_videoFilePath),
//                   MakeStringChecker ())
    .AddAttribute ("OnStartCB",
                   "Callback for start and stop",
                   CallbackValue (MakeNullCallback<void> ()),
                   MakeCallbackAccessor (&DashVideoPlayer::m_onStartClient),
                   MakeCallbackChecker ())
    .AddAttribute ("OnStopCB",
                   "Callback for start and stop",
                   CallbackValue (MakeNullCallback<void> ()), MakeCallbackAccessor (&DashVideoPlayer::m_onStopClient), MakeCallbackChecker ())
    .AddAttribute ("TracePath",
                   "File Path to store trace",
                   StringValue (),
                   MakeStringAccessor (&DashVideoPlayer::m_tracePath),
                   MakeStringChecker ())
    .AddAttribute ("NodeTracePath",
                   "Node trace provides informations like current position, speed, cellData per line",
                   StringValue (),
                   MakeStringAccessor (&DashVideoPlayer::m_nodeTracePath),
                   MakeStringChecker ())
    .AddAttribute ("NodeTraceInterval",
                   "Node trace interval",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&DashVideoPlayer::m_nodeTraceInterval),
                   MakeTimeChecker ())
    .AddAttribute ("NodeTraceHelperCallBack",
                   "NodeTrace Helper callback function collect more data than usual",
                   CallbackValue (MakeNullCallback<std::string, Ptr<Node> > ()),
                   MakeCallbackAccessor (&DashVideoPlayer::m_nodeTraceCB),
                   MakeCallbackChecker ())
    .AddAttribute ("Timeout",
                   "Timeout",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&DashVideoPlayer::m_timeout),
                   MakeTimeChecker ())
     .AddAttribute ("AbrLogPath",
                    "AbrLog",
                    StringValue (),
                    MakeStringAccessor (&DashVideoPlayer::m_abrLogPath),
                    MakeStringChecker ());
  return tid;
}

DashVideoPlayer::DashVideoPlayer () :
    m_serverPort(0), m_lastQuality(-1), m_lastChunkSize(-1), m_running (0), m_traceId(0), m_lastSegRecved(false)
{

}

DashVideoPlayer::~DashVideoPlayer ()
{
}

void
DashVideoPlayer::StopApplication ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
    return;

  if (m_httpDownloader != 0)
    {
      m_httpDownloader->StopConnection ();
    }
}

void
DashVideoPlayer::EndApplication ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
    return;
  m_running = false;
  LogTrace ();
  if (!m_onStopClient.IsNull ())
    {
      m_onStopClient ();
    }
}

void
DashVideoPlayer::StartApplication ()
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT(!m_running);
  if (!m_onStartClient.IsNull ())
    {
      m_onStartClient ();
    }
  m_running = true;
//  ReadInVideoInfo ();
  StartDash ();
  PeriodicDataCollection ();
}

//int
//DashVideoPlayer::ReadInVideoInfo ()
//{
//  NS_LOG_FUNCTION(this);
//  std::ifstream myfile;
//  myfile.open (m_videoFilePath);
//  NS_ASSERT(myfile);
//  if (!myfile)
//    {
//      return -1;
//    }
//  std::string temp;
//  std::getline (myfile, temp);
//  if (temp.empty ())
//    {
//      return -1;
//    }
//  std::istringstream buffer (temp);
//  std::vector<uint64_t> line ((std::istream_iterator<uint64_t> (buffer)), std::istream_iterator<uint64_t> ());
//  m_videoData.m_segmentDuration = line.front ();
//  //read bitrates
//  std::getline (myfile, temp);
//  if (temp.empty ())
//    {
//      return -1;
//    }
//  buffer = std::istringstream (temp);
//  std::vector<double> linef ((std::istream_iterator<double> (buffer)), std::istream_iterator<double> ());
//  m_videoData.m_averageBitrate = linef;
//  //read bitrates
//  uint16_t numsegs = 0;
//  while (std::getline (myfile, temp))
//    {
//      if (temp.empty ())
//        {
//          break;
//        }
//      std::istringstream buffer (temp);
//      std::vector<uint64_t> line ((std::istream_iterator<uint64_t> (buffer)), std::istream_iterator<uint64_t> ());
//      m_videoData.m_segmentSizes.push_back (line);
//      NS_ASSERT(numsegs == 0 || numsegs == line.size ());
//      numsegs = line.size ();
//    }
//  m_videoData.m_numSegments = numsegs;
//  NS_ASSERT_MSG(!m_videoData.m_segmentSizes.empty (), "No segment sizes read from file.");
//  return 1;
//}

/****************************************
 *             DASH functions
 ****************************************/
void
DashVideoPlayer::StartDash ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
    return;

  if (m_node)
    {
      m_nodeTraceFp.open (m_nodeTracePath + "_" + std::to_string (GetNode ()->GetId ()) + ".csv",
                          std::ofstream::out | std::ofstream::trunc);
      m_nodeTraceFp << "#,Time,rxSpeed,txSpeed," << m_nodeTraceCB (GetNode (), true) << std::endl;
      m_traceId = 0;
      m_abrLogFp.open (m_abrLogPath + "_" + std::to_string (GetNode ()->GetId ()) + ".csv",
                      std::ofstream::out | std::ofstream::trunc);
      m_abrLogFp << "#time,nodeid,segNum,lastChunkSize,lastChunkStartTime,lastChunkFinishTime,bufferUpto,totalRebuffer,lastQuality,currentRebuffer" << std::endl;
    }

  Ptr<Ipv4L3Protocol> ipnode = GetNode ()->GetObject<Ipv4L3Protocol> ();
  ipnode->TraceConnectWithoutContext ("Tx", MakeCallback (&DashVideoPlayer::TxTracedCallback, this));
  ipnode->TraceConnectWithoutContext ("Rx", MakeCallback (&DashVideoPlayer::RxTracedCallback, this));

  const clen_t expected = 1232; //Arbit as it is the mpd file

  NS_ASSERT(m_playback.m_state == DASH_PLAYER_STATE_UNINITIALIZED);

  m_httpDownloader = Create<HttpClientBasic> ();
  m_httpDownloader->SetTimeout (m_timeout);
  m_httpDownloader->SetCollectionCB (MakeCallback (&DashVideoPlayer::DownloadedCB, this).Bind (Ptr<Object> ()),
                                     MakeCallback (&DashVideoPlayer::DownloadFailedCB, this).Bind (Ptr<Object> ()),
                                     GetNode ());
  m_httpDownloader->InitConnection (m_serverAddress, m_serverPort, "/mpd");

  m_httpDownloader->AddReqHeader ("X-Require-Length", std::to_string (expected));
  //initialisation
  m_lastChunkStartTime = Simulator::Now ();	//time when download started
  m_cookie = "";
  m_lastChunkSize = expected;
  m_lastQuality = 0;
  m_totalRebuffer = Time (0);
  m_currentRebuffer = Time (0);
  m_httpDownloader->Connect ();
  m_playback.m_state = DASH_PLAYER_STATE_MPD_DOWNLOADING;
}

void
DashVideoPlayer::Downloaded ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
    return;
  HttpTrace trace = m_httpDownloader->GetTrace ();
  m_httpTrace.push_back (trace);
  switch (m_playback.m_state)
    {
    case DASH_PLAYER_STATE_MPD_DOWNLOADING:
      m_playback.m_state = DASH_PLAYER_STATE_MPD_DOWNLOADED;
      DownloadNextSegment (); //no need to go through
      break;
    case DASH_PLAYER_STATE_SEGMENT_DOWNLOADING:
      m_playback.m_state = DASH_PLAYER_STATE_IDLE;
      DashController ();
      break;
    default:
      NS_ASSERT(false && "Invalid State");
    }
}

void DashVideoPlayer::DownloadedCB(Ptr<Object> obj) {
  NS_LOG_FUNCTION(this);

  m_lastChunkFinishTime = Simulator::Now();
  Simulator::ScheduleNow(&DashVideoPlayer::Downloaded, this);
}

void DashVideoPlayer::DownloadFailedCB(Ptr<Object> obj)
{
  std::cout << "Ending application due to error" << std::endl;
  Simulator::ScheduleNow(&DashVideoPlayer::EndApplication, this);
}

void
DashVideoPlayer::PeriodicDataCollection ()
{
  NS_LOG_FUNCTION(this);
  if (!m_nodeTraceFp.is_open ())
    return;

  if (!m_running)
    return;

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
  m_nodeTraceFp << m_traceId << "," << now.GetSeconds () << "," << rxSpeed << "," << txSpeed << ","
      << m_nodeTraceCB (node, false) << std::endl;
  m_traceId++;
  Simulator::Schedule (m_nodeTraceInterval, &DashVideoPlayer::PeriodicDataCollection, this);
}

#define MIN_BUFFER_LENGTH 30 //sec
#define NS_IN_SEC 1000000
void
DashVideoPlayer::DashController ()
{
  NS_LOG_FUNCTION(this);

  AdjustVideoMetrices ();

  if(m_abrLogFp.is_open()) {
      m_abrLogFp << Simulator::Now () << "," << GetNode ()->GetId () << "," << m_playback.m_curSegmentNum << ","
          << m_lastChunkSize << "," << m_lastChunkStartTime.GetSeconds() << "," << m_lastChunkFinishTime.GetSeconds() << ","
          << m_playback.m_bufferUpto.GetSeconds() << "," << m_totalRebuffer.GetSeconds() << "," << m_lastQuality << "," << m_currentRebuffer.GetSeconds()
          << std::endl;
  }

//  if (m_playback.m_curSegmentNum < m_videoData.m_numSegments - 1)
  if(!m_lastSegRecved)
    {
      Time delay = std::max (m_playback.m_bufferUpto - Time (std::to_string (MIN_BUFFER_LENGTH) + "s"), Time (0));
      Simulator::Schedule (delay, &DashVideoPlayer::DownloadNextSegment, this);
    }
  else
    {
      Simulator::Schedule (m_playback.m_bufferUpto, &DashVideoPlayer::FinishedPlayback, this);
    }
}

std::string
DashVideoPlayer::CreateRequestString ()
{

  json jsonBody;
//  jsonBody["bitrateArray"] = m_videoData.m_averageBitrate;

  if (m_cookie == "")
    jsonBody["cookie"] = nullptr;
  else
    jsonBody["cookie"] = m_cookie;
  jsonBody["nextChunkId"] = m_playback.m_curSegmentNum;
  jsonBody["lastquality"] = m_lastQuality;
  jsonBody["buffer"] = m_playback.m_bufferUpto.GetSeconds ();  //in seconds

  jsonBody["lastRequest"] = m_playback.m_curSegmentNum - 1;
  jsonBody["rebufferTime"] = m_totalRebuffer.GetSeconds ();
  // std::cout<<"okay\n";
  jsonBody["lastChunkFinishTime"] = m_lastChunkFinishTime.GetNanoSeconds ();
  jsonBody["lastChunkStartTime"] = m_lastChunkStartTime.GetNanoSeconds ();
  jsonBody["lastChunkSize"] = m_lastChunkSize;
  std::string jsonString = jsonBody.dump (4);
  int contentLength = jsonString.length ();
  std::string requestString =
      "POST / HTTP/1.1\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\nHost: www.tutorialspoint.com\r\nContent-Type: text/xml; charset=utf-8\r\nContent-Length: "
          + std::to_string (contentLength)
          + "\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate\r\nConnection: Keep-Alive\r\n\r\n"
          + jsonString;
  // std::cout<<"jstring="<<requestString<<"\n";
  return requestString;
}

//Adds header in response
int
DashVideoPlayer::Abr ()
{
  const int PORT = 8333;
  int sock = 0, valread;
  struct sockaddr_in serv_addr;

  std::string to_send = CreateRequestString ();

  char buff[1024] =
    { 0 };
  if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
      printf ("\n Socket creation error \n");
      return -1;
    }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons (PORT);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton (AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
      printf ("\nInvalid address/ Address not supported \n");
      return -1;
    }

  if (connect (sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
      printf ("\nConnection Failed \n");
      return -1;
    }
  const char *AbrRequest = to_send.c_str ();
  send (sock, AbrRequest, strlen (AbrRequest), 0);
  std::string entireContent;
  while (1)
    {
      valread = read (sock, buff, 1024);
      if (valread <= 0)
        break;
      buff[valread] = '\0';
      entireContent += buff;
    }
  std::string delimiter = "\r\n\r\n";
  std::string responseBody = entireContent.substr (entireContent.find (delimiter) + 4, std::string::npos);
  json jsonResponse = json::parse (responseBody);

  m_lastQuality = int (jsonResponse["quality"]);
  m_cookie = jsonResponse["cookie"];

  m_segmentDuration = MicroSeconds(int64_t(jsonResponse["segmentDuration"]));
  std::vector<uint64_t> line = jsonResponse["sizes"].get<std::vector<uint64_t> >();
  m_segmentSizes[m_playback.m_curSegmentNum] = line;
  m_lastSegRecved = jsonResponse["lastSeg"].get<bool>();
  return int (jsonResponse["quality"]);
}

void
DashVideoPlayer::DownloadNextSegment ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
    return;
  m_playback.m_curSegmentNum += 1;
//  if (m_playback.m_curSegmentNum == m_videoData.m_numSegments)
  if(m_lastSegRecved)
    {
      m_playback.m_state = DASH_PLAYER_STATE_FINISHED;
      return;
    }


  int nextQualityNum = Abr ();  //updates m_lq,m_cookie
  /****************************************
   * We will call abrController from here *
   ****************************************/
  auto nextSegmentLength = m_segmentSizes[m_playback.m_curSegmentNum].at(nextQualityNum);

  std::string url = "/seg-" + std::to_string (m_playback.m_curSegmentNum) + "-"
      + std::to_string (m_playback.m_nextQualityNum);
  m_httpDownloader->InitConnection (m_serverAddress, m_serverPort, url);
  m_httpDownloader->AddReqHeader ("X-Require-Length", std::to_string (nextSegmentLength));

  m_lastChunkStartTime = Simulator::Now ();
  m_lastChunkSize = nextSegmentLength;
  m_httpDownloader->Connect ();
  m_playback.m_state = DASH_PLAYER_STATE_SEGMENT_DOWNLOADING;
}

void
DashVideoPlayer::FinishedPlayback ()
{
  NS_LOG_FUNCTION(this);
  m_playback.m_state = DASH_PLAYER_STATE_FINISHED;
  if (m_nodeTraceFp.is_open ())
    {
      m_nodeTraceFp.close ();
    }
  if (m_abrLogFp) {
      m_abrLogFp.close();
  }
  EndApplication ();
  //	StopApplication();
  //	std::cout<<"playback finished"<<std::endl;
}

void
DashVideoPlayer::AdjustVideoMetrices ()
{
  NS_LOG_FUNCTION(this);
  Time timeSpent (0);
  Time curBufUpto (0);
  Time stall (0);
  Time curPlaybackTime (0);
  if (m_playback.m_curSegmentNum > 0)
    {
      timeSpent = Simulator::Now () - m_lastIncident;
      curBufUpto = std::max (m_playback.m_bufferUpto - timeSpent, Time (0));
      auto bufChanged = m_playback.m_bufferUpto - curBufUpto;
      stall = timeSpent - bufChanged;
//      std::cout << "\t\t\t\t\t\t" << stall << "=stall\n";
      m_currentRebuffer = stall;
      m_totalRebuffer += stall;
      curPlaybackTime = m_playback.m_playbackTime + bufChanged;
    }
  m_lastIncident = Simulator::Now ();

  m_playback.m_playbackTime = curPlaybackTime;
  m_playback.m_bufferUpto = curBufUpto + Time (std::to_string (m_segmentDuration.GetMicroSeconds()) + "us");
}

std::string
DashVideoPlayer::CollectNodeTrace (Ptr<Node> node, bool firstLine)
{
  if (firstLine)
    return "nodeId";
  return std::to_string (node->GetId ());
}

void
DashVideoPlayer::TxTracedCallback (Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
  m_txData[interface] += packet->GetSize ();
}

void
DashVideoPlayer::RxTracedCallback (Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
  m_rxData[interface] += packet->GetSize ();
}

void
DashVideoPlayer::LogTrace ()
{
  NS_LOG_FUNCTION(this);
  if (m_tracePath.empty ())
    return;

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
