/*
 * -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*-
 *
 * cttc-nr-with-tcp.cc
 *
 *  Created on: 16-Apr-2020
 *      Author: abhijit
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
#include "ns3/dash-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("3gppChannelFdmBandwidthPartsExample");

struct ProgramConfigurations{
		uint16_t gNbNum;
		uint16_t ueNumPergNb;
		bool cellScan;
		double beamSearchAngleStep;
		uint16_t numerologyBwp1;
		double frequencyBwp1;
		double bandwidthBwp1;
		uint16_t numerologyBwp2;
		double frequencyBwp2;
		double bandwidthBwp2;
		bool singleBwp;
		std::string simTag;
		std::string outputDir;
		double totalTxPower;
		bool logging;
		std::string animFile;
};

void onStart(int *count){
	std::cout << "Starting at:" << Simulator::Now() <<std::endl;
	(*count) ++;
}
void onStop(int *count){
	std::cout << "Stoping at:" << Simulator::Now() <<std::endl;
	(*count) --;
	if(!(*count)) {
		Simulator::Stop();
	}
}

void storeFlowMonitor(Ptr<ns3::FlowMonitor> monitor, FlowMonitorHelper &flowmonHelper, ProgramConfigurations &conf) {
	// Print per-flow statistics
		monitor->CheckForLostPackets();
		Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(
				flowmonHelper.GetClassifier());
		FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

		double averageFlowThroughput = 0.0;
		double averageFlowDelay = 0.0;

		std::ofstream outFile;
		std::string filename = conf.outputDir + "/" + conf.simTag;
		outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
		if (!outFile.is_open()) {
			std::cerr << "Can't open file " << filename << std::endl;
			return;
		}

		outFile.setf(std::ios_base::fixed);

		for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
				stats.begin(); i != stats.end(); ++i) {
			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
			std::stringstream protoStream;
			protoStream << (uint16_t) t.protocol;
			if (t.protocol == 6) {
				protoStream.str("TCP");
			}
			if (t.protocol == 17) {
				protoStream.str("UDP");
			}
			double txDuration = (i->second.timeLastTxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());
			outFile << "Flow " << i->first << " (" << t.sourceAddress << ":"
					<< t.sourcePort << " -> " << t.destinationAddress << ":"
					<< t.destinationPort << ") proto " << protoStream.str() << "\n";
			outFile << "  Tx Packets: " << i->second.txPackets << "\n";
			outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
			outFile << "  TxOffered:  "
					<< i->second.txBytes * 8.0 / txDuration / 1000
							/ 1000 << " Mbps\n";
			outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
			if (i->second.rxPackets > 0) {
				// Measure the duration of the flow from receiver's perspective
				//double rxDuration = i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
				double rxDuration = (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstRxPacket.GetSeconds());

				averageFlowThroughput += i->second.rxBytes * 8.0 / rxDuration / 1000
						/ 1000;
				averageFlowDelay += 1000 * i->second.delaySum.GetSeconds()
						/ i->second.rxPackets;

				outFile << "  Throughput: "
						<< i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000
						<< " Mbps\n";
				outFile << "  Mean delay:  "
						<< 1000 * i->second.delaySum.GetSeconds()
								/ i->second.rxPackets << " ms\n";
				//outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << " Mbps \n";
				outFile << "  Mean jitter:  "
						<< 1000 * i->second.jitterSum.GetSeconds()
								/ i->second.rxPackets << " ms\n";
			} else {
				outFile << "  Throughput:  0 Mbps\n";
				outFile << "  Mean delay:  0 ms\n";
				outFile << "  Mean jitter: 0 ms\n";
			}
			outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
		}

		outFile << "\n\n  Mean flow throughput: "
				<< averageFlowThroughput / stats.size() << "\n";
		outFile << "  Mean flow delay: " << averageFlowDelay / stats.size() << "\n";

		outFile.close();

//		std::ifstream f(filename.c_str());
//
//		if (f.is_open()) {
//			std::cout << f.rdbuf();
//		}
}

int main(int argc, char *argv[]) {

	ProgramConfigurations conf = {
		.gNbNum = 1,
		.ueNumPergNb = 2,
		.cellScan = false,
		.beamSearchAngleStep = 10.0,
		.numerologyBwp1 = 4,
		.frequencyBwp1 = 28e9,
		.bandwidthBwp1 = 100e6,
		.numerologyBwp2 = 2,
		.frequencyBwp2 = 28e9,
		.bandwidthBwp2 = 100e6,
		.singleBwp = false,
		.simTag = "default",
		.outputDir = "./",
		.totalTxPower = 4,
		.logging = false,
		.animFile = "animation.xml"
	};

	double udpAppStartTime = 0.4; //seconds

	CommandLine cmd;

	cmd.AddValue("gNbNum", "The number of gNbs in multiple-ue topology",
			conf.gNbNum);
	cmd.AddValue("ueNumPergNb",
			"The number of UE per gNb in multiple-ue topology", conf.ueNumPergNb);
	cmd.AddValue("cellScan",
			"Use beam search method to determine beamforming vector,"
					" the default is long-term covariance matrix method"
					" true to use cell scanning method, false to use the default"
					" power method.", conf.cellScan);
	cmd.AddValue("beamSearchAngleStep",
			"Beam search angle step for beam search method",
			conf.beamSearchAngleStep);
	cmd.AddValue("numerologyBwp1",
			"The numerology to be used in bandwidth part 1", conf.numerologyBwp1);
	cmd.AddValue("frequencyBwp1",
			"The system frequency to be used in bandwidth part 1",
			conf.frequencyBwp1);
	cmd.AddValue("bandwidthBwp1",
			"The system bandwidth to be used in bandwidth part 1",
			conf.bandwidthBwp1);
	cmd.AddValue("numerologyBwp2",
			"The numerology to be used in bandwidth part 2", conf.numerologyBwp2);
	cmd.AddValue("frequencyBwp2",
			"The system frequency to be used in bandwidth part 2",
			conf.frequencyBwp2);
	cmd.AddValue("bandwidthBwp2",
			"The system bandwidth to be used in bandwidth part 2",
			conf.bandwidthBwp2);
	cmd.AddValue("singleBwp",
			"Simulate with single BWP, BWP1 configuration will be used",
			conf.singleBwp);
	cmd.AddValue("simTag",
			"tag to be appended to output filenames to distinguish simulation campaigns",
			conf.simTag);
	cmd.AddValue("outputDir", "directory where to store simulation results",
			conf.outputDir);
	cmd.AddValue("totalTxPower",
			"total tx power that will be proportionally assigned to"
					" bandwidth parts depending on each BWP bandwidth ",
					conf.totalTxPower);
	cmd.AddValue("logging", "Enable logging", conf.logging);

	cmd.Parse(argc, argv);
	NS_ABORT_IF(conf.frequencyBwp1 < 6e9 || conf.frequencyBwp1 > 100e9);
	NS_ABORT_IF(conf.frequencyBwp2 < 6e9 || conf.frequencyBwp2 > 100e9);

	//ConfigStore inputConfig;
	//inputConfig.ConfigureDefaults ();

	// enable logging or not
	if (conf.logging) {
		LogComponentEnable("MmWave3gppPropagationLossModel", LOG_LEVEL_ALL);
		LogComponentEnable("MmWave3gppBuildingsPropagationLossModel",
				LOG_LEVEL_ALL);
		LogComponentEnable("MmWave3gppChannel", LOG_LEVEL_ALL);
		LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
		LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
		LogComponentEnable("LtePdcp", LOG_LEVEL_INFO);
	}

	Config::SetDefault("ns3::MmWave3gppPropagationLossModel::ChannelCondition",
			StringValue("l"));
	Config::SetDefault("ns3::MmWave3gppPropagationLossModel::Scenario",
			StringValue("UMi-StreetCanyon")); // with antenna height of 10 m
	Config::SetDefault("ns3::MmWave3gppPropagationLossModel::Shadowing",
			BooleanValue(false));

	Config::SetDefault("ns3::MmWave3gppChannel::CellScan",
			BooleanValue(conf.cellScan));
	Config::SetDefault("ns3::MmWave3gppChannel::BeamSearchAngleStep",
			DoubleValue(conf.beamSearchAngleStep));

	Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize",
			UintegerValue(999999999));

	Config::SetDefault("ns3::PointToPointEpcHelper::S1uLinkDelay",
			TimeValue(MilliSeconds(0)));

	//Config::SetDefault("ns3::MmWaveUeNetDevice::AntennaNum", UintegerValue (4));
	//Config::SetDefault("ns3::MmWaveEnbNetDevice::AntennaNum", UintegerValue (16));
	//Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue (txPower));

	if (conf.singleBwp) {
		Config::SetDefault("ns3::MmWaveHelper::NumberOfComponentCarriers",
				UintegerValue(1));
	} else {
		Config::SetDefault("ns3::MmWaveHelper::NumberOfComponentCarriers",
				UintegerValue(2));
	}

	Config::SetDefault("ns3::BwpManagerAlgorithmStatic::NGBR_LOW_LAT_EMBB",
			UintegerValue(0));
	Config::SetDefault("ns3::BwpManagerAlgorithmStatic::GBR_CONV_VOICE",
			UintegerValue(1));

	// create base stations and mobile terminals
	NodeContainer gNbNodes;
	NodeContainer ueNodes;
	MobilityHelper gNbMobility, ueMobility;

	double gNbHeight = 10;
	double ueHeight = 1.5;

	gNbNodes.Create(conf.gNbNum);
	ueNodes.Create(conf.ueNumPergNb * conf.gNbNum);

	Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
	Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator>();

//	Ptr<GridPositionAllocator> staPositionAlloc = CreateObject<GridPositionAllocator>();

	int32_t yValue = 0.0;

	int32_t xMin, xMax, yMin, yMax;

	xMin = -30;
	xMax = 30;
	yMin = yMax = 0;

	for (uint32_t i = 1; i <= gNbNodes.GetN(); ++i) {
		// 2.0, -2.0, 6.0, -6.0, 10.0, -10.0, ....
		if (i % 2 != 0) {
			yValue = static_cast<int>(i) * 30;
			yMax = yValue + 30; //bounding box
		} else {
			yValue = -yValue;
			yMin = yValue - 30; //bounding box
		}

		apPositionAlloc->Add(Vector(0.0, yValue, gNbHeight));

		// 1.0, -1.0, 3.0, -3.0, 5.0, -5.0, ...
		double xValue = 0.0;
		for (uint32_t j = 1; j <= conf.ueNumPergNb; ++j) {
			if (j % 2 != 0) {
				xValue = j;
			} else {
				xValue = -xValue;
			}
			std::cout << "i: " << i << ", y:" << yValue << ", j:" << j << ", x:" << xValue << std::endl;
			if (yValue > 0) {
				staPositionAlloc->Add(Vector(xValue, yValue, ueHeight));
			} else {
				staPositionAlloc->Add(Vector(xValue, yValue, ueHeight));
			}
		}
	}
//	return 0;

	gNbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	gNbMobility.SetPositionAllocator(apPositionAlloc);
	gNbMobility.Install(gNbNodes);

//	auto uvar = Create<UniformRandomVariable>();
//	uvar->SetAttribute("Min", DoubleValue(100));
//	uvar->SetAttribute("Max", DoubleValue(400));
	ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
            "Bounds", RectangleValue (Rectangle (xMin, xMax, yMin, yMax)),
			"Distance", DoubleValue(.5),
			"Speed", StringValue ("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"));
	ueMobility.SetPositionAllocator(staPositionAlloc);
	ueMobility.Install(ueNodes);

	// setup the mmWave simulation
	Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper>();
	mmWaveHelper->SetAttribute("PathlossModel",
			StringValue("ns3::MmWave3gppPropagationLossModel"));
	mmWaveHelper->SetAttribute("ChannelModel",
			StringValue("ns3::MmWave3gppChannel"));

	Ptr<MmWavePhyMacCommon> phyMacCommonBwp1 =
			CreateObject<MmWavePhyMacCommon>();
	phyMacCommonBwp1->SetCentreFrequency(conf.frequencyBwp1);
	phyMacCommonBwp1->SetBandwidth(conf.bandwidthBwp1);
	phyMacCommonBwp1->SetNumerology(conf.numerologyBwp1);
	phyMacCommonBwp1->SetAttribute("MacSchedulerType",
			TypeIdValue(MmWaveMacSchedulerTdmaRR::GetTypeId()));
	phyMacCommonBwp1->SetCcId(0);

	BandwidthPartRepresentation repr1(0, phyMacCommonBwp1, nullptr, nullptr,
			nullptr);
	mmWaveHelper->AddBandwidthPart(0, repr1);

	// if not single BWP simulation add second BWP configuration
	if (!conf.singleBwp) {
		Ptr<MmWavePhyMacCommon> phyMacCommonBwp2 = CreateObject<
				MmWavePhyMacCommon>();
		phyMacCommonBwp2->SetCentreFrequency(conf.frequencyBwp2);
		phyMacCommonBwp2->SetBandwidth(conf.bandwidthBwp2);
		phyMacCommonBwp2->SetNumerology(conf.numerologyBwp2);
		phyMacCommonBwp2->SetCcId(1);
		BandwidthPartRepresentation repr2(1, phyMacCommonBwp2, nullptr, nullptr,
				nullptr);
		mmWaveHelper->AddBandwidthPart(1, repr2);
	}

	Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<
			NrPointToPointEpcHelper>();
	mmWaveHelper->SetEpcHelper(epcHelper);
	mmWaveHelper->Initialize();

	// install mmWave net devices
	NetDeviceContainer enbNetDev = mmWaveHelper->InstallEnbDevice(gNbNodes);
	NetDeviceContainer ueNetDev = mmWaveHelper->InstallUeDevice(ueNodes);

	double x = pow(10, conf.totalTxPower / 10);

	double totalBandwidth = 0;

	if (conf.singleBwp) {
		totalBandwidth = conf.bandwidthBwp1;
	} else {
		totalBandwidth = conf.bandwidthBwp1 + conf.bandwidthBwp2;
	}

	for (uint32_t j = 0; j < enbNetDev.GetN(); ++j) {
		ObjectMapValue objectMapValue;
		Ptr<MmWaveEnbNetDevice> netDevice = DynamicCast<MmWaveEnbNetDevice>(
				enbNetDev.Get(j));
		netDevice->GetAttribute("ComponentCarrierMap", objectMapValue);
		for (uint32_t i = 0; i < objectMapValue.GetN(); i++) {
			Ptr<ComponentCarrierGnb> bandwidthPart = DynamicCast<
					ComponentCarrierGnb>(objectMapValue.Get(i));
			if (i == 0) {
				bandwidthPart->GetPhy()->SetTxPower(
						10 * log10((conf.bandwidthBwp1 / totalBandwidth) * x));
				std::cout << "\n txPower1 = "
						<< 10 * log10((conf.bandwidthBwp1 / totalBandwidth) * x)
						<< std::endl;
			} else if (i == 1) {
				bandwidthPart->GetPhy()->SetTxPower(
						10 * log10((conf.bandwidthBwp2 / totalBandwidth) * x));
				std::cout << "\n txPower2 = "
						<< 10 * log10((conf.bandwidthBwp2 / totalBandwidth) * x)
						<< std::endl;
			} else {
				std::cout
						<< "\n Please extend power assignment for additional bandwidht parts...";
			}
		}
	}

	// create the internet and install the IP stack on the UEs
	// get SGW/PGW and create a single RemoteHost
	Ptr<Node> pgw = epcHelper->GetPgwNode();
	NodeContainer remoteHostContainer;
	remoteHostContainer.Create(1);
	Ptr<Node> remoteHost = remoteHostContainer.Get(0);
	InternetStackHelper internet;
	internet.Install(remoteHostContainer);

	// connect a remoteHost to pgw. Setup routing too
	PointToPointHelper p2ph;
	p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
	p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
	p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));
	NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
	Ipv4AddressHelper ipv4h;
	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	ipv4h.SetBase("1.0.0.0", "255.0.0.0");
	Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
	Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
			ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
	remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
			Ipv4Mask("255.0.0.0"), 1);
	internet.Install(ueNodes);
	Ipv4InterfaceContainer ueIpIface;
	ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));

	// Set the default gateway for the UEs
	for (uint32_t j = 0; j < ueNodes.GetN(); ++j) {
		Ptr<Ipv4StaticRouting> ueStaticRouting =
				ipv4RoutingHelper.GetStaticRouting(
						ueNodes.Get(j)->GetObject<Ipv4>());
		ueStaticRouting->SetDefaultRoute(
				epcHelper->GetUeDefaultGatewayAddress(), 1);
	}

	// assign IP address to UEs, and install UDP downlink applications
	uint16_t dlPort = 1234;
	ApplicationContainer clientApps, serverApps;

	ApplicationContainer clientAppsEmbb, serverAppsEmbb;

	DashServerHelper dashSrHelper(dlPort);
	serverApps.Add(dashSrHelper.Install(remoteHost));

	int counter = 0;
	DashClientHelper dlClient(internetIpIfaces.GetAddress(1), dlPort); //Remotehost is the second node, pgw is first
	dlClient.SetAttribute("VideoFilePath", StringValue("src/spdash/examples/vid.txt"));
	dlClient.SetAttribute("OnStartCB", CallbackValue(MakeBoundCallback(onStart, &counter)));
	dlClient.SetAttribute("OnStopCB", CallbackValue(MakeBoundCallback(onStop, &counter)));
	dlClient.SetAttribute("TracePath", StringValue("SomeData"));
	// configure here UDP traffic
	for (uint32_t j = 0; j < ueNodes.GetN(); ++j) {

		clientApps.Add(dlClient.Install(ueNodes.Get(j)));

		Ptr<EpcTft> tft = Create<EpcTft>();
		EpcTft::PacketFilter dlpf;
		dlpf.localPortStart = dlPort;
		dlpf.localPortEnd = dlPort;
		dlPort++;
		tft->Add(dlpf);

		enum EpsBearer::Qci q;

		if (j % 2 == 0) {
			q = EpsBearer::NGBR_LOW_LAT_EMBB;
		} else {
			q = EpsBearer::GBR_CONV_VOICE;
		}

		EpsBearer bearer(q);
		mmWaveHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(j), bearer, tft);
	}
	// start UDP server and client apps
	serverApps.Start(Seconds(udpAppStartTime));
	clientApps.Start(Seconds(udpAppStartTime+1));

	// attach UEs to the closest eNB
	mmWaveHelper->AttachToClosestEnb(ueNetDev, enbNetDev);

	// enable the traces provided by the mmWave module
//  mmWaveHelper->EnableTraces();

	FlowMonitorHelper flowmonHelper;
	NodeContainer endpointNodes;
	endpointNodes.Add(remoteHost);
	endpointNodes.Add(ueNodes);

	Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
	monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
	monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
	monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));



	p2ph.EnablePcapAll ("nr-p2p");

//	AnimationInterface anim(conf.animFile);
//	anim.EnablePacketMetadata(); // Optional
//	anim.EnableIpv4L3ProtocolCounters(Seconds(0), Seconds(10)); // Optional


//	Simulator::Stop(Seconds(simTime));
	Simulator::Run();

	storeFlowMonitor(monitor, flowmonHelper, conf);


	Simulator::Destroy();
	return 0;
}

