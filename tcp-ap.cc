/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Md Ashiqur Rahman: University of Arizona.
 *
 **/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-velocity-mobility-model.h"

#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>


NS_LOG_COMPONENT_DEFINE ("tcp-wifi-ap");

using namespace ns3;

int
main( int argc, char *argv[])
{
  std::string phyMode = "DsssRate1Mbps";
  
  int staNodes = 1;
  int apNodes = 4;
  int spacing = 200;
  int range = 110;
  double endtime = 10.0;
  double speed = (double)(apNodes*spacing)/endtime; //setting speed to span full sim time
  
  std::string animFile = "ap-tcp-animation.xml";
  
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5kb/s"));
  // Control TCP fragmentation
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (2200));
  Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
  //Config::SetDefault ("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue (true));

  CommandLine cmd;
  cmd.AddValue ("animFile", "File Name for Animation Output", animFile);
  cmd.Parse (argc, argv);
  
  // Here, we will create one server, N client and M wifinodes nodes in a star.
  NS_LOG_INFO ("Create nodes.");
  
  NodeContainer serverNode;
  NodeContainer switchNode;
  NodeContainer clientNodes;
  NodeContainer wifiApNodes;
  serverNode.Create (1);
  switchNode.Create (1);
  clientNodes.Create (staNodes);
  wifiApNodes.Create (apNodes);
  NodeContainer allNodes = NodeContainer (serverNode, clientNodes, wifiApNodes, switchNode);
  
  // Install netwok stack
  InternetStackHelper internet;
  internet.Install (allNodes);
  
  ////////////////////////////////////////////////////////////////////////////////////////
  // Wired topology
  ////////////////////////////////////////////////////////////////////////////////////////
  std::vector<NodeContainer> nodeAdjacencyList (apNodes+1);
  for(uint32_t i=0; i<nodeAdjacencyList.size()-1; i++)
  {
    nodeAdjacencyList[i] = NodeContainer (switchNode, wifiApNodes.Get(i));
  }
  nodeAdjacencyList[apNodes] = NodeContainer (serverNode, switchNode);
  
  // We create the wired channels first without any IP addressing informatio
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  //p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  //p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  
  std::vector<NetDeviceContainer> deviceAdjacencyList (apNodes+1);
  for(uint32_t i=0; i<deviceAdjacencyList.size(); ++i)
  {
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    std::string delay = std::to_string(2+i) + "ms";
    p2p.SetChannelAttribute ("Delay", StringValue (delay));
    deviceAdjacencyList[i] = p2p.Install (nodeAdjacencyList[i]);
  }
  
  
  ////////////////////////////////////////////////////////////////////////////////////////
  // Wireless topology
  ////////////////////////////////////////////////////////////////////////////////////////
  // disable fragmentation, RTS/CTS and enable non-unicast data rate for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
  
  // Wifi helper setup
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager"); // Use AARF rate control
  
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  
  // The following has an absolute cutoff at distance > range (range == radius)
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", 
                                  "MaxRange", DoubleValue(range));
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));
  
  // Setup rest of upper MAC
  Ssid ssid = Ssid ("wifi-default");
  
  // Setup: STA; Add a non-QoS upper mac of STAs, and disable rate control
  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();
  
  // Active associsation of STA to AP via probing.
  wifiMacHelper.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid),
                         "ActiveProbing", BooleanValue (true));
                         //"ProbeRequestTimeout", TimeValue(Seconds(0.25)));
   
  NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMacHelper, clientNodes);
  NetDeviceContainer devices = staDevice;
  
  // Setup AP.
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
                   //"BeaconGeneration", BooleanValue(false));
  
  NetDeviceContainer apDevices = wifi.Install (wifiPhy, wifiMac, wifiApNodes);
  devices.Add (apDevices);
  
  // Set positions for APs 
  MobilityHelper sessile;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  int Xpos = 0;
  for (int i=0; i < apNodes; i++) 
  {
    positionAlloc->Add(Vector(100+Xpos, 0.0, 0.0));
    Xpos += spacing;
  }
  positionAlloc->Add(Vector(600, 200, 0.0));
  positionAlloc->Add(Vector(600, 400, 0.0));
  
  sessile.SetPositionAllocator (positionAlloc);
  sessile.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  sessile.Install (wifiApNodes);
  sessile.Install (switchNode);
  sessile.Install (serverNode);
  
  // Setting mobility model and movement parameters for mobile nodes
  // ConstantVelocityMobilityModel is a subclass of MobilityModel
  MobilityHelper mobile; 
  mobile.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobile.Install(clientNodes);
  // Setting each mobile client 100m apart from each other
  int nxt = 0;
  for (int i=0; i<staNodes ; i++)
  {
    Ptr<ConstantVelocityMobilityModel> cvmm = clientNodes.Get(i)->GetObject<ConstantVelocityMobilityModel> ();
    Vector pos (0-nxt, 0, 0);
    Vector vel (speed, 0, 0);
    cvmm->SetPosition(pos);
    cvmm->SetVelocity(vel);
    nxt += 100;
  }
  
  
  //////////////////////////////////////////////////////////////////////////////
  // Add IP addresses.
  //////////////////////////////////////////////////////////////////////////////
  NS_LOG_INFO ("Assign IP Addresses.");
  
  //////////////////////////////////////////////////////
  // Wired interfaces
  //////////////////////////////////////////////////////
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaceAdjacencyList = ipv4.Assign (deviceAdjacencyList[apNodes]);
  for(int i=0; i<apNodes; ++i)
  {
    std::ostringstream subnet;
    subnet<<"10.1."<<i+2<<".0";
    ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
    ipv4.Assign (deviceAdjacencyList[i]);
  }
  
  
  //////////////////////////////////////////////////////
  // Wireless interfaces
  //////////////////////////////////////////////////////
  ipv4.SetBase ("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces = ipv4.Assign (staDevice);
  Ipv4InterfaceContainer wifiApInterfaces = ipv4.Assign (apDevices);
  
  // Install TCP application
  // Create a packet sink on the clientNodes to receive these packets
  uint16_t port = 5000;
  Address sinkLocalAddress (InetSocketAddress (wifiInterfaces.GetAddress (0), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (clientNodes.Get(0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (4.0));

  // Create the OnOff applications to send TCP to the server
  OnOffHelper senderHelper ("ns3::TcpSocketFactory", sinkLocalAddress );
  senderHelper.SetConstantRate(DataRate("1Mb/s"), 1024);
  //clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  //clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer senderApp = senderHelper.Install(serverNode.Get(0));
  senderApp.Start (Seconds (1.0));
  senderApp.Stop (Seconds (4.0));
  
  //Simulator::Stop (Seconds (endtime));
  
  //Turn on global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  AnimationInterface anim (animFile);
  
  Simulator::Stop(Seconds(5.0));

  //configure tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("tcp-ap-server.tr"));
  p2p.EnablePcapAll ("tcp-ap-server");
  
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO ("Done.");
  return 0;
}
