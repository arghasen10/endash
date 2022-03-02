/*
 * derived-http-request-handler.h
 *
 *  Created on: 06-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_DERIVED_HTTP_REQUEST_HANDLER_H_
#define SRC_SPDASH_MODEL_DERIVED_HTTP_REQUEST_HANDLER_H_


#include <fstream>      // std::ifstream

#include "http-server-base-request-handler.h"

namespace ns3 {

class HttpServerSimpleRequestHandler:public HttpServerBaseRequestHandler {
public:
	static TypeId GetTypeId(void);
	HttpServerSimpleRequestHandler();
protected:
	virtual void ReadyToSend(uint32_t);
	virtual void RequestHeaderReceived();
private:
	bool m_sent;
	std::string m_cwd;
	std::ifstream m_ifs;
};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_DERIVED_HTTP_REQUEST_HANDLER_H_ */
