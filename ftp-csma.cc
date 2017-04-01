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

NS_LOG_COMPONENT_DEFINE ("FTP-CSMA");

int main (int argc,char *argv[])
{
	Packet::EnableChecking();
	Packet::EnablePrinting();	
	uint32_t nVoipSta = 1;
	uint32_t nFtpSta = 1;
	//uint32_t nSta = nVoipSta + nFtpSta;
	uint32_t nSta = 2;
	uint32_t nVoipSer = 1;
	uint32_t nFtpSer = 1;
	uint32_t nCsma = nFtpSer;
	bool verbose = true;
	if(verbose)
	{
		LogComponentEnable("ApWifiMac", LOG_LEVEL_ALL);
		LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
	}
	
	std::string animFile = "ftp-csma-animation.xml";	
	
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (2200));
	Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",  StringValue ("2200")); 
	//create nodes
	NodeContainer p2pNode;
	p2pNode.Create (2);

	NodeContainer wifiApNode;
	wifiApNode.Add (p2pNode.Get(0));
	NodeContainer wifiStaNode;
	wifiStaNode.Create (nSta);
	//NodeContainer rtNodes = NodeContainer(p2pNode.Get(1),p2pNode.Get(2));
  
	NodeContainer csmaNode;
	csmaNode.Add(p2pNode.Get(1));
	csmaNode.Create (1);
    
	//create & setup wifi channel
	YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
	phy.SetChannel(channel.Create());

	//install wire devices
	PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
	
	NetDeviceContainer ap2switch;
	ap2switch = pointToPoint.Install (p2pNode.Get(0), p2pNode.Get(1));
	//NetDeviceContainer switch2server;
	//switch2server = pointToPoint.Install (p2pNode.Get(1), p2pNode.Get(2)); // for p2p server

	CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", StringValue("100Mbps"));
	csma.SetChannelAttribute ("Delay", TimeValue(NanoSeconds (6560)));
	csma.SetDeviceAttribute ("Mtu", UintegerValue(1508));
  
	NetDeviceContainer csmaDevice;
	csmaDevice = csma.Install (csmaNode);
  
  
	//install wireless devices
	WifiHelper wifi = WifiHelper::Default();
	wifi.SetRemoteStationManager("ns3::AarfWifiManager");

	NqosWifiMacHelper mac = NosWifiMacHelper::Default();
	
	Ssid ssid = Ssid("ns-3-ssid");
	mac.SetType("ns3::StaWifiMac","Ssid",SsidValue(ssid),"ActiveProbing",BooleanValue(true));
	
	NetDeviceContainer staDevices;
	staDevices = wifi.Install(phy, mac, wifiStaNode);

	mac.SetType("ns3::ApWifiMac","Ssid",SsidValue(ssid));

	NetDeviceContainer apDevices;
	apDevices = wifi.Install(phy, mac , wifiApNode);

	//setting mobility model
	MobilityHelper mobility;
	mobility.SetPositionAllocator("ns3::GridPositionAllocator",
					"MinX", DoubleValue (0.0),
					"MinY", DoubleValue (0.0),
					"DeltaX", DoubleValue (5.0),
					"DeltaY", DoubleValue (10.0),
					"GridWidth", UintegerValue (3),
					"LayoutType", StringValue ("RowFirst"));
	mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
	                          "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
	mobility.Install(wifiStaNode);
  
  	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  	positionAlloc->Add(Vector(0, 50, 0));
  	positionAlloc->Add(Vector(0, 150, 0));
  	positionAlloc->Add(Vector(0, 250, 0));
  	//positionAlloc->Add(Vector(100, 200, 0));
  	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(wifiApNode);
	mobility.Install(csmaNode);
	
	InternetStackHelper stack;
	stack.Install(csmaNode);
	stack.Install(wifiApNode);
	stack.Install(wifiStaNode);

	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0","255.255.255.0");
	Ipv4InterfaceContainer ap2swInterface;
	ap2swInterface = address.Assign (ap2switch);

	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer wifiInterfaces;
	wifiInterfaces = address.Assign(staDevices);
	Ipv4InterfaceContainer wifiApInterfaces;
	wifiApInterfaces = address.Assign(apDevices);

	address.SetBase("10.1.3.0","255.255.255.0");
	Ipv4InterfaceContainer sw2svInterface;
	sw2svInterface = address.Assign (csmaDevice);

	//tcp receiver
	Address remoteAddress1 = InetSocketAddress(wifiInterfaces.GetAddress(0),2000);
	PacketSinkHelper receiver1("ns3::TcpSocketFactory", remoteAddress1);
	ApplicationContainer receiverapp1 = receiver1.Install(wifiStaNode.Get(0));
	receiverapp1.Start(Seconds(1.0));
	receiverapp1.Stop(Seconds(3.0));

	Address remoteAddress2 = InetSocketAddress(wifiInterfaces.GetAddress(1),4000);
	PacketSinkHelper receiver2("ns3::TcpSocketFactory", remoteAddress1);
	ApplicationContainer receiverapp2 = receiver1.Install(wifiStaNode.Get(1));
	receiverapp2.Start(Seconds(1.2));
	receiverapp2.Stop(Seconds(3.2));

	//tcp sender
	OnOffHelper sender1("ns3::TcpSocketFactory", remoteAddress1);
	sender1.SetConstantRate(DataRate("1Mb/s"), 300);
	OnOffHelper sender2("ns3::TcpSocketFactory", remoteAddress2);
	sender2.SetConstantRate(DataRate("1Mb/s"), 300);
	
	ApplicationContainer senderapp = sender1.Install(csmaNode.Get(1));
	senderapp.Add(sender2.Install(csmaNode.Get(1)));
	senderapp.Start(Seconds(1.0));
	senderapp.Stop(Seconds(3.2));

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	
	AnimationInterface anim (animFile);
	
	Simulator::Stop(Seconds(4.0));
	pointToPoint.EnablePcapAll("P2p_ftp");
	csma.EnablePcapAll("Csma_ftp");
	Simulator::Run();
	Simulator::Destroy();
	return 0;
	
}
