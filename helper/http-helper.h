/*
 * http-helper.h
 *
 *  Created on: 03-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_HELPER_HTTP_HELPER_H_
#define SRC_SPDASH_HELPER_HTTP_HELPER_H_

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"

namespace ns3 {

class HttpServerHelper {
public:
	HttpServerHelper(uint16_t port);
	void SetAttribute(std::string name, const AttributeValue &value);
	ApplicationContainer Install(Ptr<Node> node) const;
	ApplicationContainer Install(NodeContainer nodes) const;

protected:
	ObjectFactory& GetFactory(){return m_factory;}

private:
	Ptr<Application> InstallPriv(Ptr<Node> node) const;
	ObjectFactory m_factory;
};

} /* namespace ns3 */

namespace ns3 {

class HttpClientHelper {
public:
	HttpClientHelper(Address addr, uint16_t port);
	HttpClientHelper(Ipv4Address addr, uint16_t port);
	HttpClientHelper(Ipv6Address addr, uint16_t port);
	void SetAttribute(std::string name, const AttributeValue &value);
	ApplicationContainer Install(Ptr<Node> node) const;
	ApplicationContainer Install(NodeContainer nodes) const;

protected:
	ObjectFactory& GetFactory(){return m_factory;}

private:
	Ptr<Application> InstallPriv(Ptr<Node> node) const;
	ObjectFactory m_factory;
};

} /* namespace ns3 */
#endif /* SRC_SPDASH_HELPER_HTTP_HELPER_H_ */

