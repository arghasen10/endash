/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2016 Technische Universitaet Berlin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "http-server.h"

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/global-value.h"
#include <ns3/core-module.h>
#include "ns3/trace-source-accessor.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("HttpServerApplication");

NS_OBJECT_ENSURE_REGISTERED(HttpServer);

/*******************************************************
 *                     HttpServer                      *
 *******************************************************/

TypeId HttpServer::GetTypeId(void) {
	static TypeId tid = TypeId("ns3::HttpServer")
			.SetParent<Application>()
			.SetGroupName("Applications")
			.AddConstructor<HttpServer>()
			.AddAttribute(
					"Port",
					"Port on which we listen for incoming packets.",
					UintegerValue(9),
					MakeUintegerAccessor(&HttpServer::m_port),
					MakeUintegerChecker<uint16_t>())
			.AddAttribute(
					"HttpRequestHandlerTypeId",
					"Call back function to create RequestHandler",
					TypeIdValue(HttpServerBaseRequestHandler::GetTypeId()),
					MakeTypeIdAccessor(&HttpServer::GetReqHandlerFactoryTypeId, &HttpServer::SetReqHandlerFactoryTypeId),
					MakeTypeIdChecker());
	return tid;
}

HttpServer::HttpServer() :
		m_port(0) {
	NS_LOG_FUNCTION(this);
}

HttpServer::~HttpServer() {
	NS_LOG_FUNCTION(this);
	m_socket = 0;
	m_socket6 = 0;
}

void HttpServer::DoDispose(void) {
	NS_LOG_FUNCTION(this);
	Application::DoDispose();
}

void HttpServer::DoInitialize (void) {
	NS_LOG_FUNCTION(this);
	Application::DoInitialize();
//	if(!m_reqHandlerFactory.IsTypeIdSet())
	if(!m_reqHandlerFactory.GetTypeId().GetUid ()){
//		m_reqHandlerFactory = Create<BaseHttpRequestHandlerFactory>();
		m_reqHandlerFactory.SetTypeId(HttpServerBaseRequestHandler::GetTypeId());
	}
}

void HttpServer::StartApplication(void) {
	NS_LOG_FUNCTION(this);

	if (m_socket == 0) {
		TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
		m_socket = Socket::CreateSocket(GetNode(), tid);
		InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(),
				m_port);
		m_socket->Bind(local);
		m_socket->Listen();
	}

	if (m_socket6 == 0) {
		TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
		m_socket6 = Socket::CreateSocket(GetNode(), tid);
		Inet6SocketAddress local6 = Inet6SocketAddress(Ipv6Address::GetAny(),
				m_port);
		m_socket6->Bind(local6);
		m_socket6->Listen();
	}

	// Accept connection requests from remote hosts.
	m_socket->SetAcceptCallback(
			MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
			MakeCallback(&HttpServer::HandleAccept, this));

}

void HttpServer::StopApplication() {
	NS_LOG_FUNCTION(this);

	if (m_socket != 0) {
		m_socket->Close();
		m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
	if (m_socket6 != 0) {
		m_socket6->Close();
		m_socket6->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
}

void HttpServer::HandleAccept(Ptr<Socket> s, const Address &from) {
	NS_LOG_FUNCTION(this << s << from);

	Ptr<HttpServerBaseRequestHandler> htsCli = m_reqHandlerFactory.Create<HttpServerBaseRequestHandler>();
//	Ptr<BaseHttpRequestHandler> htsCli = m_handlerFactory->CreateHandler(s, this);
//	htsCli->SetOnClose(MakeCallback(&HttpServer::HandleConnectionClose, this));
	htsCli->InitFromServer(s, this, MakeCallback(&HttpServer::HandleConnectionClose, this).Bind((void *)PeekPointer(htsCli)));

	m_connectedClients.push_back(htsCli);

}

void HttpServer::SetReqHandlerFactoryTypeId(TypeId tid) {
	m_reqHandlerFactory.SetTypeId(tid);
}

TypeId HttpServer::GetReqHandlerFactoryTypeId(void) const {
	return m_reqHandlerFactory.GetTypeId();
}

void HttpServer::HandleConnectionClose(void *ptr) {
	NS_LOG_FUNCTION(this);
	for (auto it = m_connectedClients.begin(); it != m_connectedClients.end(); ++it) {
		if(PeekPointer(*it) == ptr){
			m_connectedClients.erase(it);
			return;
		}
	}
}



} // Namespace ns3
