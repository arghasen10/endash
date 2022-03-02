/*
 -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*-

 *   Copyright (c) 2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation;
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *   Author:  Biljana Bojovic <bbojovic@cttc.es>
 *
 */

#include "ns3/core-module.h"
#include "ns3/config-store.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/log.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/config-store-module.h"
#include "ns3/mmwave-mac-scheduler-tdma-rr.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("3gppChannelFdmBandwidthPartsExample");

class DemoTcpServer;
class DemoTcpClient;

int
main (int argc, char *argv[])
{
  bool udpFullBuffer = false;
  int32_t fixedMcs = -1;
  uint16_t gNbNum = 9;
  uint16_t ueNum = 2;
  bool cellScan = false;
  double beamSearchAngleStep = 10.0;
  uint16_t numerologyBwp1 = 4;
  double frequencyBwp1 = 28e9;
  double bandwidthBwp1 = 100e6;
  uint16_t numerologyBwp2 = 2;
  double frequencyBwp2 = 28e9;
  double bandwidthBwp2 = 100e6;
  uint32_t udpPacketSizeUll = 100;
  uint32_t udpPacketSizeBe = 1252;
  uint32_t lambdaUll = 10000;
  uint32_t lambdaBe = 1000;
  bool singleBwp = false;
  std::string simTag = "default";
  std::string outputDir = "./";
  double totalTxPower = 4;
  bool logging = false;
  uint16_t interGNbGap = 30;
  bool logPcap = false;

  double simTime = 20; // seconds
  double appStartTime = 1.4; //seconds

  CommandLine cmd;

  cmd.AddValue ("simTime", "Simulation time", simTime);
  cmd.AddValue ("udpFullBuffer", "Whether to set the full buffer traffic; if this parameter is "
                "set then the udpInterval parameter will be neglected.",
                udpFullBuffer);
  cmd.AddValue ("fixedMcs", "The MCS that will be used in this example, -1 for auto", fixedMcs);
  cmd.AddValue ("gNbNum", "The number of gNbs in multiple-ue topology", gNbNum);
  cmd.AddValue ("ueNum", "The number of UEs", ueNum);
  cmd.AddValue ("cellScan", "Use beam search method to determine beamforming vector,"
                " the default is long-term covariance matrix method"
                " true to use cell scanning method, false to use the default"
                " power method.",
                cellScan);
  cmd.AddValue ("beamSearchAngleStep", "Beam search angle step for beam search method", beamSearchAngleStep);
  cmd.AddValue ("numerologyBwp1", "The numerology to be used in bandwidth part 1", numerologyBwp1);
  cmd.AddValue ("frequencyBwp1", "The system frequency to be used in bandwidth part 1", frequencyBwp1);
  cmd.AddValue ("bandwidthBwp1", "The system bandwidth to be used in bandwidth part 1", bandwidthBwp1);
  cmd.AddValue ("numerologyBwp2", "The numerology to be used in bandwidth part 2", numerologyBwp2);
  cmd.AddValue ("frequencyBwp2", "The system frequency to be used in bandwidth part 2", frequencyBwp2);
  cmd.AddValue ("bandwidthBwp2", "The system bandwidth to be used in bandwidth part 2", bandwidthBwp2);
  cmd.AddValue ("packetSizeUll", "packet size in bytes to be used by ultra low latency traffic", udpPacketSizeUll);
  cmd.AddValue ("packetSizeBe", "packet size in bytes to be used by best effort traffic", udpPacketSizeBe);
  cmd.AddValue ("lambdaUll", "Number of UDP packets in one second for "
                "ultra low latency traffic",
                lambdaUll);
  cmd.AddValue ("lambdaBe", "Number of UDP packets in one second for best effor traffic", lambdaBe);
  cmd.AddValue ("singleBwp", "Simulate with single BWP, BWP1 configuration will be used", singleBwp);
  cmd.AddValue ("simTag", "tag to be appended to output filenames to "
                "distinguish simulation campaigns",
                simTag);
  cmd.AddValue ("outputDir", "directory where to store simulation results", outputDir);
  cmd.AddValue ("totalTxPower", "total tx power that will be proportionally assigned to "
                "bandwidth parts depending on each BWP bandwidth ",
                totalTxPower);
  cmd.AddValue ("logging", "Enable logging", logging);

  cmd.AddValue ("interGNbGap", "Gap between two node", interGNbGap);
  cmd.AddValue ("logPcap", "Whether pcap files need to be store or not [0]", logPcap);

  cmd.Parse (argc, argv);
  NS_ABORT_IF(frequencyBwp1 < 6e9 || frequencyBwp1 > 100e9);
  NS_ABORT_IF(frequencyBwp2 < 6e9 || frequencyBwp2 > 100e9);

  // enable logging or not
  if (logging)
    {
      LogComponentEnable ("MmWave3gppPropagationLossModel", LOG_LEVEL_ALL);
      LogComponentEnable ("MmWave3gppBuildingsPropagationLossModel", LOG_LEVEL_ALL);
      LogComponentEnable ("MmWave3gppChannel", LOG_LEVEL_ALL);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("LtePdcp", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::ChannelCondition", StringValue ("l"));
  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::Scenario", StringValue ("UMi-StreetCanyon")); // with antenna height of 10 m
  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::Shadowing", BooleanValue (false));

  Config::SetDefault ("ns3::MmWave3gppChannel::CellScan", BooleanValue (cellScan));
  Config::SetDefault ("ns3::MmWave3gppChannel::BeamSearchAngleStep", DoubleValue (beamSearchAngleStep));

  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (999999999));

  Config::SetDefault ("ns3::PointToPointEpcHelper::S1uLinkDelay", TimeValue (MilliSeconds (0)));

  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (Seconds (2)));

  if (singleBwp)
    {
      Config::SetDefault ("ns3::MmWaveHelper::NumberOfComponentCarriers", UintegerValue (1));
    }
  else
    {
      Config::SetDefault ("ns3::MmWaveHelper::NumberOfComponentCarriers", UintegerValue (2));
    }

  Config::SetDefault ("ns3::BwpManagerAlgorithmStatic::NGBR_LOW_LAT_EMBB", UintegerValue (0));
  Config::SetDefault ("ns3::BwpManagerAlgorithmStatic::GBR_CONV_VOICE", UintegerValue (1));

  // create base stations and mobile terminals
  NodeContainer gNbNodes;
  NodeContainer ueNodes;
  MobilityHelper gNbMobility, ueMobility;

  double gNbHeight = 10;
  double ueHeight = 1.5;

  gNbNodes.Create (gNbNum);
  ueNodes.Create (ueNum);
  for (uint32_t i = 0; i < gNbNum; i++)
    {
      std::cout << "gNb:" << i << " : " << gNbNodes.Get (i)->GetId () << std::endl;
    }
  for (uint32_t i = 0; i < ueNodes.GetN (); i++)
    {
      std::cout << "ue:" << i << " : " << ueNodes.Get (i)->GetId () << std::endl;
    }

  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator> ();

  Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator> ();
  Ptr<ListPositionAllocator> staVelocityAlloc = CreateObject<ListPositionAllocator> ();

  std::vector<double> apPositions;

  int32_t yValue = 0.0;

  int32_t xMin, xMax, yMin, yMax;

  xMin = -interGNbGap;
  xMax = interGNbGap;
  yMin = yMax = 0;

  for (uint32_t i = 1; i <= gNbNodes.GetN (); ++i)
    {
      // 2.0, -2.0, 6.0, -6.0, 10.0, -10.0, ....
      if (i % 2 != 0)
        {
          yValue = static_cast<int> (i) * interGNbGap;
          yMax = yValue + 30; //bounding box
        }
      else
        {
          yValue = -yValue;
          yMin = yValue - 30; //bounding box
        }

      apPositionAlloc->Add (Vector (0.0, yValue, gNbHeight));
      apPositions.push_back (yValue);

    }

  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      int32_t staXVal, staYVal;
      uint32_t speedX = 0;
      uint32_t speedY = 10;

      uint32_t gI = (i / 2) % (gNbNodes.GetN ());
      staYVal = apPositions[gI];
      if (i % 2 == 0)
        {
          staXVal = xMin;
//          speedX = 10;
        }
      else
        {
          staXVal = xMax;
//          speedX = -10;
        }
      staPositionAlloc->Add (Vector (staXVal, staYVal, ueHeight));
      staVelocityAlloc->Add (Vector (speedX, speedY, 0));
    }

  gNbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  gNbMobility.SetPositionAllocator (apPositionAlloc);
  gNbMobility.Install (gNbNodes);

  ueMobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  ueMobility.SetPositionAllocator (staPositionAlloc);
  ueMobility.Install (ueNodes);
  std::cout << "Bounds are " << Rectangle (xMin, xMax, yMin, yMax) << std::endl;
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      auto ptr = ueNodes.Get(i) -> GetObject<ConstantVelocityMobilityModel>();
      ptr -> SetVelocity(staVelocityAlloc->GetNext());
    }

  // setup the mmWave simulation
  Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper> ();
  mmWaveHelper->SetAttribute ("PathlossModel", StringValue ("ns3::MmWave3gppPropagationLossModel"));
  mmWaveHelper->SetAttribute ("ChannelModel", StringValue ("ns3::MmWave3gppChannel"));

  Ptr<MmWavePhyMacCommon> phyMacCommonBwp1 = CreateObject<MmWavePhyMacCommon> ();
  phyMacCommonBwp1->SetCentreFrequency (frequencyBwp1);
  phyMacCommonBwp1->SetBandwidth (bandwidthBwp1);
  phyMacCommonBwp1->SetNumerology (numerologyBwp1);
  phyMacCommonBwp1->SetAttribute ("MacSchedulerType", TypeIdValue (MmWaveMacSchedulerTdmaRR::GetTypeId ()));
  phyMacCommonBwp1->SetCcId (0);

  BandwidthPartRepresentation repr1 (0, phyMacCommonBwp1, nullptr, nullptr, nullptr);
  mmWaveHelper->AddBandwidthPart (0, repr1);

  // if not single BWP simulation add second BWP configuration
  if (!singleBwp)
    {
      Ptr<MmWavePhyMacCommon> phyMacCommonBwp2 = CreateObject<MmWavePhyMacCommon> ();
      phyMacCommonBwp2->SetCentreFrequency (frequencyBwp2);
      phyMacCommonBwp2->SetBandwidth (bandwidthBwp2);
      phyMacCommonBwp2->SetNumerology (numerologyBwp2);
      phyMacCommonBwp2->SetCcId (1);
      BandwidthPartRepresentation repr2 (1, phyMacCommonBwp2, nullptr, nullptr, nullptr);
      mmWaveHelper->AddBandwidthPart (1, repr2);
    }

  Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper> ();
  mmWaveHelper->SetEpcHelper (epcHelper);
  mmWaveHelper->Initialize ();

  // install mmWave net devices
  NetDeviceContainer enbNetDev = mmWaveHelper->InstallEnbDevice (gNbNodes);
  NetDeviceContainer ueNetDev = mmWaveHelper->InstallUeDevice (ueNodes);

  double x = pow (10, totalTxPower / 10);

  double totalBandwidth = 0;

  if (singleBwp)
    {
      totalBandwidth = bandwidthBwp1;
    }
  else
    {
      totalBandwidth = bandwidthBwp1 + bandwidthBwp2;
    }

  for (uint32_t j = 0; j < enbNetDev.GetN (); ++j)
    {
      ObjectMapValue objectMapValue;
      Ptr<MmWaveEnbNetDevice> netDevice = DynamicCast<MmWaveEnbNetDevice> (enbNetDev.Get (j));
      netDevice->GetAttribute ("ComponentCarrierMap", objectMapValue);
      for (uint32_t i = 0; i < objectMapValue.GetN (); i++)
        {
          Ptr<ComponentCarrierGnb> bandwidthPart = DynamicCast<ComponentCarrierGnb> (objectMapValue.Get (i));
          if (i == 0)
            {
              bandwidthPart->GetPhy ()->SetTxPower (10 * log10 ((bandwidthBwp1 / totalBandwidth) * x));
              std::cout << "\n txPower1 = " << 10 * log10 ((bandwidthBwp1 / totalBandwidth) * x) << std::endl;
            }
          else if (i == 1)
            {
              bandwidthPart->GetPhy ()->SetTxPower (10 * log10 ((bandwidthBwp2 / totalBandwidth) * x));
              std::cout << "\n txPower2 = " << 10 * log10 ((bandwidthBwp2 / totalBandwidth) * x) << std::endl;
            }
          else
            {
              std::cout << "\n Please extend power assignment for additional bandwidht parts...";
            }
        }
    }

  // create the internet and install the IP stack on the UEs
  // get SGW/PGW and create a single RemoteHost
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // connect a remoteHost to pgw. Setup routing too
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.000)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueNetDev));

  // Set the default gateway for the UEs
  for (uint32_t j = 0; j < ueNodes.GetN (); ++j)
    {
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (j)->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // assign IP address to UEs, and install UDP downlink applications
  uint16_t dlPort = 1234;
  ApplicationContainer clientApps, serverApps;

  Ptr<Application> sapp = CreateObject<DemoTcpServer> (dlPort);
  remoteHost->AddApplication (sapp);
  serverApps.Add (sapp);

  // configure here UDP traffic
  for (uint32_t j = 0; j < ueNodes.GetN (); ++j)
    {
      Ptr<Application> capp = CreateObject<DemoTcpClient> (internetIpIfaces.GetAddress (1), dlPort);
      ueNodes.Get (j)->AddApplication (capp);
      clientApps.Add (capp);

      Ptr<EpcTft> tft = Create<EpcTft> ();
      EpcTft::PacketFilter dlpf;
      dlpf.localPortStart = dlPort;
      dlpf.localPortEnd = dlPort;
//      dlPort++;`
      tft->Add (dlpf);

      enum EpsBearer::Qci q;

      if (j % 2 == 0)
        {
          q = EpsBearer::NGBR_LOW_LAT_EMBB;
        }
      else
        {
          q = EpsBearer::GBR_CONV_VOICE;
        }

      EpsBearer bearer (q);
      mmWaveHelper->ActivateDedicatedEpsBearer (ueNetDev.Get (j), bearer, tft);
    }
  // start server and client apps
  serverApps.Start (Seconds (appStartTime));
  clientApps.Start (Seconds (appStartTime + 1));
  serverApps.Stop (Seconds (simTime));
  clientApps.Stop (Seconds (simTime));

  // attach UEs to the closest eNB
  mmWaveHelper->AttachToClosestEnb (ueNetDev, enbNetDev);

  // enable the traces provided by the mmWave module
  //mmWaveHelper->EnableTraces();

  FlowMonitorHelper flowmonHelper;
  NodeContainer endpointNodes;
  endpointNodes.Add (remoteHost);
  endpointNodes.Add (ueNodes);

  Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install (endpointNodes);
  monitor->SetAttribute ("DelayBinWidth", DoubleValue (0.001));
  monitor->SetAttribute ("JitterBinWidth", DoubleValue (0.001));
  monitor->SetAttribute ("PacketSizeBinWidth", DoubleValue (20));

  if (!outputDir.empty () && logPcap)
    p2ph.EnablePcapAll (outputDir + "/nr-p2p");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}

class DemoTcpServer : public Application
{
private:
  Ptr<Socket> m_socket;
  uint16_t m_port;
public:
  DemoTcpServer (uint16_t port) :
      m_port (port)
  {
  }
  ~DemoTcpServer ()
  {
    m_socket = 0;
  }
  void
  StartApplication (void)
  {
    if (m_socket != 0)
      return;

    TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->Listen ();

    // Accept connection requests from remote hosts.
    m_socket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address&> (),
                                 MakeCallback (&DemoTcpServer::HandleAccept, this));
  }
  void
  StopApplication ()
  {

    if (m_socket != 0)
      {
        m_socket->Close ();
        m_socket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address&> (),
                                     MakeNullCallback<void, Ptr<Socket>, const Address&> ());
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
        m_socket->SetSendCallback (MakeNullCallback<void, Ptr<Socket>, uint32_t> ());
        m_socket = 0;
      }
    std::cout << "Application started at: " << GetNode ()->GetId () << std::endl;
  }

  void
  HandleAccept (Ptr<Socket> s, const Address &from)
  {
    if (m_socket == 0)
      return;

    std::cout << "new connection rcved " << GetNode ()->GetId () << std::endl;
    s->SetRecvCallback (MakeCallback (&DemoTcpServer::HandleRecv, this));
    s->SetSendCallback (MakeCallback (&DemoTcpServer::HandleSend, this));
    HandleSend (s, s->GetTxAvailable ());
  }

  void
  HandleRecv (Ptr<Socket> socket)
  {
    if (m_socket == 0)
      return;
    uint8_t buf[2048];
    uint16_t len = 2048;
    len = socket->Recv (buf, len, 0);
//    std::cerr << "recved at server " << len << "\r";
  }

  void
  HandleSend (Ptr<Socket> socket, uint32_t txSpace)
  {
    if (m_socket == 0)
      return;
    uint8_t buf[2048];
    uint16_t len = 2048;
    len = socket->Send (buf, std::min (txSpace, (uint32_t) len), 0);
//    std::cerr << "sent at server " << len << "\r";
  }
};

class DemoTcpClient : public Application
{
private:
  Ptr<Socket> m_socket;
  Address m_peerAddress; //!< Remote peer address
  uint16_t m_peerPort; //!< Remote peer port

public:
  DemoTcpClient (Address peerAddress, uint16_t peerPort) :
      m_peerAddress (peerAddress), m_peerPort (peerPort)
  {
  }
  ~DemoTcpClient ()
  {
    m_socket = 0;
  }
  void
  StartApplication ()
  {
    TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket (m_node, tid);

    m_socket->SetRecvCallback (MakeCallback (&DemoTcpClient::HandleRecv, this));
    m_socket->SetSendCallback (MakeCallback (&DemoTcpClient::HandleSend, this));

    m_socket->SetConnectCallback (MakeCallback (&DemoTcpClient::Success, this),
                                  MakeCallback (&DemoTcpClient::Error, this));

    m_socket->SetCloseCallbacks (MakeCallback (&DemoTcpClient::Success, this),
                                 MakeCallback (&DemoTcpClient::Error, this));
    auto ret = m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
    std::cout << "Application started at: " << GetNode ()->GetId () << " connected with: " << ret << std::endl;
  }

  void
  Success (Ptr<Socket> s)
  {
    std::cout << "Connection successfull in node:" << GetNode ()->GetId () << std::endl;
  }

  void
  Error (Ptr<Socket> s)
  {
    std::cout << "Connection Error in node:" << GetNode ()->GetId () << std::endl;
  }

  void
  HandleRecv (Ptr<Socket> socket)
  {
    uint8_t buf[2048];
    uint16_t len = 2048;
    len = socket->Recv (buf, len, 0);
//    std::cerr << "recved at client " << len << "\r";
  }

  void
  HandleSend (Ptr<Socket> socket, uint32_t txSpace)
  {
    uint8_t buf[2048];
    uint16_t len = 2048;
    len = socket->Send (buf, std::min (txSpace, (uint32_t) len), 0);
//    std::cerr << "sent at client " << len << "\r";
  }
};
