/*
 * dash-video-player.h
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_ENDASH_MODEL_ENDASH_ENDASH_VIDEO_PLAYER_H_
#define SRC_ENDASH_MODEL_ENDASH_ENDASH_VIDEO_PLAYER_H_

#include "ns3/dash-video-player.h"
//#include "endash-common.h"
//#include "ns3/application.h"
//#include "ns3/http-client-basic.h"
#include <fstream>

namespace ns3 {


class endashVideoPlayer:public DashVideoPlayer {
public:
  static TypeId GetTypeId(void);
  endashVideoPlayer();
  virtual ~endashVideoPlayer();

private:
  /********************************
   *   DASH state functions
   ********************************/
  virtual void StartDash(); //Should be called only once.
  virtual void DownloadNextSegment();
  virtual void DownloadedCB (Ptr<Object> obj);

};

} /* namespace ns3 */

#endif /* SRC_ENDASH_MODEL_DASH_DASH_VIDEO_PLAYER_H_ */
