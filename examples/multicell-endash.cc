/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/* *
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
 *
 * Author: Michele Polese <michele.polese@gmail.com>
 */

#include <sys/stat.h>
#include <sys/types.h>
#include "ns3/mmwave-helper.h"
#include "ns3/trace-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-radio-energy-model-helper.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/flow-monitor-module.h"
//#include "ns3/gtk-config-store.h"
#include <ns3/buildings-helper.h>
#include <ns3/buildings-module.h>
#include <ns3/random-variable-stream.h>
#include <ns3/lte-ue-net-device.h>
#include "ns3/log.h"
#include "ns3/internet-apps-module.h"
#include "ns3/endash-helper.h"
#include <iostream>
#include <ctime>
#include <stdlib.h>
#include <list>
#include <random>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>


using namespace ns3;
using namespace mmwave;

/**
 * Sample simulation script for MC device. It instantiates a LTE and two MmWave eNodeB,
 * attaches one MC UE to both and starts a flow for the UE to and from a remote host.
 */

NS_LOG_COMPONENT_DEFINE ("endash");

std::string outputDir = "";
std::string handoverfilename = "/handover_endash_pensieve.csv";
std::string tracefilename1 = "/endashtracefileuelargepensieve.csv";
std::string tracefilename2 =  "/endashtracefileenblargepensieve.csv";
std::string simfilename =  "/simulation_time_endash_pensieve.csv"; 
std::string energyFileName =  "/energyfile.csv";
std::string stateChangefile = "/stateChange.csv";

void
onStart (int *count)
{
  std::cout << "Starting at:" << Simulator::Now () << std::endl;
  (*count)++;
}
void
onStop (int *count)
{
  std::cout << "Stoping at:" << Simulator::Now () << std::endl;
  (*count)--;
  if (!(*count))
    {
      auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      std::cout << "Simulation stopped at: " << ctime(&timenow) << std::endl;
      std::fstream simtimefile;
      simtimefile.open(outputDir + simfilename,std::ios::out | std::ios::app);
      simtimefile << "Stop," << Simulator::Now().GetSeconds() << "," << ctime(&timenow) << std::endl;

      Simulator::Stop ();
    }
}

void
PrintPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> model = node->GetObject<MobilityModel> ();
  NS_LOG_UNCOND ("Position +****************************** " << model->GetPosition () << " at time " << Simulator::Now ().GetSeconds ());
}

bool
AreOverlapping (Box a, Box b)
{
  return !((a.xMin > b.xMax) || (b.xMin > a.xMax) || (a.yMin > b.yMax) || (b.yMin > a.yMax) );
}


bool
OverlapWithAnyPrevious (Box box, std::list<Box> m_previousBlocks)
{
  for (std::list<Box>::iterator it = m_previousBlocks.begin (); it != m_previousBlocks.end (); ++it)
    {
      if (AreOverlapping (*it,box))
        {
          return true;
        }
    }
  return false;
}


void deployEnb(int deploypoints[][2])
{

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed1);
    std::uniform_int_distribution<int> distribution(1,1000);

    int x = distribution(generator);
    int y = distribution(generator);
    std::cout << "1 Point X: " << x <<"\tY: " << y <<std::endl;
    deploypoints[0][0] = x;
    deploypoints[0][1] = y;
    int numofgNb = 1;
    int intergNbgap = 100;
    int numofAttempts = 0;
    while(numofgNb < 10){
        numofAttempts++;
        int thisx = distribution(generator);
        int thisy = distribution(generator);
        bool overlapping = false;
        for(int j=0;j<numofgNb;j++)
        {
            double distance = sqrt((pow((thisx-deploypoints[j][0]),2)+pow((thisy-deploypoints[j][1]),2)));

            if((distance<2*intergNbgap) || (thisx<150) || (thisy < 150) || (thisx > 850) || (thisy > 850)){
                //overlapping gNbs
                overlapping = true;
                break;
            }
        }
        if(!overlapping){
            deploypoints[numofgNb][0] = thisx;
            deploypoints[numofgNb][1] = thisy;
            numofgNb++;
            std::cout << numofgNb << " Point X: " << deploypoints[numofgNb-1][0] <<"\tY: " << deploypoints[numofgNb-1][1] <<std::endl;
        }
        if(numofAttempts>10000){
            break;
        }
    }
    std::cout << "Number of Attempts Made\t" << numofAttempts << std::endl;
}


std::pair<Box, std::list<Box> >
GenerateBuildingBounds (double xCoormin, double yCoormin, double xArea, double yArea, double maxBuildSize, std::list<Box> m_previousBlocks )
{

  Ptr<UniformRandomVariable> xMinBuilding = CreateObject<UniformRandomVariable> ();
  xMinBuilding->SetAttribute ("Min",DoubleValue (xCoormin));
  xMinBuilding->SetAttribute ("Max",DoubleValue (xArea));

  NS_LOG_UNCOND ("min " << xCoormin << " max " << xArea);

  Ptr<UniformRandomVariable> yMinBuilding = CreateObject<UniformRandomVariable> ();
  yMinBuilding->SetAttribute ("Min",DoubleValue (yCoormin));
  yMinBuilding->SetAttribute ("Max",DoubleValue (yArea));

  NS_LOG_UNCOND ("min " << yCoormin << " max " << yArea);

  Box box;
  uint32_t attempt = 0;
  do
    {
      NS_ASSERT_MSG (attempt < 100, "Too many failed attempts to position non-overlapping buildings. Maybe area too small or too many buildings?");
      box.xMin = xMinBuilding->GetValue ();

      Ptr<UniformRandomVariable> xMaxBuilding = CreateObject<UniformRandomVariable> ();
      xMaxBuilding->SetAttribute ("Min",DoubleValue (box.xMin));
      xMaxBuilding->SetAttribute ("Max",DoubleValue (box.xMin + maxBuildSize));
      box.xMax = xMaxBuilding->GetValue ();

      box.yMin = yMinBuilding->GetValue ();

      Ptr<UniformRandomVariable> yMaxBuilding = CreateObject<UniformRandomVariable> ();
      yMaxBuilding->SetAttribute ("Min",DoubleValue (box.yMin));
      yMaxBuilding->SetAttribute ("Max",DoubleValue (box.yMin + maxBuildSize));
      box.yMax = yMaxBuilding->GetValue ();

      ++attempt;
    }
  while (OverlapWithAnyPrevious (box, m_previousBlocks));


  NS_LOG_UNCOND ("Building in coordinates (" << box.xMin << " , " << box.yMin << ") and ("  << box.xMax << " , " << box.yMax <<
                 ") accepted after " << attempt << " attempts");
  m_previousBlocks.push_back (box);
  std::pair<Box, std::list<Box> > pairReturn = std::make_pair (box,m_previousBlocks);
  return pairReturn;

}

static ns3::GlobalValue g_interPckInterval ("interPckInterval", "Interarrival time of UDP packets (us)",
                                            ns3::UintegerValue (20), ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_bufferSize ("bufferSize", "RLC tx buffer size (MB)",
                                      ns3::UintegerValue (20), ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_x2Latency ("x2Latency", "Latency on X2 interface (us)",
                                     ns3::DoubleValue (500), ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_mmeLatency ("mmeLatency", "Latency on MME interface (us)",
                                      ns3::DoubleValue (10000), ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_rlcAmEnabled ("rlcAmEnabled", "If true, use RLC AM, else use RLC UM",
                                        ns3::BooleanValue (true), ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_runNumber ("runNumber", "Run number for rng",
                                     ns3::UintegerValue (10), ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_noiseAndFilter ("noiseAndFilter", "If true, use noisy SINR samples, filtered. If false, just use the SINR measure",
                                          ns3::BooleanValue (false), ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_handoverMode ("handoverMode",
                                        "Handover mode",
                                        ns3::UintegerValue (3), ns3::MakeUintegerChecker<uint8_t> ());
static ns3::GlobalValue g_reportTablePeriodicity ("reportTablePeriodicity", "Periodicity of RTs",
                                                  ns3::UintegerValue (1600), ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_outageThreshold ("outageTh", "Outage threshold",
                                           ns3::DoubleValue (-5), ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_lteUplink ("lteUplink", "If true, always use LTE for uplink signalling",
                                     ns3::BooleanValue (false), ns3::MakeBooleanChecker ());

void
NotifyConnectionEstablishedUe (std::string context,
                               uint64_t imsi,
                               uint16_t cellid,
                               uint16_t rnti)
{
  if (imsi == 9)
  {
    std::fstream fileout;
    fileout.open( outputDir + handoverfilename,std::ios::out | std::ios::app);
    fileout << Simulator::Now().GetSeconds() <<",";
    fileout << "ConnectionEstablishedUe" <<",";
    fileout << imsi << ",";
    fileout << cellid <<",";
    fileout << rnti <<",";
    fileout << "NA" << std::endl;
    std::cout << Simulator::Now ().GetSeconds () << " " << context
              << " UE IMSI " << imsi
              << ": connected to CellId " << cellid
              << " with RNTI " << rnti
              << std::endl;
  }
}

void
NotifyHandoverStartUe (std::string context,
                       uint64_t imsi,
                       uint16_t cellid,
                       uint16_t rnti,
                       uint16_t targetCellId)
{
  if (imsi == 9)
  {
    std::fstream fileout;
    fileout.open(outputDir + handoverfilename,std::ios::out | std::ios::app);
    fileout << Simulator::Now().GetSeconds() <<",";
    fileout << "HandoverStartUe" <<",";
    fileout << imsi << ",";
    fileout << cellid <<",";
    fileout << rnti <<",";
    fileout << targetCellId << std::endl;

    std::cout << Simulator::Now ().GetSeconds () << " " << context
              << " UE IMSI " << imsi
              << ": previously connected to CellId " << cellid
              << " with RNTI " << rnti
              << ", doing handover to CellId " << targetCellId
              << std::endl;

  }
  
}

void
NotifyHandoverEndOkUe (std::string context,
                       uint64_t imsi,
                       uint16_t cellid,
                       uint16_t rnti)
{
  if (imsi == 9)
  {
    std::fstream fileout;
    fileout.open(outputDir + handoverfilename,std::ios::out | std::ios::app);
    fileout << Simulator::Now().GetSeconds() <<",";
    fileout << "HandoverEndOkUe" <<",";
    fileout << imsi << ",";
    fileout << cellid <<",";
    fileout << rnti <<",";
    fileout << "NA" << std::endl;

    std::cout << Simulator::Now ().GetSeconds () << " " << context
              << " UE IMSI " << imsi
              << ": successful handover to CellId " << cellid
              << " with RNTI " << rnti
              << std::endl;
  }

}

void
NotifyConnectionEstablishedEnb (std::string context,
                                uint64_t imsi,
                                uint16_t cellid,
                                uint16_t rnti)
{
  if (imsi==9)
  {
    std::fstream fileout;
    fileout.open(outputDir + handoverfilename,std::ios::out | std::ios::app);
    fileout << Simulator::Now().GetSeconds() <<",";
    fileout << "ConnectionEstablishedEnb" <<",";
    fileout << imsi << ",";
    fileout << cellid <<",";
    fileout << rnti <<",";
    fileout << "NA" << std::endl;

    std::cout << Simulator::Now ().GetSeconds () << " " << context
              << " eNB CellId " << cellid
              << ": successful connection of UE with IMSI " << imsi
              << " RNTI " << rnti
              << std::endl;

  }

}

void
NotifyHandoverStartEnb (std::string context,
                        uint64_t imsi,
                        uint16_t cellid,
                        uint16_t rnti,
                        uint16_t targetCellId)
{
  if (imsi == 9)
  {
    std::fstream fileout;
    fileout.open(outputDir + handoverfilename,std::ios::out | std::ios::app);
    fileout << Simulator::Now().GetSeconds() <<",";
    fileout << "HandoverStartEnb" <<",";
    fileout << imsi << ",";
    fileout << cellid <<",";
    fileout << rnti <<",";
    fileout << targetCellId << std::endl;

    std::cout << Simulator::Now ().GetSeconds () << " " << context
              << " eNB CellId " << cellid
              << ": start handover of UE with IMSI " << imsi
              << " RNTI " << rnti
              << " to CellId " << targetCellId
              << std::endl;
  }
}

void
NotifyHandoverEndOkEnb (std::string context,
                        uint64_t imsi,
                        uint16_t cellid,
                        uint16_t rnti)
{
  if (imsi == 9)
  {
    std::fstream fileout;
    fileout.open(outputDir + handoverfilename,std::ios::out | std::ios::app);
    fileout << Simulator::Now().GetSeconds() <<",";
    fileout << "HandoverEndOkEnb" <<",";
    fileout << imsi << ",";
    fileout << cellid <<",";
    fileout << rnti <<",";
    fileout << "NA" << std::endl;

    std::cout << Simulator::Now ().GetSeconds () << " " << context
              << " eNB CellId " << cellid
              << ": completed handover of UE with IMSI " << imsi
              << " RNTI " << rnti
              << std::endl;
  }

}


void
traceuefunc (std::string path, RxPacketTraceParams params)
{
  std::fstream tracefile;
  tracefile.open (outputDir + tracefilename1, std::ios::out | std::ios::app);

  std::cout << "DL\t" << Simulator::Now ().GetSeconds () << "\t" 
                      << params.m_frameNum << "\t" << +params.m_sfNum << "\t" 
                      << +params.m_slotNum << "\t" << +params.m_symStart << "\t" 
                      << +params.m_numSym << "\t" << params.m_cellId << "\t" 
                      << params.m_rnti << "\t" << +params.m_ccId << "\t" 
                      << params.m_tbSize << "\t" << +params.m_mcs << "\t" 
                      << +params.m_rv << "\t" << 10 * std::log10 (params.m_sinr) << "\t" 
                      << params.m_corrupt << "\t" <<  params.m_tbler << "\t"
                      << 10 * std::log10(params.m_sinrMin) << std::endl;
  tracefile << "DL," << Simulator::Now ().GetSeconds () << "," 
                      << params.m_frameNum << "," << +params.m_sfNum << "," 
                      << +params.m_slotNum << "," << +params.m_symStart << "," 
                      << +params.m_numSym << "," << params.m_cellId << "," 
                      << params.m_rnti << "," << +params.m_ccId << "," 
                      << params.m_tbSize << "," << +params.m_mcs << "," 
                      << +params.m_rv << "," << 10 * std::log10 (params.m_sinr) << "," 
                      << params.m_corrupt << "," <<  params.m_tbler << ","
                      << 10 * std::log10(params.m_sinrMin) << std::endl;
}


void
traceenbfunc (std::string path, RxPacketTraceParams params)
{
  std::fstream tracefile;
  
  tracefile.open (outputDir + tracefilename2, std::ios::out | std::ios::app);

  std::cout << "UL\t" << Simulator::Now ().GetSeconds () << "\t" 
                      << params.m_frameNum << "\t" << +params.m_sfNum << "\t" 
                      << +params.m_slotNum << "\t" << +params.m_symStart << "\t" 
                      << +params.m_numSym << "\t" << params.m_cellId << "\t" 
                      << params.m_rnti << "\t" << +params.m_ccId << "\t" 
                      << params.m_tbSize << "\t" << +params.m_mcs << "\t" 
                      << +params.m_rv << "\t" << 10 * std::log10 (params.m_sinr) << "\t" 
                      << params.m_corrupt << "\t" <<  params.m_tbler << "\t"
                      << 10 * std::log10(params.m_sinrMin) << std::endl;
  tracefile << "UL," << Simulator::Now ().GetSeconds () << "," 
                      << params.m_frameNum << "," << +params.m_sfNum << "," 
                      << +params.m_slotNum << "," << +params.m_symStart << "," 
                      << +params.m_numSym << "," << params.m_cellId << "," 
                      << params.m_rnti << "," << +params.m_ccId << "," 
                      << params.m_tbSize << "," << +params.m_mcs << "," 
                      << +params.m_rv << "," << 10 * std::log10 (params.m_sinr) << "," 
                      << params.m_corrupt << "," <<  params.m_tbler << ","
                      << 10 * std::log10(params.m_sinrMin) << std::endl;
}

void
storeFlowMonitor (Ptr<ns3::FlowMonitor> monitor,
                  FlowMonitorHelper &flowmonHelper)
{
  // Print per-flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (
      flowmonHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double averageFlowThroughput = 0.0;
  double averageFlowDelay = 0.0;

  std::ofstream outFile;
  std::string filename = outputDir+ "/default";
  outFile.open (filename.c_str (), std::ofstream::out | std::ofstream::trunc);
  if (!outFile.is_open ())
    {
      std::cerr << "Can't open file " << filename << std::endl;
      return;
    }

  outFile.setf (std::ios_base::fixed);

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
      stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::stringstream protoStream;
      protoStream << (uint16_t) t.protocol;
      if (t.protocol == 6)
        {
          protoStream.str ("TCP");
        }
      if (t.protocol == 17)
        {
          protoStream.str ("UDP");
        }
      double txDuration = (i->second.timeLastTxPacket.GetSeconds ()
          - i->second.timeFirstTxPacket.GetSeconds ());
      outFile << "Flow " << i->first << " (" << t.sourceAddress << ":"
          << t.sourcePort << " -> " << t.destinationAddress << ":"
          << t.destinationPort << ") proto " << protoStream.str () << "\n";
      outFile << "  Tx Packets: " << i->second.txPackets << "\n";
      outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
      outFile << "  TxOffered:  "
          << i->second.txBytes * 8.0 / txDuration / 1000 / 1000 << " Mbps\n";
      outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      if (i->second.rxPackets > 0)
        {
          // Measure the duration of the flow from receiver's perspective
          //double rxDuration = i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
          double rxDuration = (i->second.timeLastRxPacket.GetSeconds ()
              - i->second.timeFirstRxPacket.GetSeconds ());

          averageFlowThroughput += i->second.rxBytes * 8.0 / rxDuration / 1000
              / 1000;
          averageFlowDelay += 1000 * i->second.delaySum.GetSeconds ()
              / i->second.rxPackets;

          outFile << "  Throughput: "
              << i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000
              << " Mbps\n";
          outFile << "  Mean delay:  "
              << 1000 * i->second.delaySum.GetSeconds () / i->second.rxPackets
              << " ms\n";
          //outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << " Mbps \n";
          outFile << "  Mean jitter:  "
              << 1000 * i->second.jitterSum.GetSeconds () / i->second.rxPackets
              << " ms\n";
        }
      else
        {
          outFile << "  Throughput:  0 Mbps\n";
          outFile << "  Mean delay:  0 ms\n";
          outFile << "  Mean jitter: 0 ms\n";
        }
      outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
    }

  outFile << "\n\n  Mean flow throughput: "
      << averageFlowThroughput / stats.size () << "\n";
  outFile << "  Mean flow delay: " << averageFlowDelay / stats.size () << "\n";

  outFile.close ();
}

std::string
readNodeTrace (NodeContainer *gnbNodes, Ptr<Node> node, bool firstLine = false)
{

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
    return "nodeId,velo_x,velo_y,pos_x,pos_y,IsLinkUp,Csgid,Earfcn,Imsi,rrcState,rrcState_str,rrcCellId,rrcDlBw,distfromcell";
}
std::stringstream stream;
stream << std::to_string (node->GetId ());
auto mModel = node->GetObject<MobilityModel> ();
stream << "," << mModel->GetVelocity ().x << "," << mModel->GetVelocity ().y;
stream << "," << mModel->GetPosition ().x << "," << mModel->GetPosition ().y;

Ptr<McUeNetDevice> netDevice; // = node->GetObject<MmWaveUeNetDevice>();
//Ptr<ns3::McUeNetDevice> netDevice;

for (uint32_t i = 0; i < node->GetNDevices (); i++)
{
    std::cout << "\tNode: " << node->GetId () << ", Device " << i << ": "
    << node->GetDevice (i)->GetInstanceTypeId () << std::endl;
    auto nd = node->GetDevice (i);
    //if (nd->GetInstanceTypeId () == MmWaveUeNetDevice::GetTypeId ())
    std::string nameofdevice = nd->GetInstanceTypeId ().GetName();
    if (nameofdevice == "ns3::McUeNetDevice")
    {
    netDevice = DynamicCast<McUeNetDevice> (nd);
    break;
    }

}
// std::cout << "Channel Id" << netDevice->GetChannel()->GetId() << std::endl;
// std::cout << "Channel Id" << netDevice->GetChannel()->GetNDevices() << std::endl;

std::cout << "Mmwave Frequency";
std::cout << "CsgId: "<<netDevice->GetCsgId () <<std::endl;
std::cout << "GetMmWaveEarfcn: "<<netDevice->GetMmWaveEarfcn () <<std::endl;
std::cout << "GetLteDlEarfcn: "<<netDevice->GetLteDlEarfcn () <<std::endl;
std::cout << "GetImsi: "<<netDevice->GetImsi () <<std::endl;

// stream << "," << std::to_string (netDevice->GetChannel ()->GetId());
// stream << "," << std::to_string (netDevice->GetChannel ()->GetNDevices());
Ptr<MmWaveUePhy> phy = netDevice->GetMmWavePhy ();

stream << "," << std::to_string(netDevice->IsLinkUp());


stream << "," << std::to_string (netDevice->GetCsgId ());
stream << "," << std::to_string (netDevice->GetMmWaveEarfcn ());
stream << "," << std::to_string (netDevice->GetImsi ());
Ptr<LteUeRrc> rrc = netDevice->GetMmWaveRrc ();

std::cout << "GetState: "<<rrc->GetState () <<std::endl;
std::cout << "rrcstate: "<<rrcStates[rrc->GetState ()] <<std::endl;
std::cout << "GetCellId: "<<rrc->GetCellId () <<std::endl;
std::cout << "GetDlBandwidth: "<<rrc->GetDlBandwidth () <<std::endl;

stream << "," << std::to_string (rrc->GetState ());
stream << "," << rrcStates[rrc->GetState ()];
stream << "," << std::to_string (rrc->GetCellId ());
stream << "," << std::to_string (rrc->GetDlBandwidth ());
uint16_t cell_id = rrc->GetCellId();

for(uint32_t i = 0; i < gnbNodes->GetN(); i++)
{
    Ptr<MmWaveEnbNetDevice> mmdev = DynamicCast<MmWaveEnbNetDevice> (gnbNodes->Get(i)->GetDevice(0));
    if (mmdev->GetCellId () == cell_id){
        auto ap = gnbNodes->Get(i);
        auto apModel = ap->GetObject<MobilityModel> ();
        //      auto appos = apModel->GetPosition();
        stream << "," << mModel->GetDistanceFrom(apModel);
    }
    
}

return stream.str ();
  

}


void
IntStateChange (int32_t old_state, int32_t new_state)
{
  std::string old_state_val;
  std::string new_state_val;
  switch (old_state)
  {
  case 0:
    old_state_val = "IDLE";
    break;
  case 1:
    old_state_val = "TX";
    break;
  case 2:
    old_state_val = "RX_DATA";
    break;
  case 3:
    old_state_val = "RX_CTRL";
    break;
  }
  switch (new_state)
  {
  case 0:
    new_state_val = "IDLE";
    break;
  case 1:
    new_state_val = "TX";
    break;
  case 2:
    new_state_val = "RX_DATA";
    break;
  case 3:
    new_state_val = "RX_CTRL";
    break;
  }
std::ofstream outFile;
  outFile.open (outputDir + stateChangefile, std::ios::out | std::ios::app);
  outFile << Simulator::Now().GetNanoSeconds () << "," << old_state_val << "," << new_state_val << std::endl;
}

void
EnbStateChange (int32_t old_state, int32_t new_state, int32_t i)
{
  std::string old_state_val;
  std::string new_state_val;
  switch (old_state)
  {
  case 0:
    old_state_val = "IDLE";
    break;
  case 1:
    old_state_val = "TX";
    break;
  case 2:
    old_state_val = "RX_DATA";
    break;
  case 3:
    old_state_val = "RX_CTRL";
    break;
  }
  switch (new_state)
  {
  case 0:
    new_state_val = "IDLE";
    break;
  case 1:
    new_state_val = "TX";
    break;
  case 2:
    new_state_val = "RX_DATA";
    break;
  case 3:
    new_state_val = "RX_CTRL";
    break;
  }
  std::ofstream outFile;
  std::string outname = outputDir + "/Enb" + std::to_string(i) + "stateChange.csv";
  outFile.open (outname, std::ios::out | std::ios::app);
  outFile << Simulator::Now().GetNanoSeconds () << "," << old_state_val << "," << new_state_val << std::endl;
}

void MakeBaseStationSleep (Ptr<MmWaveSpectrumPhy> endlphy, Ptr<MmWaveSpectrumPhy> enulphy, bool val)
{
  endlphy->SetAttribute ("MakeItSleep", BooleanValue(val));
  enulphy->SetAttribute ("MakeItSleep", BooleanValue (val));
}

void
EnergyConsumptionUpdate (double totaloldEnergyConsumption, double totalnewEnergyConsumption)
{
  //std::cout << "Total Energy Consumption " << totalnewEnergyConsumption << "J" << std::endl;
  Time currentTime = Simulator::Now ();
std::ofstream outFile;
  outFile.open (outputDir + energyFileName, std::ios::out | std::ios::app);
  outFile << currentTime.GetNanoSeconds () << "," << totalnewEnergyConsumption << "," << (totalnewEnergyConsumption-totaloldEnergyConsumption) << std::endl;
}


bool
isDir (std::string path)
{
  struct stat statbuf;
  if (stat (path.c_str (), &statbuf) != 0)
    return false;
  return S_ISDIR(statbuf.st_mode);
}


int
main (int argc, char *argv[])
{
  bool harqEnabled = true;
  bool fixedTti = false;
  // unsigned symPerSf = 24;
  // double sfPeriod = 100.0;

  std::list<Box>  m_previousBlocks;
  double nodeTraceInterval = 1;
  uint16_t gNbNum = 5;
  double udpAppStartTime = 0.4; //seconds

  int mobilityType = 0;     //default without mobility
  int sleepEnabled = 0;     //default not enabled
  double minSpeedVal = 0;   //default zero
  double maxSpeedVal = 0;   //default zero
  double sleepStartTime = 10;
  double sleepStopTime = 20;
  int AbrPortVal = 8333;    //default 8333
  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("outputDir", "Output Directory for trace storing", outputDir);
  cmd.AddValue("MobilityType", "1 if With Mobility & 0 if Without Mobility", mobilityType);
  cmd.AddValue("MinSpeed", "Minimum Speed of the UE", minSpeedVal);
  cmd.AddValue("MaxSpeed", "Maximum Speed of the UE", maxSpeedVal);
  cmd.AddValue("AbrPort", "Port to connect ABR proxy Server", AbrPortVal);
  cmd.AddValue("gNbNum", "Number of gNode-Bs", gNbNum);
  cmd.AddValue("sleepStart", "Base station Sleep Starting Time", sleepStartTime);
  cmd.AddValue("sleepStop", "Base station Sleep Stopping Time", sleepStopTime);
  cmd.AddValue("sleepEnabled", "If 1 then enabled, else not enabled", sleepEnabled);
  cmd.Parse (argc, argv);
  

  if (!isDir (outputDir))
    {
      mkdir (outputDir.c_str (), S_IRWXU);
    }


  UintegerValue uintegerValue;
  BooleanValue booleanValue;
  StringValue stringValue;
  DoubleValue doubleValue;

  // Variables for the RT
  int windowForTransient = 150; // number of samples for the vector to use in the filter
  GlobalValue::GetValueByName ("reportTablePeriodicity", uintegerValue);
  int ReportTablePeriodicity = (int)uintegerValue.Get (); // in microseconds
  if (ReportTablePeriodicity == 1600)
    {
      windowForTransient = 150;
    }
  else if (ReportTablePeriodicity == 25600)
    {
      windowForTransient = 50;
    }
  else if (ReportTablePeriodicity == 12800)
    {
      windowForTransient = 100;
    }
  else
    {
      NS_ASSERT_MSG (false, "Unrecognized");
    }

  int vectorTransient = windowForTransient * ReportTablePeriodicity;

  // params for RT, filter, HO mode
  GlobalValue::GetValueByName ("noiseAndFilter", booleanValue);
  bool noiseAndFilter = booleanValue.Get ();
  GlobalValue::GetValueByName ("handoverMode", uintegerValue);
  uint8_t hoMode = uintegerValue.Get ();
  GlobalValue::GetValueByName ("outageTh", doubleValue);
  double outageTh = doubleValue.Get ();

  GlobalValue::GetValueByName ("rlcAmEnabled", booleanValue);
  bool rlcAmEnabled = booleanValue.Get ();
  GlobalValue::GetValueByName ("bufferSize", uintegerValue);
  uint32_t bufferSize = uintegerValue.Get ();
  GlobalValue::GetValueByName ("interPckInterval", uintegerValue);
  uint32_t interPacketInterval = uintegerValue.Get ();
  GlobalValue::GetValueByName ("x2Latency", doubleValue);
  double x2Latency = doubleValue.Get ();
  GlobalValue::GetValueByName ("mmeLatency", doubleValue);
  double mmeLatency = doubleValue.Get ();

  double simTime = 21.1;
  NS_LOG_UNCOND ("rlcAmEnabled " << rlcAmEnabled << " bufferSize " << bufferSize << " interPacketInterval " <<
                 interPacketInterval << " x2Latency " << x2Latency << " mmeLatency " << mmeLatency);

  // rng things
  GlobalValue::GetValueByName ("runNumber", uintegerValue);
  uint32_t runSet = uintegerValue.Get ();
  uint32_t seedSet = 5;
  RngSeedManager::SetSeed (seedSet);
  RngSeedManager::SetRun (runSet);
  char seedSetStr[21];
  char runSetStr[21];
  sprintf (seedSetStr, "%d", seedSet);
  sprintf (runSetStr, "%d", runSet);

  Config::SetDefault ("ns3::MmWaveUeMac::UpdateUeSinrEstimatePeriod", DoubleValue (0));

  //get current time
  time_t rawtime;
  struct tm * timeinfo;
  char buffer[80];
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buffer,80,"%d_%m_%Y_%I_%M_%S",timeinfo);
  std::string time_str (buffer);

  Config::SetDefault ("ns3::MmWaveHelper::RlcAmEnabled", BooleanValue (rlcAmEnabled));
  Config::SetDefault ("ns3::MmWaveHelper::HarqEnabled", BooleanValue (harqEnabled));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue (harqEnabled));
  Config::SetDefault ("ns3::MmWaveFlexTtiMaxWeightMacScheduler::HarqEnabled", BooleanValue (harqEnabled));
  Config::SetDefault ("ns3::MmWaveFlexTtiMaxWeightMacScheduler::FixedTti", BooleanValue (fixedTti));
  Config::SetDefault ("ns3::MmWaveFlexTtiMaxWeightMacScheduler::SymPerSlot", UintegerValue (6));
  // Config::SetDefault ("ns3::MmWavePhyMacCommon::ResourceBlockNum", UintegerValue (1));
  // Config::SetDefault ("ns3::MmWavePhyMacCommon::ChunkPerRB", UintegerValue (72));
  // Config::SetDefault ("ns3::MmWavePhyMacCommon::SymbolsPerSubframe", UintegerValue (symPerSf));
  // Config::SetDefault ("ns3::MmWavePhyMacCommon::SubframePeriod", DoubleValue (sfPeriod));
  Config::SetDefault ("ns3::MmWavePhyMacCommon::TbDecodeLatency", UintegerValue (200.0));
  Config::SetDefault ("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue (100));
  // Config::SetDefault ("ns3::MmWaveBeamforming::LongTermUpdatePeriod", TimeValue (MilliSeconds (100.0)));
  Config::SetDefault ("ns3::LteEnbRrc::SystemInformationPeriodicity", TimeValue (MilliSeconds (5.0)));
  Config::SetDefault ("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue (MicroSeconds (100.0)));
  Config::SetDefault ("ns3::LteRlcUmLowLat::ReportBufferStatusTimer", TimeValue (MicroSeconds (100.0)));
  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (320));
  Config::SetDefault ("ns3::LteEnbRrc::FirstSibTime", UintegerValue (2));
  Config::SetDefault ("ns3::MmWavePointToPointEpcHelper::X2LinkDelay", TimeValue (MicroSeconds (x2Latency)));
  Config::SetDefault ("ns3::MmWavePointToPointEpcHelper::X2LinkDataRate", DataRateValue (DataRate ("1000Gb/s")));
  Config::SetDefault ("ns3::MmWavePointToPointEpcHelper::X2LinkMtu",  UintegerValue (10000));
  Config::SetDefault ("ns3::MmWavePointToPointEpcHelper::S1uLinkDelay", TimeValue (MicroSeconds (1000)));
  Config::SetDefault ("ns3::MmWavePointToPointEpcHelper::S1apLinkDelay", TimeValue (MicroSeconds (mmeLatency)));
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (bufferSize * 1024 * 1024));
  Config::SetDefault ("ns3::LteRlcUmLowLat::MaxTxBufferSize", UintegerValue (bufferSize * 1024 * 1024));
  Config::SetDefault ("ns3::LteRlcAm::StatusProhibitTimer", TimeValue (MilliSeconds (10.0)));
  Config::SetDefault ("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue (bufferSize * 1024 * 1024));

    // handover and RT related params
  switch (hoMode)
    {
    case 1:
      Config::SetDefault ("ns3::LteEnbRrc::SecondaryCellHandoverMode", EnumValue (LteEnbRrc::THRESHOLD));
      break;
    case 2:
      Config::SetDefault ("ns3::LteEnbRrc::SecondaryCellHandoverMode", EnumValue (LteEnbRrc::FIXED_TTT));
      break;
    case 3:
      Config::SetDefault ("ns3::LteEnbRrc::SecondaryCellHandoverMode", EnumValue (LteEnbRrc::DYNAMIC_TTT));
      break;
    }

  Config::SetDefault ("ns3::LteEnbRrc::FixedTttValue", UintegerValue (150));
  Config::SetDefault ("ns3::LteEnbRrc::CrtPeriod", IntegerValue (ReportTablePeriodicity));
  Config::SetDefault ("ns3::LteEnbRrc::OutageThreshold", DoubleValue (outageTh));
  Config::SetDefault ("ns3::MmWaveEnbPhy::UpdateSinrEstimatePeriod", IntegerValue (ReportTablePeriodicity));
  Config::SetDefault ("ns3::MmWaveEnbPhy::Transient", IntegerValue (vectorTransient));
  Config::SetDefault ("ns3::MmWaveEnbPhy::NoiseAndFilter", BooleanValue (noiseAndFilter));

  GlobalValue::GetValueByName ("lteUplink", booleanValue);
  bool lteUplink = booleanValue.Get ();

  Config::SetDefault ("ns3::McUePdcp::LteUplink", BooleanValue (lteUplink));
  std::cout << "Lte uplink " << lteUplink << "\n";

  
  Config::SetDefault ("ns3::McUeNetDevice::AntennaNum", UintegerValue(4));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (harqEnabled);
  mmwaveHelper->SetMmWaveUeNetDeviceAttribute("AntennaNum", UintegerValue(4));
  mmwaveHelper->SetMmWaveEnbNetDeviceAttribute("AntennaNum", UintegerValue(16));
  mmwaveHelper->Initialize ();

  // Get SGW/PGW and create a single RemoteHost
  Ptr<Node> mme = epcHelper->GetMmeNode ();
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  std::cout << "pgw: " << pgw->GetId () << std::endl;
  std::cout << "mme: " << mme->GetId () << std::endl;
  std::cout << "remote: " << remoteHost->GetId () << std::endl;
  // Create the Internet by connecting remoteHost to pgw. Setup routing too
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // create LTE, mmWave eNB nodes and UE node
  double gNbHeight = 10;
  uint16_t ueNum = 10;
  NodeContainer ueNodes;
  NodeContainer mmWaveEnbNodes;
  NodeContainer lteEnbNodes;
  NodeContainer allEnbNodes;
  mmWaveEnbNodes.Create (gNbNum);
  lteEnbNodes.Create (1);
  ueNodes.Create (ueNum);
  allEnbNodes.Add (lteEnbNodes);
  allEnbNodes.Add (mmWaveEnbNodes);

  for (uint32_t i = 0; i < mmWaveEnbNodes.GetN(); i++)
    {
      std::cout << "gNb:" << i << " : " << mmWaveEnbNodes.Get (i)->GetId ()
          << std::endl;
    }
  for (uint32_t i = 0; i < ueNodes.GetN (); i++)
    {
      std::cout << "ue:" << i << " : " << ueNodes.Get (i)->GetId ()
          << std::endl;
    }
  for (uint32_t i = 0; i < lteEnbNodes.GetN (); i++)
    {
      std::cout << "LTE:" << i << " : " << lteEnbNodes.Get (i)->GetId ()
          << std::endl;
    }

 
  
  MobilityHelper gNbMobility, ueMobility, LteMobility;
  Ptr<ListPositionAllocator> ltepositionAloc = CreateObject<ListPositionAllocator> ();
    
  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator> ();
  if(mobilityType == 0)
    {
      ueMobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                       "MinX", DoubleValue (166.0),
                                       "MinY", DoubleValue (333.0),
                                       "DeltaX", DoubleValue (166.0),
                                       "DeltaY", DoubleValue (333.0),
                                       "GridWidth", UintegerValue (5),
                                       "LayoutType", StringValue ("RowFirst"));
      ueMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    }
  else if (mobilityType == 1)
  {
    Ptr<ListPositionAllocator> staPositionAllocator = CreateObject<ListPositionAllocator> ();
    double x_random, y_random;
    for(uint32_t i = 0; i < ueNodes.GetN(); i ++)
    {
      x_random = (rand() % 1000) + 1;
      y_random = (rand() % 1000) + 1;
      staPositionAllocator->Add (Vector (x_random, y_random, 1.5));
    }
    ueMobility.SetPositionAllocator(staPositionAllocator);
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
      "Bounds", RectangleValue (Rectangle (0, 1000, 0, 1000)),
      "Speed", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minSpeedVal) + "|Max=" + std::to_string(maxSpeedVal) + "]"));
  }
  
  
  ueMobility.Install (ueNodes);
  //BuildingsHelper::Install(ueNodes);
    for (uint32_t i = 0; i < ueNodes.GetN (); i++)
  {
    Ptr<MobilityModel> model = ueNodes.Get (i)->GetObject<MobilityModel> ();
    std::cout << "Node " << i << " Position " << model->GetPosition () << std::endl;
  }
  int p[gNbNum][2] = {};
  deployEnb(p);
  for(int i = 0; i < gNbNum; i++)
   {
       apPositionAlloc->Add (Vector (p[i][0],p[i][1],gNbHeight));
   }

  LteMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  ltepositionAloc->Add(Vector(500.0, 500, 10.0));
  LteMobility.SetPositionAllocator (ltepositionAloc);
  LteMobility.Install (lteEnbNodes);

  gNbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  gNbMobility.SetPositionAllocator (apPositionAlloc);
  gNbMobility.Install (mmWaveEnbNodes);
  //BuildingsHelper::Install(allEnbNodes);
  
  AnimationInterface::SetConstantPosition(mme,580,580);
  AnimationInterface::SetConstantPosition(pgw,640,580);
  AnimationInterface::SetConstantPosition(remoteHostContainer.Get(0),740,580);
  // Install mmWave, lte, mc Devices to the nodes
  NetDeviceContainer lteEnbDevs = mmwaveHelper->InstallLteEnbDevice (lteEnbNodes);
  NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice (mmWaveEnbNodes);
  NetDeviceContainer mcUeDevs;
  mcUeDevs = mmwaveHelper->InstallMcUeDevice (ueNodes);
  //ueDevs = mmwaveHelper->InstallUeDevice (ueNodes);

  for (uint32_t j = 0; j < ueNodes.GetN (); j++)
  {
    std::cout <<"Number of ue Net device after "<<j<<" iter :" << ueNodes.Get(j)->GetNDevices()<<std::endl;
  }
  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (mcUeDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Add X2 interfaces
  mmwaveHelper->AddX2Interface (lteEnbNodes, mmWaveEnbNodes);

  // Manual attachment
  mmwaveHelper->AttachToClosestEnb (mcUeDevs, mmWaveEnbDevs, lteEnbDevs);

   //Handover store in file
  std::fstream fileout;
  fileout.open(outputDir + handoverfilename,std::ios::out | std::ios::trunc);
  fileout << "Time,Event,IMSI,CellId,RNTI,TargetCellId";
  fileout << std::endl; 

  std::cout << "outputDir: " << outputDir << std::endl;
  //MCS and other rxtrace report file
  std::fstream tracefile1;
  tracefile1.open (outputDir + tracefilename1, std::ios::out | std::ios::trunc);
  tracefile1 << "DL/UL,time,frame,subF,slot,1stSym,symbol#,cellId,rnti,ccId,tbSize,mcs,rv,SINR(dB),corrupt,TBler,SINR_MIN(dB)";
  tracefile1 << std::endl;
  std::ofstream stateChange;
  stateChange.open (outputDir + stateChangefile, std::ios::out | std::ios::trunc);
  stateChange << "Time,Old_state,New_state" << std::endl;
  std::fstream tracefile2;
  tracefile2.open (outputDir + tracefilename2, std::ios::out | std::ios::trunc);
  tracefile2 << "DL/UL,time,frame,subF,slot,1stSym,symbol#,cellId,rnti,ccId,tbSize,mcs,rv,SINR(dB),corrupt,TBler,SINR_MIN(dB)";
  tracefile2 << std::endl;
  std::ofstream energyFile;
  energyFile.open (outputDir + energyFileName,std::ios::out | std::ios::trunc);
  energyFile << "Time,EnergyConsumption,StateEnergy" << std::endl;
  /*Energy Framework*/
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (10));
  basicSourceHelper.Set ("BasicEnergySupplyVoltageV", DoubleValue (5.0));
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (1000.0));
  // Install Energy Source
  EnergySourceContainer sources = basicSourceHelper.Install (ueNodes.Get (8));
  // Device Energy Model
  MmWaveRadioEnergyModelHelper nrEnergyHelper;
  DeviceEnergyModelContainer deviceEnergyModel = nrEnergyHelper.Install (ueNodes.Get(8)->GetDevice (0), sources);
  //Install and start applications on UEs and remote host
  deviceEnergyModel.Get(0)->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&EnergyConsumptionUpdate));
  // Install and start applications on UEs and remote host
  uint16_t dlPort = 1234;
  ApplicationContainer clientApps, serverApps;


  //	ApplicationContainer clientAppsEmbb, serverAppsEmbb;

  EnDashServerHelper dashSrHelper (dlPort);
  serverApps.Add (dashSrHelper.Install (remoteHost));

  int counter = 0;
  EnDashClientHelper dlClient (internetIpIfaces.GetAddress (1), dlPort); //Remotehost is the second node, pgw is first
  //dlClient.SetAttribute ("Size", UintegerValue (0xFFFFFF));
  //dlClient.SetAttribute ("NumberOfDownload", UintegerValue (1));
  dlClient.SetAttribute ("OnStartCB",
                         CallbackValue (MakeBoundCallback (onStart, &counter)));
  dlClient.SetAttribute ("OnStopCB",
                         CallbackValue (MakeBoundCallback (onStop, &counter)));
  
  dlClient.SetAttribute ("NodeTracePath", StringValue (outputDir + "/tracedashpensieve"));
  dlClient.SetAttribute ("NodeTraceInterval", TimeValue (Seconds (nodeTraceInterval)));
  dlClient.SetAttribute ("NodeTraceHelperCallBack", CallbackValue (MakeCallback (readNodeTrace)));
  dlClient.SetAttribute ("NodeTraceHelperCallBack", CallbackValue (MakeBoundCallback (readNodeTrace, &mmWaveEnbNodes)));
  dlClient.SetAttribute ("TracePath", StringValue (outputDir + "/TraceDataDashpensieve"));
  dlClient.SetAttribute("AbrLogPath", StringValue (outputDir + "/AbrDataDashpensieve"));
  dlClient.SetAttribute("AbrPort", UintegerValue(AbrPortVal));
  dlClient.SetAttribute("PathtoRSSI", StringValue(outputDir + tracefilename1));
  dlClient.SetAttribute("PathtoSpeed", StringValue(outputDir + "/tracedashpensieve_17.csv"));
  dlClient.SetAttribute ("Timeout", TimeValue(Seconds(-1)));

  if (!outputDir.empty ())
    dlClient.SetAttribute ("TracePath",
                           StringValue (outputDir + "/SomeData"));
  // configure here UDP traffic
  for (uint32_t j = 0; j < ueNodes.GetN (); j++)
  {
    std::cout <<"Ip Address of node "<<ueNodes.Get(j)->GetId()<<" "<<ueNodes.Get(j)->GetObject<Ipv4>()->GetAddress(1,0)<<std::endl;
  }
  std::cout<<"Ip Address of remoteHost "<<internetIpIfaces.GetAddress(1)<<std::endl;
  
  for (uint32_t j = 0; j < ueNodes.GetN (); j++)
    {
      if (j == 8)
      {
        clientApps.Add (dlClient.Install (ueNodes.Get (j)));
        Ptr<NetDevice> uenetDev = ueNodes.Get(j)->GetDevice(0);
        Ptr<EpcTft> tft = Create<EpcTft> ();
        EpcTft::PacketFilter dlpf;
        dlpf.localPortStart = dlPort;
        dlpf.localPortEnd = dlPort;
        dlPort++;
        tft->Add (dlpf);
        //SIGSEGV error
        // enum EpsBearer::Qci q;

        // q = EpsBearer::GBR_CONV_VOICE;
        
        // EpsBearer bearer (q);
        // mmwaveHelper->ActivateDataRadioBearer(mcUeDevs.Get(j), bearer);
        }
      
    }
  // start UDP server and client apps

  
  serverApps.Start (Seconds (udpAppStartTime));
  clientApps.Start (Seconds (udpAppStartTime + 0.5));

  double numPrints = 10;
  for (int i = 0; i < numPrints; i++)
    {
      Simulator::Schedule (Seconds (i * simTime / numPrints), &PrintPosition, ueNodes.Get (0));
    }
  //TODO  SIGSEGV ERROR
  //for (size_t i = 0; i < buildingVector.size(); i++)
//  {
  //  MobilityBuildingInfo (buildingVector[i]);
 // }

  //BuildingsHelper::MakeMobilityModelConsistent ();
  //Simulator::Stop (Seconds (simTime));

  // connect custom trace sinks for RRC connection establishment and handover notification
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                   MakeCallback (&NotifyHandoverStartEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                   MakeCallback (&NotifyHandoverStartUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                   MakeCallback (&NotifyHandoverEndOkEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                   MakeCallback (&NotifyHandoverEndOkUe));

  Config::Connect ("/NodeList/"+std::to_string (ueNodes.Get (8)->GetId ())+"/DeviceList/*/MmWaveComponentCarrierMapUe/*/MmWaveUePhy/DlSpectrumPhy/RxPacketTraceUe",
                   MakeCallback (&traceuefunc));

  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMap/*/MmWaveEnbPhy/DlSpectrumPhy/RxPacketTraceEnb",
                   MakeCallback (&traceenbfunc));
  Ptr<MmWaveUePhy> mmwaveUePhy = mcUeDevs.Get(8)->GetObject<McUeNetDevice> ()->GetMmWavePhy ();
  Ptr<MmWaveSpectrumPhy> mmwaveDlSpectrumPhy = mmwaveUePhy->GetDlSpectrumPhy();
  Ptr<MmWaveSpectrumPhy> mmwaveUlSpectrumPhy = mmwaveUePhy->GetUlSpectrumPhy ();
  mmwaveDlSpectrumPhy->TraceConnectWithoutContext ("State", MakeCallback (&IntStateChange));
  mmwaveUlSpectrumPhy->TraceConnectWithoutContext ("State", MakeCallback (&IntStateChange));
  
  for (uint32_t i = 0; i < mmWaveEnbNodes.GetN(); i++)
  {
    Ptr<MmWaveEnbPhy> mmwaveEnbPhy = mmWaveEnbDevs.Get(i)->GetObject<MmWaveEnbNetDevice> ()->GetPhy ();
    Ptr<MmWaveSpectrumPhy> enbDlPhy = mmwaveEnbPhy->GetDlSpectrumPhy ();
    Ptr<MmWaveSpectrumPhy> enbUlPhy = mmwaveEnbPhy->GetUlSpectrumPhy ();
    enbDlPhy->TraceConnectWithoutContext ("State", MakeBoundCallback (&EnbStateChange, i));
    enbUlPhy->TraceConnectWithoutContext ("State", MakeBoundCallback (&EnbStateChange, i));
  }

  if (sleepEnabled == 1)
  {
    for (uint32_t i = 0; i < mmWaveEnbNodes.GetN(); i++)
  {
  Ptr<MmWaveEnbPhy> enbPhy = mmWaveEnbDevs.Get (i)->GetObject<MmWaveEnbNetDevice> ()->GetPhy ();
  Ptr<MmWaveSpectrumPhy> enbdl = enbPhy->GetDlSpectrumPhy ();
  Ptr<MmWaveSpectrumPhy> enbul = enbPhy->GetUlSpectrumPhy ();
  Simulator::Schedule(Seconds(sleepStartTime), &MakeBaseStationSleep, enbdl, enbul, true);
  Simulator::Schedule(Seconds(sleepStopTime), &MakeBaseStationSleep, enbdl, enbul, false);
  }
  }

  //Simulator::Schedule(Seconds(10.0), &MakeBaseStationSleep, enbdl, enbul, true);
  //Simulator::Schedule(Seconds(20.0), &MakeBaseStationSleep, enbdl, enbul, false);

  AnimationInterface anim ("animation-dash-pensieve.xml");
  //Enable pdcp trace
  
  mmwaveHelper->EnablePdcpTraces ();

  for (uint32_t i = 0; i < lteEnbNodes.GetN(); i++)
  {
    anim.UpdateNodeDescription(lteEnbNodes.Get(i), "LTE eNb");
    anim.UpdateNodeColor (lteEnbNodes.Get(i), 255, 0, 0);
  }
  for (uint32_t i =0; i < mmWaveEnbNodes.GetN(); i++)
  {
    anim.UpdateNodeDescription(mmWaveEnbNodes.Get(i), "gNb"+std::to_string(i));
    anim.UpdateNodeColor(mmWaveEnbNodes.Get(i),0,255,0);
  }
  for (uint32_t i=0; i< ueNodes.GetN(); i++)
  {
    anim.UpdateNodeDescription(ueNodes.Get(i), "UE"+std::to_string(i));
    anim.UpdateNodeColor (ueNodes.Get(i),0,0,255);
  }
  anim.UpdateNodeDescription(pgw,"PGW");
  anim.UpdateNodeDescription(mme,"MME");
  anim.UpdateNodeDescription(remoteHostContainer.Get(0),"Remote Host");
  std::cout << "outputDir : " << outputDir << std::endl;
  auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::cout << "Simulation started at: " << ctime(&timenow) << std::endl; 
  std::fstream simtimefile;
  simtimefile.open(outputDir + simfilename,std::ios::out | std::ios::trunc);
  simtimefile << "Type,Simulation Time,RealTime" << std::endl;
  simtimefile << "Start," << Simulator::Now().GetSeconds() << "," << ctime(&timenow) << std::endl;

  Simulator::Run ();
  
  NodeContainer endpointNodes;
  endpointNodes.Add (remoteHost);
  endpointNodes.Add (ueNodes);

  Simulator::Destroy ();
  return 0;
}

