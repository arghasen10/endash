/*
 * dash-request-handler.h
 *
 *  Created on: 08-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_DASH_DASH_REQUEST_HANDLER_H_
#define SRC_SPDASH_MODEL_DASH_DASH_REQUEST_HANDLER_H_

#include "ns3/http-server-base-request-handler.h"

namespace ns3 {

class DashRequestHandler: public HttpServerBaseRequestHandler {
public:
	static TypeId GetTypeId(void);
	DashRequestHandler();
	virtual ~DashRequestHandler();
protected:
	virtual void ReadyToSend(uint32_t);
	virtual void SocketClosed();
	virtual void RequestHeaderReceived();
  clen_t m_toSent;
  clen_t m_sent;
private:
};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_DASH_DASH_REQUEST_HANDLER_H_ */
