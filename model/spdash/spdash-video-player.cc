/*
 * dash-video-player.cc
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#include "spdash-video-player.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SpDashVideoPlayer");
NS_OBJECT_ENSURE_REGISTERED(SpDashVideoPlayer);

TypeId
SpDashVideoPlayer::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::SpDashVideoPlayer")
    .SetParent<DashVideoPlayer>()
    .SetGroupName("Applications")
    .AddConstructor<SpDashVideoPlayer>();
  return tid;
}

SpDashVideoPlayer::SpDashVideoPlayer(): DashVideoPlayer(){
}

SpDashVideoPlayer::~SpDashVideoPlayer(){
}

/****************************************
 *             DASH functions
 ****************************************/

void
SpDashVideoPlayer::StartDash ()
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
    }

  Ptr<Ipv4L3Protocol> ipnode = GetNode ()->GetObject<Ipv4L3Protocol> ();
  ipnode->TraceConnectWithoutContext ("Tx", MakeCallback (&SpDashVideoPlayer::TxTracedCallback, this));
  ipnode->TraceConnectWithoutContext ("Rx", MakeCallback (&SpDashVideoPlayer::RxTracedCallback, this));

  const clen_t expected = 1232; //Arbit
  m_playback.m_curSegmentNum = 0;

  NS_ASSERT(m_playback.m_state == DASH_PLAYER_STATE_UNINITIALIZED);

  m_httpDownloader = Create<HttpClientBasic> ();
  m_httpDownloader->SetTimeout (m_timeout);
  m_httpDownloader->SetCollectionCB (MakeCallback (&SpDashVideoPlayer::DownloadedCB, this).Bind (Ptr<Object> ()),
                                     MakeCallback (&SpDashVideoPlayer::DownloadFailedCB, this).Bind (Ptr<Object> ()),
                                     GetNode ());
  m_httpDownloader->InitConnection (m_serverAddress, m_serverPort, "/mpd");
//  m_httpDownloader->AddReqHeader ("videoPath", m_videoFilePath);
  m_httpDownloader->AddReqHeader ("X-Require-Length", std::to_string (expected));
  // m_httpDownloader->AddReqHeader("X-Require-Length", std::to_string(expected));
  m_lastChunkStartTime = Simulator::Now ();	//time when download started
  m_cookie = "";
  // std::cout<<"first req sent reqLen="<<expected<<"\n";
  m_httpDownloader->Connect ();
  m_playback.m_state = DASH_PLAYER_STATE_MPD_DOWNLOADING;
}

void
SpDashVideoPlayer::DownloadNextSegment ()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
    return;

  m_playback.m_curSegmentNum += 1;
//  if (m_playback.m_curSegmentNum == m_videoData.m_numSegments)
    {
      m_playback.m_state = DASH_PLAYER_STATE_FINISHED;
      return;
    }
  /****************************************
   * We will call abrController from here *
   ****************************************/
  // auto nextSegmentLength = m_videoData.m_segmentSizes.at(
  // 		m_playback.m_nextQualityNum).at(m_playback.m_curSegmentNum);
  std::string url = "/seg-" + std::to_string (m_playback.m_curSegmentNum) + "-"
      + std::to_string (m_playback.m_nextQualityNum);
  m_httpDownloader->InitConnection (m_serverAddress, m_serverPort, url);
  // m_httpDownloader->AddReqHeader("X-Require-Length", std::to_string(nextSegmentLength));
//  m_httpDownloader->AddReqHeader ("X-PathToVideo", m_videoFilePath);
  m_httpDownloader->AddReqHeader ("X-Require-Segment-Num", std::to_string (m_playback.m_curSegmentNum));
  m_httpDownloader->AddReqHeader ("X-LastChunkFinishTime", std::to_string (m_lastChunkFinishTime.GetNanoSeconds ()));
  m_httpDownloader->AddReqHeader ("X-LastChunkStartTime", std::to_string (m_lastChunkStartTime.GetNanoSeconds ()));
  m_httpDownloader->AddReqHeader ("X-BufferUpto", std::to_string (m_playback.m_bufferUpto.GetSeconds ()));
  m_httpDownloader->AddReqHeader ("X-Require-Length", std::to_string (0));
  m_httpDownloader->AddReqHeader ("X-LastChunkSize", m_lastChunkSize);
  m_httpDownloader->AddReqHeader ("X-Cookie", m_cookie);
  m_httpDownloader->AddReqHeader ("X-LastQuality", m_lastQuality);
  m_httpDownloader->AddReqHeader ("X-Rebuffer", std::to_string (m_totalRebuffer.GetSeconds ()));

  m_lastChunkStartTime = Simulator::Now ();	//time when download started
  m_httpDownloader->Connect ();

  // std::cout<<"connect called"<<m_playback.m_curSegmentNum<<" segLen="<<nextSegmentLength<<"\n";
  // std::cout<<"ftme "<<m_lastChunkFinishTime<<" "<<m_lastChunkFinishTime.GetNanoSeconds()<<"\n";
  m_playback.m_state = DASH_PLAYER_STATE_SEGMENT_DOWNLOADING;
}

void
SpDashVideoPlayer::DownloadedCB (Ptr<Object> obj)
{
  NS_LOG_FUNCTION(this);

  m_cookie = m_httpDownloader->GetResponse ()->GetHeader ("X-Cookie");
  m_lastQuality = std::stoi(m_httpDownloader->GetResponse ()->GetHeader ("X-LastQuality"));
  m_lastChunkSize = std::stoi(m_httpDownloader->GetResponse ()->GetHeader ("X-LastChunkSize")); //check
  m_lastSegRecved = std::stoi(m_httpDownloader->GetResponse ()->GetHeader ("X-LastSegment"));
  m_segmentDuration = MicroSeconds(std::stoi(m_httpDownloader->GetResponse ()->GetHeader ("X-SegmentDuration")));
  // std::cout<<m_cookie<<" "<<m_lastQuality<<" "<<m_lastChunkSize<<"\n";
  m_lastChunkFinishTime = Simulator::Now ();
  Simulator::ScheduleNow (&SpDashVideoPlayer::Downloaded, this);
}

} /* namespace ns3 */

//doubts
// lastRequestId is not used by abr
// rebuffer rime - unit = seconds
// start and finish time - unit= ns
