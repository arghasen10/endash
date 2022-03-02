/*
 * dash-helper.cc
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#include "spdash-helper.h"
#include "ns3/spdash-request-handler.h"
#include "ns3/spdash-video-player.h"
// #include "ns3/dash-file-downloader.h"
#include "ns3/http-server.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

SpDashServerHelper::SpDashServerHelper(uint16_t port) :
		HttpServerHelper(port) {
	ObjectFactory dashHandlerFactory;
	dashHandlerFactory.SetTypeId(SpDashRequestHandler::GetTypeId());
	SetAttribute("HttpRequestHandlerTypeId",
			TypeIdValue(SpDashRequestHandler::GetTypeId()));
}

} /* namespace ns3 */

namespace ns3 {

SpDashClientHelper::SpDashClientHelper(Address addr, uint16_t port) {
	m_factory.SetTypeId(SpDashVideoPlayer::GetTypeId());
	SetAttribute("RemoteAddress", AddressValue(Address(addr)));
	SetAttribute("RemotePort", UintegerValue(port));
}

void SpDashClientHelper::SetAttribute(std::string name,
		const AttributeValue &value) {
	m_factory.Set(name, value);
}

ApplicationContainer SpDashClientHelper::Install(NodeContainer nodes) const {
	ApplicationContainer apps;
	for (auto it = nodes.Begin(); it != nodes.End(); ++it) {
		apps.Add(InstallPriv(*it));
	}
	return apps;
}

ApplicationContainer SpDashClientHelper::Install(Ptr<Node> node) const {
	return ApplicationContainer(InstallPriv(node));
}

Ptr<Application> SpDashClientHelper::InstallPriv(Ptr<Node> node) const {
	Ptr<Application> app = m_factory.Create<SpDashVideoPlayer>();
	node->AddApplication(app);

	return app;
}

} /* namespace ns3 */

// namespace ns3 {


// DashHttpDownloadHelper::DashHttpDownloadHelper(Address addr, uint16_t port) {
// 	m_factory.SetTypeId(DashFileDownloader::GetTypeId());
// 	SetAttribute("RemoteAddress", AddressValue(Address(addr)));
// 	SetAttribute("RemotePort", UintegerValue(port));
// }

// DashHttpDownloadHelper::~DashHttpDownloadHelper() {
// }

// void DashHttpDownloadHelper::SetAttribute(std::string name,
// 		const AttributeValue &value) {
// 	m_factory.Set(name, value);
// }

// ApplicationContainer DashHttpDownloadHelper::Install(
// 		Ptr<Node> node) const {
// 	return ApplicationContainer(InstallPriv(node));
// }

// ApplicationContainer DashHttpDownloadHelper::Install(
// 		NodeContainer nodes) const {
// 	ApplicationContainer apps;
// 	for (auto it = nodes.Begin(); it != nodes.End(); ++it) {
// 		apps.Add(InstallPriv(*it));
// 	}
// 	return apps;
// }

// Ptr<Application> DashHttpDownloadHelper::InstallPriv(
// 		Ptr<Node> node) const {
// 	Ptr<Application> app = m_factory.Create<DashFileDownloader>();
// 	node->AddApplication(app);

// 	return app;
// }

// } /* namespace ns3 */
