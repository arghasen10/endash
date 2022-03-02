/*
 * http-helper.cc
 *
 *  Created on: 03-Apr-2020
 *      Author: abhijit
 */

#include "http-helper.h"

#include "ns3/http-server.h"
#include "ns3/http-client-collection.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

HttpServerHelper::HttpServerHelper(uint16_t port) {
	m_factory.SetTypeId(HttpServer::GetTypeId());
	SetAttribute("Port", UintegerValue(port));

}

void HttpServerHelper::SetAttribute(std::string name,
		const AttributeValue &value) {
	m_factory.Set(name, value);
}

ApplicationContainer HttpServerHelper::Install(Ptr<Node> node) const {
	return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer HttpServerHelper::Install(
		NodeContainer nodes) const {
	ApplicationContainer apps;
	for (auto it = nodes.Begin(); it != nodes.End(); ++it) {
		apps.Add(InstallPriv(*it));
	}
	return apps;
}

Ptr<Application> HttpServerHelper::InstallPriv(Ptr<Node> node) const {
	Ptr<Application> app = m_factory.Create<HttpServer>();
	node->AddApplication(app);

	return app;
}

} /* namespace ns3 */

/*********************************************************
 *                        Client                         *
 *********************************************************/

namespace ns3 {

HttpClientHelper::HttpClientHelper(Address addr, uint16_t port) {
	m_factory.SetTypeId(HttpClientCollection::GetTypeId());
	SetAttribute("RemoteAddress", AddressValue(Address(addr)));
	SetAttribute("RemotePort", UintegerValue(port));
}

HttpClientHelper::HttpClientHelper(Ipv4Address addr, uint16_t port) {
	m_factory.SetTypeId(HttpClientCollection::GetTypeId());
	SetAttribute("RemoteAddress", AddressValue(Address(addr)));
	SetAttribute("RemotePort", UintegerValue(port));
}

HttpClientHelper::HttpClientHelper(Ipv6Address addr, uint16_t port) {
	m_factory.SetTypeId(HttpClientCollection::GetTypeId());
	SetAttribute("RemoteAddress", AddressValue(Address(addr)));
	SetAttribute("RemotePort", UintegerValue(port));
}

void HttpClientHelper::SetAttribute(std::string name,
		const AttributeValue &value) {
	m_factory.Set(name, value);
}

ApplicationContainer HttpClientHelper::Install(NodeContainer nodes) const {
	ApplicationContainer apps;
	for(auto it = nodes.Begin(); it != nodes.End(); ++it) {
		apps.Add(InstallPriv(*it));
	}
	return apps;//ApplicationContainer(InstallPriv(node));
}

ApplicationContainer HttpClientHelper::Install(Ptr<Node> node) const {
	return ApplicationContainer(InstallPriv(node));
}

Ptr<Application> HttpClientHelper::InstallPriv(Ptr<Node> node) const {
	Ptr<Application> app = m_factory.Create<HttpClientCollection>();
	node->AddApplication(app);

	return app;
}

} /* namespace ns3 */
