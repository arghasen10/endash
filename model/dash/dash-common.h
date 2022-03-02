/*
 * dash-common.h
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_DASH_DASH_COMMON_H_
#define SRC_SPDASH_MODEL_DASH_DASH_COMMON_H_

#include <ns3/core-module.h>
#include <vector>
#include <iterator>

namespace ns3 {

struct VideoData{
	std::uint16_t m_numSegments;
	uint64_t m_segmentDuration;
	std::vector<double> m_averageBitrate;
	std::vector<std::vector<uint64_t> > m_segmentSizes;
};

enum DashPlayerState {
	DASH_PLAYER_STATE_UNINITIALIZED,
	DASH_PLAYER_STATE_MPD_DOWNLOADING,
	DASH_PLAYER_STATE_MPD_DOWNLOADED,
	DASH_PLAYER_STATE_SEGMENT_DOWNLOADING,
	DASH_PLAYER_STATE_IDLE,
	DASH_PLAYER_STATE_FINISHED,
};

struct DashPlaybackStatus {
	DashPlaybackStatus(): m_state(DASH_PLAYER_STATE_UNINITIALIZED), m_curSegmentNum(-1), m_nextQualityNum(0){}
	DashPlayerState m_state;
	Time m_playbackTime;
	Time m_bufferUpto;
	std::uint16_t m_curSegmentNum;
	std::uint16_t m_nextQualityNum;
};

}


#endif /* SRC_SPDASH_MODEL_DASH_DASH_COMMON_H_ */
