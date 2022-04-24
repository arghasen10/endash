/*
 * dash-helper.h
 *
 *  Created on: 09-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_ENDASH_HELPER_ENDASH_HELPER_H_
#define SRC_ENDASH_HELPER_ENDASH_HELPER_H_

#include "http-helper.h"

namespace ns3 {

class endashServerHelper: public HttpServerHelper {
public:
	endashServerHelper(uint16_t port);
};

} /* namespace ns3 */

namespace ns3 {

class endashClientHelper {
public:
	endashClientHelper(Address addr, uint16_t port);
	virtual ~endashClientHelper() {
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
#endif /* SRC_ENDASH_HELPER_DASH_HELPER_H_ */

