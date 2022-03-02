/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
//#include "ns3/applications-module.h"
//#include "ns3/http-helper.h"
#include "ns3/spdash-helper.h"
#include "ns3/type-id.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");

void onStart(int *count) {
	(*count)++;
}
void onStop(int *count) {
	(*count)--;
	if (!(*count)) {
		std::cout << "stopping at:" << Simulator::Now().GetSeconds()
				<< std::endl;
		Simulator::Stop();
	}
}

int main(int argc, char *argv[]) {
	CommandLine cmd;
	cmd.Parse(argc, argv);

	Time::SetResolution(Time::NS);
	LogComponentEnable("HttpClientBasic", LOG_LEVEL_INFO);
	LogComponentEnable("HttpServerApplication", LOG_LEVEL_INFO);
	LogComponentEnable("HttpServerBaseRequestHandler", LOG_LEVEL_INFO);

	NodeContainer nodes;
	nodes.Create(2);

	PointToPointHelper pointToPoint;
	pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
	pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

	NetDeviceContainer devices;
	devices = pointToPoint.Install(nodes);

	InternetStackHelper stack;
	stack.Install(nodes);

	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0", "255.255.255.0");

	Ipv4InterfaceContainer interfaces = address.Assign(devices);

	SpDashServerHelper echoServer(9);
	ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
	serverApps.Start(Seconds(1.0));
//  serverApps.Stop (Seconds (10.0));

	int counter = 0;

	SpDashClientHelper dashClient(interfaces.GetAddress(1), 9);
	dashClient.SetAttribute("VideoFilePath",
			StringValue("src/spdash/examples/vid.txt"));

	dashClient.SetAttribute("OnStartCB",
			CallbackValue(MakeBoundCallback(onStart, &counter)));
	dashClient.SetAttribute("OnStopCB",
			CallbackValue(MakeBoundCallback(onStop, &counter)));
//  echoClient.SetAttribute("NumRequest", UintegerValue (5));
//  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
//  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
//  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

	ApplicationContainer clientApps = dashClient.Install(nodes.Get(0));
	clientApps.Start(Seconds(2.0));

	pointToPoint.EnablePcapAll("spdash-p2p");
	Simulator::Run();
	Simulator::Destroy();
	return 0;
}
