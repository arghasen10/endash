/*
 * basic-http-client.h
 *
 *  Created on: 06-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_BASIC_HTTP_CLIENT_H_
#define SRC_SPDASH_MODEL_BASIC_HTTP_CLIENT_H_


#include "http-common-request-response.h"
#include "ns3/application.h"
#include "ns3/socket.h"


//class Socket;
//class HttpRequest;

namespace ns3 {


struct HttpTrace {
	Time m_reqSentAt, m_firstByteAt, m_lastByteAt;
	clen_t m_resLen;
	double m_speed;
	std::list<std::pair<Time, uint64_t> > m_trace;

	HttpTrace(): m_resLen(0), m_speed(0){}

	void RequestSent();
	void ResponseRecv(uint32_t len);
	void StoreInFile(std::ostream &outFile);
	double GetDownloadSpeed() const;
};

class HttpClientBasic : public Object {
public:
	static TypeId GetTypeId(void);
	HttpClientBasic();
	virtual ~HttpClientBasic();
	virtual void SetCollectionCB(Callback<void> successCb, Callback<void> errorCb, Ptr<Node> node);
	virtual void InitConnection(Address peerAddress, uint16_t peerPort, std::string path="/");
	virtual void InitConnection(std::string path = "");
	virtual void StopConnection();


	virtual void AddReqHeader(std::string name, std::string value);
	template <class T>
	void AddReqHeader(std::string name, T value){AddReqHeader(name, std::to_string(value));}
	virtual void Connect();
	virtual const HttpTrace& GetTrace() const;

	virtual const Ptr<HttpResponse>& GetResponse() const; //earlier protected
	virtual const Time& GetTimeout () const;
	virtual void SetTimeout (const Time &mTimeout);
	virtual void SetRetries (uint16_t numRetries);

protected:
	virtual void RecvResponseData(uint8_t *data, uint32_t len);
	virtual void RecvResponseHeader();

private:
	void EvConnectionSucceeded (Ptr<Socket> socket);
	void EvConnectionFailed (Ptr<Socket> socket);
	void EvHandleRecv (Ptr<Socket> socket);
	void EvHandleSend (Ptr<Socket> socket, uint32_t bufAvailable);
	void EvSocketClosed (Ptr<Socket> socket);
	void EvErrorClosed (Ptr<Socket> socket);
	void EvTimeoutOccur();

	void EndConnection();
	void EndErrorConnection();
	void StopConnectionPriv();
	void RetryConnection();

	//Internal variable
	Ptr<HttpRequest> m_request;
	Ptr<HttpResponse> m_response;
	Ptr<Socket> m_socket; //!< IPv4 Socket
	uint16_t m_peerPort; //!< Remote peer port
	std::string m_path;
	std::string m_method;
	Address m_peerAddress; //!< Remote peer address
	Ptr<Node> m_node;
	EventId m_timeoutEvent;
//	Ptr<Object> m_collectionBlob;
	Callback<void> m_onConnectionClosed;
	Callback<void> m_onConnectionErrorClosed;

	Time m_timeout;
	uint16_t m_retries;
	uint16_t m_numTried;

	//=================
	// Trace
	HttpTrace m_trace;
};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_BASIC_HTTP_CLIENT_H_ */
