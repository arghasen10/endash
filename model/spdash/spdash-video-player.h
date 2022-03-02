/*
 * dash-video-player.h
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_SPDASH_SPDASH_VIDEO_PLAYER_H_
#define SRC_SPDASH_MODEL_SPDASH_SPDASH_VIDEO_PLAYER_H_

#include "ns3/dash-video-player.h"
//#include "spdash-common.h"
//#include "ns3/application.h"
//#include "ns3/http-client-basic.h"
#include <fstream>

namespace ns3 {


class SpDashVideoPlayer:public DashVideoPlayer {
public:
  static TypeId GetTypeId(void);
  SpDashVideoPlayer();
  virtual ~SpDashVideoPlayer();

private:
  /********************************
   *   DASH state functions
   ********************************/
  virtual void StartDash(); //Should be called only once.
  virtual void DownloadNextSegment();
  virtual void DownloadedCB (Ptr<Object> obj);

};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_DASH_DASH_VIDEO_PLAYER_H_ */
