/*
 * dash-helper.h
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_HELPER_SPDASH_HELPER_H_
#define SRC_SPDASH_HELPER_SPDASH_HELPER_H_

#include "http-helper.h"

namespace ns3 {

class SpDashServerHelper: public HttpServerHelper {
public:
	SpDashServerHelper(uint16_t port);
};

} /* namespace ns3 */

namespace ns3 {

class SpDashClientHelper {
public:
	SpDashClientHelper(Address addr, uint16_t port);
	virtual ~SpDashClientHelper() {
	}
	void SetAttribute(std::string name, const AttributeValue &value);
	ApplicationContainer Install(Ptr<Node> node) const;
	ApplicationContainer Install(NodeContainer nodes) const;

protected:
	Ptr<Application> InstallPriv(Ptr<Node> node) const;
	ObjectFactory m_factory;

};

} /* namespace ns3 */

namespace ns3 {

class DashHttpDownloadHelper {
public:
	DashHttpDownloadHelper(Address addr, uint16_t port);
	virtual ~DashHttpDownloadHelper();
	void SetAttribute(std::string name, const AttributeValue &value);
	ApplicationContainer Install(Ptr<Node> node) const;
	ApplicationContainer Install(NodeContainer nodes) const;

protected:
	Ptr<Application> InstallPriv(Ptr<Node> node) const;
	ObjectFactory m_factory;
};

} /* namespace ns3 */
#endif /* SRC_SPDASH_HELPER_DASH_HELPER_H_ */

