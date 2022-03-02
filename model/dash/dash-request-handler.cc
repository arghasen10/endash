/*
 * dash-request-handler.cc
 *
 *  Created on: 08-Apr-2020
 *      Author: abhijit
 */

#include "dash-request-handler.h"
#include <ns3/core-module.h>

namespace ns3 {


template<typename T>
std::string ToString(T val) {
	std::stringstream stream;
	stream << val;
	return stream.str();
}

template<typename T>
std::string FromString(T val) {
	std::stringstream stream;
	stream << val;
	return stream.str();
}

NS_LOG_COMPONENT_DEFINE("DashRequestHandler");
NS_OBJECT_ENSURE_REGISTERED(DashRequestHandler);

TypeId DashRequestHandler::GetTypeId(void) {
	static TypeId tid = TypeId("ns3::DashRequestHandler")
			.SetParent<HttpServerBaseRequestHandler>()
			.SetGroupName("Applications")
			.AddConstructor<DashRequestHandler>();
	return tid;
}

DashRequestHandler::DashRequestHandler(): m_toSent(0), m_sent(0) {
	NS_LOG_FUNCTION(this);
}

DashRequestHandler::~DashRequestHandler() {
	NS_LOG_FUNCTION(this);
}

void DashRequestHandler::ReadyToSend(uint32_t freeBufLen) {
	NS_LOG_FUNCTION(this);
	if(m_sent == m_toSent) {
		EndResponse();
		return;
	}

	NS_ASSERT(m_sent <= m_toSent);

	clen_t needToSent = std::min((clen_t)(m_toSent - m_sent), (clen_t)0xffffffff);
	uint8_t buf[4096];
	uint32_t tobeSend = std::min(freeBufLen, (uint32_t)sizeof(buf));
	tobeSend = std::min(tobeSend, (uint32_t)needToSent);
	uint32_t sent = Send(buf, tobeSend);
	NS_ASSERT(sent == tobeSend);
	m_sent += sent;
}

void DashRequestHandler::SocketClosed() {
	NS_LOG_FUNCTION(this);
}

void DashRequestHandler::RequestHeaderReceived() {
	NS_LOG_FUNCTION(this);
	std::string responseLen = GetHeader("X-Require-Length");
	if(!responseLen.empty())
		m_toSent = std::stoul(responseLen);
	SetClen(m_toSent);
	SetStatus(200, "OK");
	EndHeader();
}

} /* namespace ns3 */
