/*
 * http-client.h
 *
 *  Created on: 03-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_HTTP_CLIENT_COLLECTION_H_
#define SRC_SPDASH_MODEL_HTTP_CLIENT_COLLECTION_H_

#include "http-client-basic.h"
#include "ns3/application.h"
#include "ns3/object-factory.h"

namespace ns3 {
class Socket;
class Packet;


class HttpClientCollection: public Application {
public:
	static TypeId GetTypeId(void);
	HttpClientCollection();
	virtual ~HttpClientCollection();

protected:
	virtual void DoDispose();
	virtual void DoInitialize (void);

//	virtual void ReadyToRecv(){};
//	virtual void ReadyToSend(){};

private:
	virtual void StopApplication();
	virtual void StartApplication();
	virtual void StartNextConnection();
	virtual void onConnectionClosed(Ptr<Object>);
	virtual void onConnectionErrorClosed(Ptr<Object> blob);

	void SetHttpClientFactoryType(TypeId typeId);
	TypeId GetHttpClientFactoryType() const;


	Address m_peerAddress; //!< Remote peer address
	uint16_t m_peerPort; //!< Remote peer port
	uint16_t m_count;
	bool m_running;
	Ptr<HttpClientBasic> m_httpClient; //Temporary
	ObjectFactory m_httpClientFactory;
};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_HTTP_CLIENT_COLLECTION_H_ */
