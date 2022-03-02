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

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include <map>

#include "http-server-base-request-handler.h"
#include "ns3/random-variable-stream.h"

namespace ns3 {

class Socket;
class Packet;


class HttpServer: public Application {
public:
	static TypeId GetTypeId(void);
	HttpServer();
	virtual ~HttpServer();

protected:
	virtual void DoDispose(void);
	virtual void DoInitialize (void);

private:
	virtual void StartApplication(void);
	virtual void StopApplication(void);

	void SetReqHandlerFactoryTypeId(TypeId tid);
	TypeId GetReqHandlerFactoryTypeId(void) const;

	void HandleAccept(Ptr<Socket> s, const Address &from);

	void HandleConnectionClose(void*);

	uint16_t m_port;
	Ptr<Socket> m_socket;
	Ptr<Socket> m_socket6;
	std::vector<Ptr<HttpServerBaseRequestHandler> > m_connectedClients;
	ObjectFactory m_reqHandlerFactory;

//	Callback<Ptr<BaseHttpRequestHandler>, Ptr<Socket>, HttpServer *> m_createReqHandler;
};


} // namespace ns3

#endif /* TCP_STREAM_SERVER_H */

