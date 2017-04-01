#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/application-container.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/netanim-module.h"

#include "iostream"
#include "string"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("Test1");

int main (int argc,char *argv[])
{
	Packet::EnableChecking();
	Packet::EnablePrinting();	
	uint32_t nVoipSta = 1;
	uint32_t nFtpSta = 1;
	uint32_t nSta = nVoipSta + nFtpSta;
	uint32_t nVoipSer = 1;
	uint32_t nFtpSer = 1;
	uint32_t nCsma = nVoipSer + nFtpSer;
	bool verbose = true;
	if(verbose)
	{
		LogComponentEnable("ApWifiMac", LOG_LEVEL_ALL);
		LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
	}
	
	std::string animFile = "ftp-udp-animation.xml";	
	
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (2200));
	Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",  StringValue ("2200")); 
	//create nodes
	NodeContainer p2pNode;
	p2pNode.Create (2);

	NodeContainer wifiApNode;
	wifiApNode.Add (p2pNode.Get(0));
	NodeContainer wifiStaNode;
	wifiStaNode.Create (nSta);

	NodeContainer csmaNode;
	csmaNode.Add(p2pNode.Get(1));
	csmaNode.Create (nCsma);

	//install wireless devices
	WifiHelper wifi = WifiHelper::Default();
	wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
	wifi.SetRemoteStationManager("ns3::AarfWifiManager");

	//create & setup wifi channel
	YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
	channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  
  // The following has an absolute cutoff at distance > range (range == radius)
  channel.AddPropagationLoss ("ns3::RangePropagationLossModel", 
                                  "MaxRange", DoubleValue(100));
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
	phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
	phy.SetChannel(channel.Create());

	//install wire devices
	PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
	
	NetDeviceContainer p2pDevice;
	p2pDevice = pointToPoint.Install (p2pNode);

	CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", StringValue("100Mbps"));
	csma.SetChannelAttribute ("Delay", TimeValue(NanoSeconds (6560)));
	csma.SetDeviceAttribute ("Mtu", UintegerValue(1508));

	NetDeviceContainer csmaDevice;
	csmaDevice = csma.Install (csmaNode);

	NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
	
	Ssid ssid = Ssid("ns-3-ssid");
	mac.SetType("ns3::StaWifiMac","Ssid",SsidValue(ssid),"ActiveProbing",BooleanValue(true));
	
	NetDeviceContainer staDevices;
	staDevices = wifi.Install(phy, mac, wifiStaNode);

	mac.SetType("ns3::ApWifiMac","Ssid",SsidValue(ssid));

	NetDeviceContainer apDevices;
	apDevices = wifi.Install(phy, mac , wifiApNode);

	//setting mobility model
	MobilityHelper mobility;
	mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
	mobility.Install(wifiStaNode);
  // Setting each mobile client 100m apart from each other
  int nxt = 220;
  for (uint32_t i=0; i<nSta ; i++)
  {
    Ptr<ConstantVelocityMobilityModel> cvmm = wifiStaNode.Get(i)->GetObject<ConstantVelocityMobilityModel> ();
    Vector pos (0-nxt, 0, 0);
    Vector vel (12, 0, 0);
    cvmm->SetPosition(pos);
    cvmm->SetVelocity(vel);
    nxt += 50;
  }

  MobilityHelper mobStation;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add(Vector(0, 0, 0));
  positionAlloc->Add(Vector(0, 100, 0));
  positionAlloc->Add(Vector(-200, 200, 0));
  positionAlloc->Add(Vector(200, 200, 0));
  mobStation.SetPositionAllocator (positionAlloc);
	mobStation.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobStation.Install(wifiApNode);
	mobStation.Install(csmaNode);
	
	InternetStackHelper stack;
	stack.Install(csmaNode);
	stack.Install(wifiApNode);
	stack.Install(wifiStaNode);

	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0","255.255.255.0");
	Ipv4InterfaceContainer p2pInterface;
	p2pInterface = address.Assign (p2pDevice);

	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer wifiInterfaces;
	wifiInterfaces = address.Assign(staDevices);
	Ipv4InterfaceContainer wifiApInterfaces;
	wifiApInterfaces = address.Assign(apDevices);

	address.SetBase("10.1.3.0","255.255.255.0");
	Ipv4InterfaceContainer csmaInterface;
	csmaInterface = address.Assign (csmaDevice);

	//tcp receiver
	Address remoteAddress1 = InetSocketAddress(wifiInterfaces.GetAddress(1),2000);
	PacketSinkHelper receiver1("ns3::TcpSocketFactory", remoteAddress1);
	ApplicationContainer receiverapp1 = receiver1.Install(wifiStaNode.Get(1));
	receiverapp1.Start(Seconds(1.0));
	receiverapp1.Stop(Seconds(9.0));

	//tcp sender
	OnOffHelper sender1("ns3::TcpSocketFactory", remoteAddress1);
	sender1.SetConstantRate(DataRate("1Mb/s"), 1024);
	//DataRate dataRate ("10Mb/s");
        //sender1.SetAttribute ("DataRate", DataRateValue (dataRate));
	//sender1.SetAttribute ("PacketSize", UintegerValue (1508)); 
	//sender1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
	//sender1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
	ApplicationContainer senderapp1 = sender1.Install(csmaNode.Get(1));
	senderapp1.Start(Seconds(1.0));
	senderapp1.Stop(Seconds(9.0));
	

	//voip receiver
	Address remoteAddress2 = InetSocketAddress(wifiInterfaces.GetAddress(0), 4000);
	PacketSinkHelper receiver2("ns3::UdpSocketFactory", remoteAddress2);
	ApplicationContainer receiverapp2 = receiver2.Install(wifiStaNode.Get(0));
	receiverapp2.Start(Seconds(2.0));
	receiverapp2.Stop(Seconds(9.0));
	
	//voip sender
	OnOffHelper sender2("ns3::UdpSocketFactory", remoteAddress2);
	sender2.SetConstantRate(DataRate("65.625Kbps"), 1024);
	//sender2.SetAttribute ("DataRate", StringValue ("10Mb/s"));
	//sender2.SetAttribute ("PacketSize", UintegerValue (1508)); 
	//sender2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
	//sender2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVarialbe[Constant=0.0]"));
	ApplicationContainer senderapp2 = sender2.Install(csmaNode.Get(2));
	senderapp2.Start(Seconds(2.0));
	senderapp2.Stop(Seconds(9.0));
	

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	
	AnimationInterface anim (animFile);
	
	Simulator::Stop(Seconds(9.0));
	pointToPoint.EnablePcapAll("P2p_mytest1");
	csma.EnablePcapAll("Csma_mytest1");
	Simulator::Run();
	Simulator::Destroy();
	return 0;
	
}
