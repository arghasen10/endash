/*
 * spdash-example-bus.cc
 *
 *  Created on: 04-Apr-2020
 *      Author: abhijit
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
//#include "ns3/applications-module.h"
#include "ns3/http-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1   n2   n3   n4
//    point-to-point  |    |    |    |
//                    ================
//                      LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SecondScriptExample");

int main(int argc, char *argv[]) {
	bool verbose = true;
	uint32_t nCsma = 3;

	CommandLine cmd;
	cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
	cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);

	cmd.Parse(argc, argv);

	if (verbose) {
		LogComponentEnable("HttpClientApplication", LOG_LEVEL_INFO);
		LogComponentEnable("HttpServerApplication", LOG_LEVEL_INFO);
	}

	nCsma = nCsma == 0 ? 1 : nCsma;

	NodeContainer p2pNodes;
	p2pNodes.Create(2);

	NodeContainer csmaNodes;
	csmaNodes.Add(p2pNodes.Get(1));
	csmaNodes.Create(nCsma);

	PointToPointHelper pointToPoint;
	pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
	pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

	NetDeviceContainer p2pDevices;
	p2pDevices = pointToPoint.Install(p2pNodes);

	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
	csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

	NetDeviceContainer csmaDevices;
	csmaDevices = csma.Install(csmaNodes);

	InternetStackHelper stack;
	stack.Install(p2pNodes.Get(0));
	stack.Install(csmaNodes);

	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer p2pInterfaces;
	p2pInterfaces = address.Assign(p2pDevices);

	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer csmaInterfaces;
	csmaInterfaces = address.Assign(csmaDevices);

	HttpServerHelper httpServer(9);

	ApplicationContainer serverApps = httpServer.Install(p2pNodes.Get(0));
	serverApps.Start(Seconds(1.0));
//  serverApps.Stop (Seconds (10.0));

	HttpClientHelper httpClient(p2pInterfaces.GetAddress(0), 9);
	httpClient.SetAttribute("NumRequest", UintegerValue(6));

	ApplicationContainer clientApps = httpClient.Install(csmaNodes);
	clientApps.Start(Seconds(2.0));
//	clientApps.Stop(Seconds(10.0));

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

//	pointToPoint.EnablePcapAll("second");
	pointToPoint.EnablePcap("spdash-bus", p2pDevices.Get(0), true);

	Simulator::Run();
	Simulator::Destroy();
	return 0;
}
