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
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/spdash-helper.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |  ############  10.1.1.0
// n5   n6   n7   n0 #################-------------- n1   n2   n3   n4
//                  ################ point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

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

int
main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t nCsma = 3;
  uint32_t nWifi = 3;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  // The underlying restriction of 18 is due to the grid position
  // allocator's configuration; the grid layout will exceed the
  // bounding box if more than 18 nodes are provided.
  if (nWifi > 18)
    {
      std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
      return 1;
    }

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode;
  wifiApNode.Create(1); //n0
   // = p2pNodes.Get (0);

#ifdef OLD_NS3
  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
#else
  WifiMacHelper mac;
#endif

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  // stack.Install (csmaNodes);
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;



  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterface = address.Assign (apDevices);
  address.Assign (staDevices);



  SpDashServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
  serverApps.Start(Seconds(1.0));
//  serverApps.Stop (Seconds (10.0));

  int counter = 0;
//  Get(0) = server
  SpDashClientHelper dashClient(wifiInterface.GetAddress(0), 9);

  dashClient.SetAttribute("VideoFilePath",
      StringValue("src/spdash/examples/vid.txt"));

  dashClient.SetAttribute("OnStartCB",
      CallbackValue(MakeBoundCallback(onStart, &counter)));
  dashClient.SetAttribute("OnStopCB",
      CallbackValue(MakeBoundCallback(onStop, &counter)));
  dashClient.SetAttribute("AllLogFile",StringValue("allLogs.csv"));

  dashClient.SetAttribute("ClientId",StringValue("11"));
  ApplicationContainer clientApp1 = dashClient.Install(wifiStaNodes.Get(0));
  clientApp1.Start(Seconds(2.0));

  dashClient.SetAttribute("ClientId",StringValue("12"));
  ApplicationContainer clientApp2 = dashClient.Install(wifiStaNodes.Get(1));
  clientApp2.Start(Seconds(20.0));

  dashClient.SetAttribute("ClientId",StringValue("13"));
  ApplicationContainer clientApp3 = dashClient.Install(wifiStaNodes.Get(2));
  clientApp3.Start(Seconds(100.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // csma.EnablePcapAll("spdash-wireless");
  Simulator::Run();
  Simulator::Destroy();
  return 0;

}

//mcs modulation codein
//sinr signal interference noise ratio
//rssi
//channel width mhz 20 80
//server in LAN network (link to) ap ... client in station nodes
