/*
 * http-client.cc
 *
 *  Created on: 03-Apr-2020
 *      Author: abhijit
 */
#include "http-client-collection.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/uinteger.h"




namespace ns3 {

NS_LOG_COMPONENT_DEFINE("HttpClientCollection");
NS_OBJECT_ENSURE_REGISTERED(HttpClientCollection);

TypeId HttpClientCollection::GetTypeId(void) {
	static TypeId tid = TypeId("ns3::HttpClientCollection")
			.SetParent<Application>()
			.SetGroupName("Applications")
			.AddConstructor<HttpClientCollection>()
			.AddAttribute("RemoteAddress",
					"Server address",
					AddressValue(),
					MakeAddressAccessor(&HttpClientCollection::m_peerAddress),
					MakeAddressChecker())
			.AddAttribute("RemotePort",
					"Port on which we listen for incoming packets.",
					UintegerValue(9),
					MakeUintegerAccessor(&HttpClientCollection::m_peerPort),
					MakeUintegerChecker<uint16_t>())
			.AddAttribute("NumRequest",
					"Number of request.",
					UintegerValue(1),
					MakeUintegerAccessor(&HttpClientCollection::m_count),
					MakeUintegerChecker<uint16_t>())
			.AddAttribute(
					"HttpClientTypeId",
					"Call back function to create RequestHandler",
					TypeIdValue(HttpClientBasic::GetTypeId()),
					MakeTypeIdAccessor(&HttpClientCollection::GetHttpClientFactoryType, &HttpClientCollection::SetHttpClientFactoryType),
					MakeTypeIdChecker());
	return tid;
}

HttpClientCollection::HttpClientCollection(): m_peerPort(0), m_count(0), m_running(true) {
	NS_LOG_FUNCTION(this);
}

HttpClientCollection::~HttpClientCollection() {
	NS_LOG_FUNCTION(this);
}

void HttpClientCollection::DoDispose() {
	NS_LOG_FUNCTION(this);
	Application::DoDispose();
}

void HttpClientCollection::DoInitialize (void) {
	NS_LOG_FUNCTION(this);
	Application::DoInitialize();
//	if(!m_httpClientFactory.IsTypeIdSet())
	if(!m_httpClientFactory.GetTypeId().GetUid ()){
//		m_reqHandlerFactory = Create<BaseHttpRequestHandlerFactory>();
		m_httpClientFactory.SetTypeId(HttpClientBasic::GetTypeId());
	}
}

void HttpClientCollection::StopApplication() {
	NS_LOG_FUNCTION(this);

//	if (m_socket != 0) {
//		m_socket->Close();
//		m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
//	}
	if(m_httpClient){
		m_count = 0;
		m_httpClient->StopConnection();
	}
}

void
HttpClientCollection::onConnectionErrorClosed(Ptr<Object> blob) {
  StopApplication();
}
void HttpClientCollection::onConnectionClosed(Ptr<Object> blob) {
	NS_LOG_FUNCTION(this);
	if(!m_running) return;
	StartNextConnection();
}

void HttpClientCollection::StartApplication() {
	NS_LOG_FUNCTION(this);
	if(!m_running) return;
	StartNextConnection();
}

void HttpClientCollection::StartNextConnection() {
	NS_LOG_FUNCTION(this);
	if(!m_running) return;

	if(m_count <= 0) {
		m_httpClient = NULL;
//		StopApplication();
		return;
	}

	if(m_httpClient == 0) {
		m_httpClient = m_httpClientFactory.Create<HttpClientBasic>();
		m_httpClient->SetCollectionCB(MakeCallback(&HttpClientCollection::onConnectionClosed, this).Bind(Ptr<Object>()),
		                              MakeCallback(&HttpClientCollection::onConnectionErrorClosed, this).Bind(Ptr<Object>()),
		                              GetNode());
	}
	m_httpClient->InitConnection(m_peerAddress, m_peerPort);
	m_httpClient->Connect();
	m_count --;
}

void HttpClientCollection::SetHttpClientFactoryType(TypeId typeId) {
	m_httpClientFactory.SetTypeId(typeId);
}

TypeId HttpClientCollection::GetHttpClientFactoryType() const{
	return m_httpClientFactory.GetTypeId();
}

} /* namespace ns3 */
