/*
 * baseHttp-request-handler.h
 *
 *  Created on: 05-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_BASE_HTTP_REQUEST_HANDLER_H_
#define SRC_SPDASH_MODEL_BASE_HTTP_REQUEST_HANDLER_H_

#include "http-common-request-response.h"
#include "ns3/type-id.h"
#include "ns3/ptr.h"
#include "ns3/object-factory.h"
#include "ext-callback.h"

namespace ns3 {

class Socket;
class HttpServer;

/*
 * \Brief The BaseHttpHandler provides very basic functionality
 * 	      Regarding HttpServer. It don't even serves a file.
 * 	      Here derived classes are expected to implement vitual
 * 	      functions only. Several functions are kept not virtual
 * 	      intentionally.
 */
class HttpServerBaseRequestHandler: public Object {
public:
	static TypeId GetTypeId(void);
	HttpServerBaseRequestHandler();
	virtual ~HttpServerBaseRequestHandler();

//===================Request Handler==========

protected:
	virtual void RequestHeaderReceived();
	virtual void RequestDataReceived();
	virtual void ReadyToSend(uint32_t);
	virtual void SocketClosed();


	void AddHeader(std::string name, std::string value);
	std::string GetPath();
	std::string GetHeader(std::string name);
	const Ptr<HttpRequest>& GetRequest() const {return m_request;}
	const Ptr<HttpResponse>& GetResponse() const {return m_response;}
	uint16_t Send(const uint8_t *data, const uint16_t len);
	uint16_t Recv(uint8_t *data, uint16_t len);
	void EndResponse(); ///< indicate that response ended.
	void EndHeader(); ///< indicate that the response header have ended.
	void SetClen(clen_t mClen) { m_clen = mClen; }
	void SetStatus(uint16_t code, std::string status);
private:
	void HandleRead(Ptr<Socket> socket);
	void HandleSend(Ptr<Socket> socket, uint32_t packetSizeToReturn);
	void SendHeaders();

	void HandlePeerClose(Ptr<Socket> socket);
	void HandlePeerError(Ptr<Socket> socket);

	void CleanCBs();
	void InitFromServer(Ptr<Socket> sock, HttpServer *server,
			Callback<void> onClose);

	Ptr<Socket> m_socket;
	Ptr<HttpRequest> m_request;
	Ptr<HttpResponse> m_response;
	clen_t m_clen;
	HttpServer *m_server;
	bool m_sendStarted;
	bool m_headerSent;
	bool m_processedHeader;
	bool m_running;
	Callback<void> m_onClose;
	friend class HttpServer;
};

class BaseHttpRequestHandlerFactory : public ObjectFactory {
public:
	BaseHttpRequestHandlerFactory();

};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_BASE_HTTP_REQUEST_HANDLER_H_ */
