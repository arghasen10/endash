/*
 * baseHttp-request-handler.cc
 *
 *  Created on: 05-Apr-2020
 *      Author: abhijit
 */


#include "http-server-base-request-handler.h"

#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/application.h"


namespace ns3 {



/**************************************************
 *            Http Request Handler                *
 **************************************************/

NS_LOG_COMPONENT_DEFINE("HttpServerBaseRequestHandler");

NS_OBJECT_ENSURE_REGISTERED(HttpServerBaseRequestHandler);

TypeId HttpServerBaseRequestHandler::GetTypeId(void) {
	static TypeId tid = TypeId("ns3::HttpServerBaseRequestHandler")
			.SetParent<Application>()
			.SetGroupName("Applications")
			.AddConstructor<HttpServerBaseRequestHandler>();
	return tid;
}

HttpServerBaseRequestHandler::HttpServerBaseRequestHandler() :
		m_clen(0), m_server(NULL), m_sendStarted(false), m_headerSent(false), m_processedHeader(
				false), m_running(true) {
	NS_LOG_FUNCTION(this);

}

HttpServerBaseRequestHandler::~HttpServerBaseRequestHandler(){
	NS_LOG_FUNCTION(this);
//	std::cout << "~HttpServerClient" << std::endl;
}

void HttpServerBaseRequestHandler::SendHeaders() {
	NS_LOG_FUNCTION(this);

	uint16_t canSend = std::min(m_socket->GetTxAvailable(), (uint32_t)1024);
	if(!canSend)
		return;
	uint8_t *buf = new uint8_t[canSend];
	uint16_t toBeSent = m_response->ReadHeader(buf, canSend);
	uint16_t sent;
	if(!toBeSent) {
		m_headerSent = true;
		goto clean;
	}
	sent = m_socket->Send(buf, toBeSent, 0);
	NS_ASSERT(sent == toBeSent);

clean:
	delete[] buf;
}

void HttpServerBaseRequestHandler::HandleSend(Ptr<Socket> socket, uint32_t txSpace) {
	NS_LOG_FUNCTION(this << socket);
	if(!m_sendStarted || !m_running)
		return;
	if(!m_headerSent) {
		return SendHeaders();
	}
	ReadyToSend(txSpace);
}

void HttpServerBaseRequestHandler::HandleRead(Ptr<Socket> socket) {
	NS_LOG_FUNCTION(this << socket);
	if(!m_running) return;
	uint8_t buf[2048];
	uint16_t len = 0;
	len = socket->Recv(buf, 2048, 0);
	m_request->ParseHeader(buf, len);
	if(!m_request->IsHeaderReceived())
		return;
	if(m_processedHeader)
		return RequestDataReceived();
	RequestHeaderReceived();
	m_processedHeader = true;
}

void HttpServerBaseRequestHandler::HandlePeerClose(Ptr<Socket> socket) {
	NS_LOG_FUNCTION(this << socket);
	CleanCBs();
}

void HttpServerBaseRequestHandler::HandlePeerError(Ptr<Socket> socket) {
	NS_LOG_FUNCTION(this << socket);
	CleanCBs();
}

void HttpServerBaseRequestHandler::RequestHeaderReceived() {
	NS_LOG_FUNCTION(this);
	std::string path = GetPath();
	AddHeader("Server", "Hello NS3 http server");
	AddHeader("Server-x", "Hello NS3 http server");
	AddHeader("Server-x2", "Hello NS3 http server");
	EndHeader();
}
void HttpServerBaseRequestHandler::RequestDataReceived() {
	NS_LOG_FUNCTION(this);
}
void HttpServerBaseRequestHandler::ReadyToSend(uint32_t txSpace){
	NS_LOG_FUNCTION(this);

	EndResponse();
}
void HttpServerBaseRequestHandler::AddHeader(std::string name, std::string value) {
	NS_LOG_FUNCTION(this << " " << name << " " << value);
	GetResponse()->AddHeader(name, value);
}
void HttpServerBaseRequestHandler::SetStatus(uint16_t code, std::string status) {
	NS_LOG_FUNCTION(this);
	GetResponse()->SetStatusCode(code);
	GetResponse()->SetStatusText(status);
}
std::string HttpServerBaseRequestHandler::GetPath() {
	NS_LOG_FUNCTION(this);
	return GetRequest()->GetPath();
}

std::string HttpServerBaseRequestHandler::GetHeader(std::string name) {
	NS_LOG_FUNCTION(this);
	return GetRequest()->GetHeader(name);
}

uint16_t HttpServerBaseRequestHandler::Send(const uint8_t *data, const uint16_t len) {
	NS_LOG_FUNCTION(this);
	NS_ASSERT(m_sendStarted);
	return m_socket->Send(data, len, 0);
}

void HttpServerBaseRequestHandler::SocketClosed() {

}

void HttpServerBaseRequestHandler::EndHeader(){
	NS_LOG_FUNCTION(this);
	m_sendStarted = true;
	AddHeader("Content-Length", std::to_string(m_clen));
	m_response->EndHeader();
	HandleSend(m_socket, m_socket->GetTxAvailable());
}

void HttpServerBaseRequestHandler::EndResponse() {
	NS_LOG_FUNCTION(this);
	CleanCBs();
	m_socket->Close();
}

uint16_t HttpServerBaseRequestHandler::Recv(uint8_t *data, uint16_t len) {
	return GetRequest()->ReadBody(data, len);
}

void HttpServerBaseRequestHandler::CleanCBs() {
	if(!m_running) return;
	m_running = false;
	if(!m_onClose.IsNull()) m_onClose();
	m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	m_socket->SetSendCallback(MakeNullCallback<void, Ptr<Socket>, uint32_t >());
	m_socket->SetCloseCallbacks(
			MakeNullCallback<void, Ptr<Socket> >(),
			MakeNullCallback<void, Ptr<Socket> >());
}

void HttpServerBaseRequestHandler::InitFromServer(Ptr<Socket> sock, HttpServer *server,
			Callback<void> onClose) {
	m_socket = sock;
	m_server = server;
	m_onClose = onClose;

	m_request = Create<HttpRequest>();
	m_response = Create<HttpResponse>("http1.1", 407, "Not Found");
	m_socket->SetRecvCallback(MakeCallback(&HttpServerBaseRequestHandler::HandleRead, this));
	m_socket->SetSendCallback(MakeCallback(&HttpServerBaseRequestHandler::HandleSend, this));
	m_socket->SetCloseCallbacks(
			MakeCallback(&HttpServerBaseRequestHandler::HandlePeerClose, this),
			MakeCallback(&HttpServerBaseRequestHandler::HandlePeerError, this));
}

BaseHttpRequestHandlerFactory::BaseHttpRequestHandlerFactory(){
	SetTypeId(HttpServerBaseRequestHandler::GetTypeId());
}

} /* namespace ns3 */
