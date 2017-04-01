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

NS_LOG_COMPONENT_DEFINE ("TCP-FTP");

int
main (int argc,char *argv[])
{
	Packet::EnableChecking();
	Packet::EnablePrinting();	
	uint32_t nVoipSta = 1;
	uint32_t nFtpSta = 1;
	//uint32_t nSta = nVoipSta + nFtpSta;
	uint32_t nSta = 1;
	uint32_t nVoipSer = 1;
	uint32_t nFtpSer = 1;
	//uint32_t nCsma = nVoipSer + nFtpSer;
	uint32_t nCsma = 1;
	bool verbose = true;
	if(verbose)
	{
		LogComponentEnable("ApWifiMac", LOG_LEVEL_ALL);
		LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
	}

	std::string animFile = "tcp-ftp-animation.xml";	

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

	//create & setup wifi channel
	YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
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

	//install wireless devices
	WifiHelper wifi = WifiHelper::Default();
	wifi.SetRemoteStationManager("ns3::AarfWifiManager");

	QosWifiMacHelper mac = QosWifiMacHelper::Default();

	Ssid ssid = Ssid("ns-3-ssid");
	mac.SetType("ns3::StaWifiMac","Ssid",SsidValue(ssid),"ActiveProbing",BooleanValue(true));

	NetDeviceContainer staDevices;
	staDevices = wifi.Install(phy, mac, wifiStaNode);

	mac.SetType("ns3::ApWifiMac","Ssid",SsidValue(ssid));

	NetDeviceContainer apDevices;
	apDevices = wifi.Install(phy, mac , wifiApNode);

	//setting mobility model
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add(Vector(-200, 50, 0)); // STA
	positionAlloc->Add(Vector(0, 0, 0)); // AP
  positionAlloc->Add(Vector(0, 100, 0)); // Switch
  positionAlloc->Add(Vector(0, 200, 0)); // FTP
  //positionAlloc->Add(Vector(500, 200, 0)); // UDP
	mobility.SetPositionAllocator (positionAlloc);
	
	mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
	Ptr<ConstantVelocityMobilityModel> cvmm = wifiStaNode.Get(0)->GetObject<ConstantVelocityMobilityModel> ();
	mobility.Install(wifiStaNode);
	Vector vel (12, 0, 0);
  cvmm->SetVelocity(vel);

	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(wifiApNode);
	mobility.Install(csmaNode);
	
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
	Address remoteAddress1 = InetSocketAddress(wifiInterfaces.GetAddress(0),2000);
	PacketSinkHelper receiver1("ns3::TcpSocketFactory", remoteAddress1);
	ApplicationContainer receiverapp1 = receiver1.Install(wifiStaNode.Get(0));
	receiverapp1.Start(Seconds(1.0));
	receiverapp1.Stop(Seconds(3.0));

	//tcp sender
	OnOffHelper sender1("ns3::TcpSocketFactory", remoteAddress1);
	sender1.SetConstantRate(DataRate("1Mb/s"), 1024);
	ApplicationContainer senderapp1 = sender1.Install(csmaNode.Get(1));
	senderapp1.Start(Seconds(1.0));
	senderapp1.Stop(Seconds(3.0));

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	
	AnimationInterface anim (animFile);
	
	Simulator::Stop(Seconds(4.0));
	pointToPoint.EnablePcapAll("P2p_mytest1");
	csma.EnablePcapAll("Csma_mytest1");
	Simulator::Run();
	Simulator::Destroy();
	return 0;
	
}
