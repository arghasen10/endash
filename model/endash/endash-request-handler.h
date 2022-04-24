/*
 * dash-request-handler.h
 *
 *  Created on: 08-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_ENDASH_MODEL_ENDASH_ENDASH_REQUEST_HANDLER_H_
#define SRC_ENDASH_MODEL_ENDASH_ENDASH_REQUEST_HANDLER_H_

#include "ns3/dash-request-handler.h"
#include "ns3/dash-common.h"

namespace ns3 {

class endashRequestHandler: public DashRequestHandler {
public:
	static TypeId GetTypeId(void);
	endashRequestHandler();
	virtual ~endashRequestHandler();
protected:
  virtual void RequestHeaderReceived();

private:
	// new
//	std::string m_videoFilePath;
//	VideoData m_videoData;
	// int m_lastChunkSize;
//	int ReadInVideoInfo();
	int Abr(std::string cookie, std::string nextChunkId, std::string lastQuality, std::string buffer, std::string lastRequest, std::string rebufferTime, std::string lastChunkFinishTime, std::string lastChunkStartTime, std::string lastChunkSize);
	std::string CreateRequestString( std::string cookie, std::string nextChunkId, std::string lastQuality, std::string buffer, std::string lastRequest, std::string rebufferTime, std::string lastChunkFinishTime, std::string lastChunkStartTime, std::string lastChunkSize);
  std::vector<uint64_t> m_segmentSizes;
};

} /* namespace ns3 */

#endif /* SRC_ENDASH_MODEL_DASH_DASH_REQUEST_HANDLER_H_ */
