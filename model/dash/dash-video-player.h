/*
 * dash-video-player.h
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_DASH_DASH_VIDEO_PLAYER_H_
#define SRC_SPDASH_MODEL_DASH_DASH_VIDEO_PLAYER_H_

#include "dash-common.h"
#include "ns3/application.h"
#include "ns3/http-client-basic.h"
#include <ns3/internet-module.h>


namespace ns3 {


class DashVideoPlayer:public Application {
public:
  static TypeId GetTypeId(void);
  DashVideoPlayer();
  virtual ~DashVideoPlayer();

protected:
  virtual void EndApplication();

  virtual void TxTracedCallback(Ptr< const Packet > packet, Ptr< Ipv4 > ipv4, uint32_t interface);
  virtual void RxTracedCallback(Ptr< const Packet > packet, Ptr< Ipv4 > ipv4, uint32_t interface);
  /********************************
   *   DASH state functions
   ********************************/
  virtual void DownloadedCB(Ptr<Object> obj);
  virtual void DownloadFailedCB(Ptr<Object> obj);
  virtual void FinishedPlayback ();
  virtual void DashController();
  virtual void Downloaded();


  Address m_serverAddress;
  uint16_t m_serverPort;

  /********************************
   *    DASH state variable
   ********************************/
  DashPlaybackStatus m_playback;
  Ptr<HttpClientBasic> m_httpDownloader;
//  std::string m_videoFilePath;
  int m_lastQuality;
  int m_lastChunkSize;
  std::string m_cookie;
//  VideoData m_videoData;
  Time m_totalRebuffer;
  Time m_lastChunkStartTime;
  Time m_lastChunkFinishTime;


  Time m_timeout;
  bool m_running;
  int32_t m_traceId;
  std::string m_nodeTracePath;
  std::ofstream m_nodeTraceFp;
  Callback<std::string, Ptr<Node>, bool > m_nodeTraceCB;
  std::ofstream m_abrLogFp;
  std::string m_abrLogPath;

  bool m_lastSegRecved;
  Time m_segmentDuration;

private:
  void StopApplication();
  void StartApplication();

//  int ReadInVideoInfo ();


  /********************************
   *   DASH state functions
   ********************************/
  void StartDash(); //Should be called only once.
  void DownloadNextSegment();
  void AdjustVideoMetrices();


  void PeriodicDataCollection();

  std::string CollectNodeTrace(Ptr<Node> node, bool firstLine=false);


  virtual void LogTrace();

  std::string CreateRequestString();
  int Abr();

  /********************************
   *    DASH state variable
   ********************************/
  std::vector<HttpTrace> m_httpTrace;
  std::string m_tracePath;
  Time m_lastIncident;

  Time m_currentRebuffer;


  std::map<uint32_t, uint64_t> m_rxData;
  std::map<uint32_t, uint64_t> m_txData;
  Time m_nodeTraceInterval;
  Time m_nodeTraceLastTime;
  std::map<uint32_t, uint64_t> m_rxNodeTraceLastData;
  std::map<uint32_t, uint64_t> m_txNodeTraceLastData;
  Callback<void> m_onStartClient;
  Callback<void> m_onStopClient;

  std::map<uint32_t, std::vector<uint64_t> > m_segmentSizes;

//  std::ofstream m_nodeTraceFp;
};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_DASH_DASH_VIDEO_PLAYER_H_ */
