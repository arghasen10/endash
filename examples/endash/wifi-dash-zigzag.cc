#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/bridge-helper.h"
#include "ns3/dash-helper.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-net-device.h"
#include "ns3/sta-wifi-mac.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

struct ProgramConfigurations
{
  uint16_t nAp;
  uint16_t nSta;
  uint16_t interApGap;
  std::string simTag;
  std::string outputDir;
  bool logging;
  std::string animFile;
  uint32_t downloadSize;
  uint16_t numDownload;
  std::string nodeTraceFile;
  double nodeTraceInterval;
  bool logPcap;
//  std::string vidPath;
};

void
onStart (int *count)
{
  std::cout << "Starting at:" << Simulator::Now ().GetSeconds() << std::endl;
  (*count)++;
}
void
onStop (int *count)
{
  std::cout << "Stoping at:" << Simulator::Now ().GetSeconds() << std::endl;
  (*count)--;
  if (!(*count))
    {
      Simulator::Stop ();
    }
}

bool
isDir (std::string path)
{
  struct stat statbuf;
  if (stat (path.c_str (), &statbuf) != 0)
    return false;
  return S_ISDIR(statbuf.st_mode);
}


std::string
readNodeTrace (NodeContainer *apNodes, Ptr<Node> node, bool firstLine = false)
{
//  apNodes.Get(0)->GetDevice(0)->GetAddress()
  const std::string rrcStates[] =
    {
        "IDLE_START",
        "IDLE_CELL_SEARCH",
        "IDLE_WAIT_MIB_SIB1",
        "IDLE_WAIT_MIB",
        "IDLE_WAIT_SIB1",
        "IDLE_CAMPED_NORMALLY",
        "IDLE_WAIT_SIB2",
        "IDLE_RANDOM_ACCESS",
        "IDLE_CONNECTING",
        "CONNECTED_NORMALLY",
        "CONNECTED_HANDOVER",
        "CONNECTED_PHY_PROBLEM",
        "CONNECTED_REESTABLISHING",
        "NUM_STATES"
    };

  if (firstLine)
    {
      return "nodeId,velo_x,velo_y,pos_x,pos_y,chId,#dev,Freq,ChNo,rxGain,rxSensitivity,linkup,bssid,";
    }

  std::stringstream stream;
  stream << std::to_string (node->GetId ());
  auto mModel = node->GetObject<MobilityModel> ();
//  auto pos = mModel->GetPosition ();
  stream << "," << mModel->GetVelocity ().x << "," << mModel->GetVelocity ().y;
  stream << "," << mModel->GetPosition ().x << "," << mModel->GetPosition ().y;

  Ptr<WifiNetDevice> netDevice;   // = node->GetObject<MmWaveUeNetDevice>();
  for (uint32_t i = 0; i < node->GetNDevices (); i++)
    {
      auto nd = node->GetDevice (i);
      if (nd->GetInstanceTypeId () == WifiNetDevice::GetTypeId ())
        {
          netDevice = DynamicCast<WifiNetDevice> (nd);
          break;
        }
      std::cout << "\tNode: " << node->GetId () << ", Device " << i << ": "
          << node->GetDevice (i)->GetInstanceTypeId () << std::endl;
    }

//  auto fuck = node->GetObject<Ipv4L3Protocol>();

  stream << "," << std::to_string (netDevice->GetChannel ()->GetId());
  stream << "," << std::to_string (netDevice->GetChannel ()->GetNDevices());
  Ptr< WifiPhy > phy = netDevice->GetPhy();
  stream << "," << std::to_string(phy->GetFrequency ());
  stream << "," << std::to_string(phy->GetChannelNumber ());
  stream << "," << std::to_string(phy->GetRxGain ());
  stream << "," << std::to_string(phy->GetRxSensitivity ());
  stream << "," << std::to_string(netDevice->IsLinkUp());

  Ptr<StaWifiMac> mac = DynamicCast<StaWifiMac>(netDevice->GetMac());

  Mac48Address remAddress = mac->GetBssid();
//  uint32_t id = 128;
//  Ptr<Node> ap;
//  if(mac->IsAssociated())
//  for(uint32_t i = 0; i < apNodes->GetN(); i++) {
//      auto ptr = apNodes->Get(i);
//      for(uint32_t j = 0; i < ptr->GetNDevices(); i++) {
//          auto device = ptr->GetDevice(j);
//          if(device->GetInstanceTypeId() == WifiNetDevice::GetTypeId()) {
//              Ptr<WifiNetDevice> wd = DynamicCast<WifiNetDevice>(device);
//              if(wd->GetMac()->GetAddress() == remAddress) {
//                  ap = ptr;
//                  id = i;
//                  break;
//              }
//          }
//      }
//      if(id != 128) break;
//  }

//  stream << "," << !ap ? "-1" : std::to_string(ap->GetId());
  stream << "," << remAddress;
//  stream << "," << id;

  for(uint32_t i = 0; i < apNodes->GetN(); i++)
    {
      auto ap = apNodes->Get(i);
      auto apModel = ap->GetObject<MobilityModel> ();
//      auto appos = apModel->GetPosition();
      stream << "," << mModel->GetDistanceFrom(apModel);
    }


  return stream.str ();
}

void AssocDeassoc(Ptr<Node> node, bool assoc, Mac48Address val1) {
  std::cout << "time: " << Simulator::Now().GetSeconds() << " assoc: " << std::to_string(assoc) << " with: " << val1 << std::endl;
}

int
main (int argc, char *argv[])
{
  ProgramConfigurations conf =
      {
          .nAp = 3,
          .nSta = 3,
          .interApGap = 30,
          .simTag = "default",
          .outputDir = "",
          .logging = false,
          .animFile = "",
          .downloadSize = 0xFFFFFF,
          .numDownload = 1,
          .nodeTraceFile = "trace",
          .nodeTraceInterval = 1,
          .logPcap = false,
//          .vidPath = ""
      };


  CommandLine cmd;

  cmd.AddValue ("nAp", "The number of Ap in multiple-ue topology",
                conf.nAp);
  cmd.AddValue ("nSta", "The number of sta multiple-sta topology", conf.nSta);
  cmd.AddValue ("interApGap", "Gap between two node", conf.interApGap);
  cmd.AddValue ("downloadSize", "Data to be downloaded in a session (in byte)",
                conf.downloadSize);
  cmd.AddValue ("numDownload", "Number of download session per ue",
                conf.numDownload);
  cmd.AddValue ("nodeTraceFile", "Trace file path prefix", conf.nodeTraceFile);
  cmd.AddValue ("nodeTraceInterval", "Node trace interval",
                conf.nodeTraceInterval);
  cmd.AddValue (
      "simTag",
      "tag to be appended to output filenames to distinguish simulation campaigns",
      conf.simTag);
  cmd.AddValue ("outputDir", "directory where to store simulation results",
                conf.outputDir);
  cmd.AddValue ("logging", "Enable logging", conf.logging);
  cmd.AddValue ("logPcap", "Whether pcap files need to be store or not [0]",
                conf.logPcap);
//  cmd.AddValue ("vidPath", "Video information path", conf.vidPath);

  cmd.Parse (argc, argv);

//  NS_ASSERT_MSG(!conf.vidPath.empty(), "vidPath can't be empty");

  double udpAppStartTime = 0.4; //seconds


  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1442));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(10485760));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(10485760));


  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);


  NodeContainer staNodes;
  //  staNodes.Create (nWifi);
  NodeContainer apNodes;
  //  wifiApNode.Create(1); //n0


  //===========================================================
  double apHeight = 10;
  //  double staHeight = 1.5;

  apNodes.Create (conf.nAp);
  staNodes.Create (conf.nSta);
  for (uint32_t i = 0; i < conf.nAp; i++)
    {
      std::cout << "nAp:" << i << " : " << apNodes.Get (i)->GetId () << std::endl;
    }
  for (uint32_t i = 0; i < conf.nSta; i++)
    {
      std::cout << "sta:" << i << " : " << staNodes.Get (i)->GetId () << std::endl;
    }

  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator> ();

  Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator> ();
  Ptr<ListPositionAllocator> staVelocityAlloc = CreateObject<ListPositionAllocator> ();

  std::vector<double> apPositions;

  int32_t yValue = 0.0;

  int32_t xMin, xMax, yMin, yMax;

  xMin = -conf.interApGap;
  xMax = conf.interApGap;
  yMin = yMax = 0;

  for (uint32_t i = 1; i <= apNodes.GetN (); ++i)
    {
      // 2.0, -2.0, 6.0, -6.0, 10.0, -10.0, ....
      if (i % 2 != 0)
        {
          yValue = static_cast<int> (i) * conf.interApGap; //TODO change it
          yMax = yValue + 30; //bounding box
        }
      else
        {
          yValue = -yValue;
          yMin = yValue - 30; //bounding box
        }

      apPositionAlloc->Add (Vector (0.0, yValue, apHeight));
      apPositions.push_back(yValue);

    }

  for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
      int32_t staXVal, staYVal;
      int32_t speedX = 0;
      int32_t speedY = 10;

      int32_t gI = (i/2) % (apNodes.GetN());
      staYVal = apPositions[gI];
      if(i % 2 == 0) {
          staXVal = xMin;
          speedX = 10;
      }
      else
        {
          staXVal = xMax;
          speedX = -10;
        }
      staPositionAlloc->Add(Vector(staXVal, staYVal, apHeight));
      staVelocityAlloc->Add(Vector(speedX, speedY, 0));
    }


  MobilityHelper apMobility, staMobility;

  apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  apMobility.SetPositionAllocator (apPositionAlloc);
  apMobility.Install (apNodes);

  apMobility.Install(remoteHostContainer);

//  staMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
//                                "Bounds", RectangleValue (Rectangle (xMin, xMax, yMin, yMax)));
  staMobility.SetMobilityModel ( "ns3::ConstantSpeedZigzagBoxMobilityModel",
                                 "Bounds", RectangleValue (Rectangle (xMin, xMax, yMin, yMax)),
                                 "VelocityAllocator", PointerValue(staVelocityAlloc));

  staMobility.SetPositionAllocator (staPositionAlloc);
  staMobility.Install (staNodes);

  if (!isDir (conf.outputDir))
    {
      mkdir (conf.outputDir.c_str (), S_IRWXU);
    }

  //===========================================================

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
//  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
//  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (2.4e9)); //wireless range limited to 5 meters!
//  channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(50.0));
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();

#ifndef OLD_NS3
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
#endif
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
//  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
//  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

#ifdef OLD_NS3
  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
#else
  WifiMacHelper mac;
#endif
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNodes);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);
  //===========================================================
  //  InternetStackHelper stack;
  // stack.Install (csmaNodes);

  InternetStackHelper stack;
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  stack.Install(remoteHostContainer);
//  NetDeviceContainer remoteHostDevice = csma.Install(remoteHostContainer);

  stack.Install (apNodes);
  NetDeviceContainer backboneDevices;
  NetDeviceContainer bridgeDevices;
  backboneDevices = csma.Install (NodeContainer(apNodes, remoteHost));
  for(uint32_t i=0; i < conf.nAp; i++) {
      BridgeHelper bridge;
      NetDeviceContainer bridgeDev;
      bridgeDev = bridge.Install (apNodes.Get (i), NetDeviceContainer (apDevices.Get(i), backboneDevices.Get (i)));
      bridgeDevices.Add(bridgeDev);
  }

  stack.Install (staNodes);

  Ipv4AddressHelper address;


  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer remotehostIpIfaces = address.Assign(backboneDevices.Get(conf.nAp));
  Ipv4InterfaceContainer wifiInterface = address.Assign (bridgeDevices);
  address.Assign (staDevices);
  //===========================================================
  std::cout << remotehostIpIfaces.GetAddress(0) << " Num b:" << bridgeDevices.GetN() << " sta:" << staDevices.GetN() << " nAp:" << conf.nAp << std::endl;
  std::cout << "BbN:" << apNodes.GetN() << " bbDev:" << backboneDevices.GetN() << " apdev:" << apDevices.GetN() << std::endl;
  //===========================================================

  uint16_t dlPort = 1234;
  ApplicationContainer clientApps, serverApps;

  auto trackNode = staNodes.Get(0);
  std::string trcSrc = "/NodeList/" + std::to_string(trackNode->GetId ()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/";
  Config::ConnectWithoutContext(trcSrc + "Assoc", MakeBoundCallback(AssocDeassoc, trackNode, true));
  Config::ConnectWithoutContext(trcSrc + "DeAssoc", MakeBoundCallback(AssocDeassoc, trackNode, false));


  //  ApplicationContainer clientAppsEmbb, serverAppsEmbb;

  DashServerHelper dashSrHelper (dlPort);
  serverApps.Add (dashSrHelper.Install (remoteHost));

  int counter = 0;
  DashClientHelper dlClient (remotehostIpIfaces.GetAddress (0), dlPort); //Remotehost is the second node, pgw is first
  dlClient.SetAttribute ("OnStartCB",
                         CallbackValue (MakeBoundCallback (onStart, &counter)));
  dlClient.SetAttribute ("OnStopCB",
                         CallbackValue (MakeBoundCallback (onStop, &counter)));
  dlClient.SetAttribute ("NodeTraceInterval",
                         TimeValue (Seconds (conf.nodeTraceInterval)));
    dlClient.SetAttribute ("NodeTraceHelperCallBack", CallbackValue (MakeCallback (readNodeTrace)));
  dlClient.SetAttribute ("NodeTraceHelperCallBack",
                         CallbackValue (MakeBoundCallback (readNodeTrace, &apNodes)));
  dlClient.SetAttribute ("Timeout", TimeValue(Seconds(-1)));

  if (!conf.outputDir.empty ()) {
      dlClient.SetAttribute ("NodeTracePath", StringValue (conf.outputDir + "/" + conf.nodeTraceFile));
    dlClient.SetAttribute ("TracePath", StringValue (conf.outputDir + "/TraceData"));
    dlClient.SetAttribute("AbrLogPath", StringValue (conf.outputDir + "/AbrData"));
  }

//  dlClient.SetAttribute("VideoFilePath",
//        StringValue(conf.vidPath));

  clientApps = dlClient.Install (staNodes);

  serverApps.Start (Seconds (udpAppStartTime));
  clientApps.Start (Seconds (udpAppStartTime + 1));
  //===========================================================

  if (!conf.outputDir.empty () && conf.logPcap) {
    csma.EnablePcapAll (conf.outputDir + "/nr-csma");
    phy.EnablePcapAll(conf.outputDir + "/nr-wifi");

    AnimationInterface anim(conf.outputDir + "/anim.xml");
    anim.EnablePacketMetadata ();

    for (uint32_t i = 0; i < staNodes.GetN (); ++i)
        {
          anim.UpdateNodeDescription (staNodes.Get (i), "STA"); // Optional
          anim.UpdateNodeColor (staNodes.Get (i), 255, 0, 0); // Optional
        }
      for (uint32_t i = 0; i < apNodes.GetN (); ++i)
        {
          anim.UpdateNodeDescription (apNodes.Get (i), "AP"); // Optional
          anim.UpdateNodeColor (apNodes.Get (i), 0, 255, 0); // Optional
        }
      for (uint32_t i = 0; i < remoteHostContainer.GetN (); ++i)
        {
          anim.UpdateNodeDescription (remoteHostContainer.Get (i), "CSMA"); // Optional
          anim.UpdateNodeColor (remoteHostContainer.Get (i), 0, 0, 255); // Optional
        }
  }


  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
