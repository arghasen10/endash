/*
 * dash-helper.cc
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#include "dash-helper.h"
#include "ns3/dash-request-handler.h"
#include "ns3/dash-video-player.h"
#include "ns3/dash-file-downloader.h"
#include "ns3/http-server.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

DashServerHelper::DashServerHelper(uint16_t port) :
		HttpServerHelper(port) {
	ObjectFactory dashHandlerFactory;
	dashHandlerFactory.SetTypeId(DashRequestHandler::GetTypeId());
	SetAttribute("HttpRequestHandlerTypeId",
			TypeIdValue(DashRequestHandler::GetTypeId()));
}

} /* namespace ns3 */

namespace ns3 {

DashClientHelper::DashClientHelper(Address addr, uint16_t port) {
	m_factory.SetTypeId(DashVideoPlayer::GetTypeId());
	SetAttribute("RemoteAddress", AddressValue(Address(addr)));
	SetAttribute("RemotePort", UintegerValue(port));
}

void DashClientHelper::SetAttribute(std::string name,
		const AttributeValue &value) {
	m_factory.Set(name, value);
}

ApplicationContainer DashClientHelper::Install(NodeContainer nodes) const {
	ApplicationContainer apps;
	for (auto it = nodes.Begin(); it != nodes.End(); ++it) {
		apps.Add(InstallPriv(*it));
	}
	return apps;
}

ApplicationContainer DashClientHelper::Install(Ptr<Node> node) const {
	return ApplicationContainer(InstallPriv(node));
}

Ptr<Application> DashClientHelper::InstallPriv(Ptr<Node> node) const {
	Ptr<Application> app = m_factory.Create<DashVideoPlayer>();
	node->AddApplication(app);

	return app;
}

} /* namespace ns3 */

namespace ns3 {


DashHttpDownloadHelper::DashHttpDownloadHelper(Address addr, uint16_t port) {
	m_factory.SetTypeId(DashFileDownloader::GetTypeId());
	SetAttribute("RemoteAddress", AddressValue(Address(addr)));
	SetAttribute("RemotePort", UintegerValue(port));
}

DashHttpDownloadHelper::~DashHttpDownloadHelper() {
}

void DashHttpDownloadHelper::SetAttribute(std::string name,
		const AttributeValue &value) {
	m_factory.Set(name, value);
}

ApplicationContainer DashHttpDownloadHelper::Install(
		Ptr<Node> node) const {
	return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer DashHttpDownloadHelper::Install(
		NodeContainer nodes) const {
	ApplicationContainer apps;
	for (auto it = nodes.Begin(); it != nodes.End(); ++it) {
		apps.Add(InstallPriv(*it));
	}
	return apps;
}

Ptr<Application> DashHttpDownloadHelper::InstallPriv(
		Ptr<Node> node) const {
	Ptr<Application> app = m_factory.Create<DashFileDownloader>();
	node->AddApplication(app);

	return app;
}

} /* namespace ns3 */
