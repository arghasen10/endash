/*
 * derived-http-request-handler.cc
 *
 *  Created on: 06-Apr-2020
 *      Author: abhijit
 */

#include "http-server-simple-request-handler.h"

#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/application.h"
#include <ns3/core-module.h>
#include <sys/stat.h>



namespace ns3 {

NS_LOG_COMPONENT_DEFINE("HttpServerSimpleRequestHandler");
NS_OBJECT_ENSURE_REGISTERED(HttpServerSimpleRequestHandler);




bool fileExists(const char* filePath)
{
	//The variable that holds the file information
	struct stat fileAtt; //the type stat and function stat have exactly the same names, so to refer the type, we put struct before it to indicate it is an structure.

	//Use the stat function to get the information
	if (stat(filePath, &fileAtt) != 0) //start will be 0 when it succeeds
		return false; //So on non-zero, throw an exception

	//S_ISREG is a macro to check if the filepath referers to a file. If you don't know what a macro is, it's ok, you can use S_ISREG as any other function, it 'returns' a bool.
	return S_ISREG(fileAtt.st_mode);
}


TypeId HttpServerSimpleRequestHandler::GetTypeId(void) {
	static TypeId tid = TypeId("ns3::HttpServerSimpleRequestHandler")
			.SetParent<HttpServerBaseRequestHandler>()
			.SetGroupName("Applications")
			.AddConstructor<HttpServerSimpleRequestHandler>()
			.AddAttribute("cwd",
					"Working directory to server content",
					StringValue("."),
					MakeStringAccessor(&HttpServerSimpleRequestHandler::m_cwd),
					MakeStringChecker());
	return tid;
}

HttpServerSimpleRequestHandler::HttpServerSimpleRequestHandler(): m_sent(false){}

void HttpServerSimpleRequestHandler::ReadyToSend(uint32_t bufLen){
	NS_LOG_FUNCTION(this);
	char buf[1024];
	uint32_t tobeSend = std::min(bufLen, (uint32_t)sizeof(buf));
	if(m_ifs.good()) {
		m_ifs.read(buf, tobeSend);
		uint16_t canbeSend = tobeSend;
		if(!m_ifs) {
			canbeSend = m_ifs.gcount();
		}
		uint16_t sent = Send((uint8_t *) buf, canbeSend);
		NS_ASSERT(sent == canbeSend);
		if(canbeSend == tobeSend)
			return;
	}
	EndResponse();
}

void HttpServerSimpleRequestHandler::RequestHeaderReceived() {
	std::string path = GetPath();
	if(path.at(0) != '/')
		path = "/" + path;
	path = m_cwd + path;
	if(fileExists(path.c_str())) {
		m_ifs.open(path, std::ifstream::binary);
		if (m_ifs) { // File found
			m_ifs.seekg(0, m_ifs.end);
			int length = m_ifs.tellg();
			m_ifs.seekg(0, m_ifs.beg);
			SetClen(length);
			EndHeader();
			return;
		}
	}

	SetStatus(407, "File Not Found");
	m_sent = true;
	EndHeader();
}

} /* namespace ns3 */
