#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/random-variable-stream-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("sender_wifi_p2p_csma_receiver_tcp");

// ======================================================
//
//          sender                          receiver
//  n2  n3  n4  n0      ---     n1  n5  n6  n7
//      Wifi            p2p         LAN
//      10.1.2.x        10.1.1.x    10.1.3.x
//
// ======================================================


/* loss callback */
static void RxDrop(Ptr<PcapFileWrapper> file, const std::string & dropType, Ptr<const Packet> p) {
    if (dropType == "receiver") {
        NS_LOG_UNCOND("ReceiverRxDrop at " << Simulator::Now().GetSeconds());
        file->Write(Simulator::Now(), p);
    }
    else if (dropType == "inter") {
        NS_LOG_UNCOND("InterRxDrop at " << Simulator::Now().GetSeconds());
        file->Write(Simulator::Now(), p);
    }
}


int main(int argc, char *argv[]) {

    std::string verbose = "all";
    bool tracing = true;
    double seconds = 10.0;

    std::string p2pDataRate = "5Mbps";
    std::string p2pDelay = "50ms";

    uint32_t wifiNumber = 3;

    uint32_t csmaNumber = 3;
    std::string csmaDataRate = "100Mbps";
    uint32_t csmaDelay = 6560;

    std::string senderOnTime = "1.0";
    std::string senderOffTime = "1.0";
    uint32_t senderPacketSize = 1024;
    std::string senderDataRate = "1Mbps";

    std::string receiverRanVarMin = "0.8";
    std::string receiverRanVarMax = "1.0";
    double receiverErrorRate = 0.001;

    std::string interRanVarMin = "0.8";
    std::string interRanVarMax = "1.0";
    double interErrorRate = 0.001;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable tracing", tracing);
    cmd.AddValue("seconds", "Simulation duration seconds", seconds);
    cmd.AddValue("p2pDataRate", "Point to point DataRate", p2pDataRate);
    cmd.AddValue("p2pDelay", "Point to point Delay", p2pDelay);
    cmd.AddValue("wifiNumber", "Wifi nodes number", wifiNumber);
    cmd.AddValue("csmaNumber", "Csma nodes number", csmaNumber);
    cmd.AddValue("csmaDataRate", "Csma DataRate", csmaDataRate);
    cmd.AddValue("csmaDelay", "Csma Delay", csmaDelay);
    cmd.AddValue("senderOnTime", "Sender OnOffTime OnTime", senderOnTime);
    cmd.AddValue("senderOffTime", "Sender OnOffTime OffTime", senderOffTime);
    cmd.AddValue("senderPacketSize", "Send packet size", senderPacketSize);
    cmd.AddValue("senderDataRate", "Send DataRate", senderDataRate);
    cmd.AddValue("receiverRanVarMin", "Receiver RanVar Min", receiverRanVarMin);
    cmd.AddValue("receiverRanVarMax", "Receiver RanVar Max", receiverRanVarMax);
    cmd.AddValue("receiverErrorRate", "Rate in receiver RateErrorModel", receiverErrorRate);
    cmd.AddValue("interRanVarMin", "Inter RanVar Min", interRanVarMin);
    cmd.AddValue("interRanVarMax", "Inter RanVar Max", interRanVarMin);
    cmd.AddValue("interErrorRate", "Rate in inter RateErrorModel", interErrorRate);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    if (verbose == "all") {
        LogComponentEnable("sender_wifi_p2p_csma_receiver_tcp", LOG_LEVEL_ALL);
        LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    }
    else if (verbose == "info") {
        LogComponentEnable("sender_wifi_p2p_csma_receiver_tcp", LOG_LEVEL_INFO);
        LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }


    /* nodes topology */
    NS_LOG_INFO("Creating Topology");

    // p2p
    NodeContainer p2pNodes;
    p2pNodes.Create(2);
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(p2pDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(p2pDelay));
    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(p2pNodes);

    // wifiSender
    YansWifiChannelHelper wifiSenderChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiSenderPhy = YansWifiPhyHelper::Default();
    wifiSenderPhy.SetChannel(wifiSenderChannel.Create());
    WifiHelper wifiSender;
    wifiSender.SetRemoteStationManager("ns3::AarfWifiManager");
    WifiMacHelper wifiSenderMac;
    Ssid wifiSenderSsid = Ssid("ns-3-ssid");

    NodeContainer wifiSenderStaNodes;
    wifiSenderStaNodes.Create(wifiNumber);
    wifiSenderMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(wifiSenderSsid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer wifiSenderStaDevices = wifiSender.Install(wifiSenderPhy, wifiSenderMac, wifiSenderStaNodes);

    NodeContainer wifiSenderApNode = p2pNodes.Get(0);
    wifiSenderMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(wifiSenderSsid));
    NetDeviceContainer wifiSenderApDevice = wifiSender.Install(wifiSenderPhy, wifiSenderMac, wifiSenderApNode);

    MobilityHelper senderMobility;
    senderMobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
    senderMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    senderMobility.Install(wifiSenderStaNodes);
    senderMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    senderMobility.Install(wifiSenderApNode);

    // csmaReceiver
    NodeContainer csmaReceiverNodes;
    csmaReceiverNodes.Add(p2pNodes.Get(1));
    csmaReceiverNodes.Create(csmaNumber);
    CsmaHelper csmaReceiver;
    csmaReceiver.SetChannelAttribute("DataRate", StringValue(csmaDataRate));
    csmaReceiver.SetChannelAttribute("Delay", TimeValue(NanoSeconds(csmaDelay)));
    NetDeviceContainer csmaReceiverDevices;
    csmaReceiverDevices = csmaReceiver.Install(csmaReceiverNodes);


    /* network protocol stack */
    InternetStackHelper stack;
    // stack.Install(p2pNodes);
    stack.Install(wifiSenderApNode);
    stack.Install(wifiSenderStaNodes);
    stack.Install(csmaReceiverNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);
    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(wifiSenderStaDevices);
    address.Assign(wifiSenderApDevice);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaReceiverInterfaces = address.Assign(csmaReceiverDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    /* application */
    // receiver
    uint16_t sinkPort = 8080;
    ApplicationContainer receiverApps;
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    receiverApps.Add(packetSinkHelper.Install(csmaReceiverNodes.Get(csmaNumber)));
    receiverApps.Start(Seconds(1.0));
    receiverApps.Stop(Seconds(seconds + 1));

    // sender
    OnOffHelper onOffSender("ns3::TcpSocketFactory", Address());
    std::string senderOnTimeString = "ns3::ConstantRandomVariable[Constant=" + senderOnTime + "]";
    std::string senderOffTimeString = "ns3::ConstantRandomVariable[Constant=" + senderOffTime + "]";
    onOffSender.SetAttribute("OnTime", StringValue(senderOnTimeString));
    onOffSender.SetAttribute("OffTime", StringValue(senderOffTimeString));
    onOffSender.SetAttribute("PacketSize", UintegerValue(senderPacketSize));
    onOffSender.SetAttribute("DataRate", StringValue(senderDataRate));
    ApplicationContainer senderApps;
    AddressValue remoteAddress(InetSocketAddress(csmaReceiverInterfaces.GetAddress(csmaNumber), sinkPort));
    onOffSender.SetAttribute("Remote", remoteAddress);
    senderApps.Add(onOffSender.Install(wifiSenderStaNodes.Get(wifiNumber - 1)));
    senderApps.Start(Seconds(2.0));
    senderApps.Stop(Seconds(seconds));
    

    /* loss */
    // receiver
    std::string receiverRanVar = "ns3::UniformRandomVariable[Min=" + receiverRanVarMin + "|Max=" + receiverRanVarMax + "]";
    Ptr<RateErrorModel> receiverEm = CreateObjectWithAttributes<RateErrorModel>("RanVar", StringValue(receiverRanVar), "ErrorRate", DoubleValue(receiverErrorRate));
    csmaReceiverDevices.Get(csmaNumber)->SetAttribute("ReceiveErrorModel", PointerValue(receiverEm));

    // inter
    std::string p2p0RanVar = "ns3::UniformRandomVariable[Min=" + interRanVarMin + "|Max=" + interRanVarMax + "]";
    Ptr<RateErrorModel> p2p0Em = CreateObjectWithAttributes<RateErrorModel>("RanVar", StringValue(p2p0RanVar), "ErrorRate", DoubleValue(interErrorRate));
    p2pDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(p2p0Em));

    std::string p2p1RanVar = "ns3::UniformRandomVariable[Min=" + interRanVarMin + "|Max=" + interRanVarMax + "]";
    Ptr<RateErrorModel> p2p1Em = CreateObjectWithAttributes<RateErrorModel>("RanVar", StringValue(p2p1RanVar), "ErrorRate", DoubleValue(interErrorRate));
    p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(p2p1Em));


    /* tracing */
    if (tracing) {
        // AsciiTraceHelper ascii;
        // p2p.EnableAsciiAll(ascii.CreateFileStream("scratch/sender_wifi_p2p_csma_receiver_tcp_p2p.tr"));
        // wifiSenderPhy.EnableAsciiAll(ascii.CreateFileStream("scratch/sender_wifi_p2p_csma_receiver_tcp_wifiSender.tr"));
        // csmaReceiver.EnableAsciiAll(ascii.CreateFileStream("scratch/sender_wifi_p2p_csma_receiver_tcp_csmaReceiver.tr"));
        // p2p.EnablePcapAll("scratch/sender_wifi_p2p_csma_receiver_tcp_p2p");
        wifiSenderPhy.EnablePcap("scratch/sender_wifi_p2p_csma_receiver_tcp", wifiSenderStaNodes.Get(wifiNumber - 1)->GetId(), 0, true);
        // csmaReceiver.EnablePcap("scratch/sender_wifi_p2p_csma_receiver_tcp", csmaReceiverNodes.Get(csmaNumber)->GetId(), 0, true);

        PcapHelper pcapHelper;
        Ptr<PcapFileWrapper> file = pcapHelper.CreateFile("scratch/sender_wifi_p2p_csma_receiver_tcp_drop.pcap", std::ios::out, PcapHelper::DLT_PPP);
        csmaReceiverDevices.Get(csmaNumber)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file, "receiver"));
        p2pDevices.Get(0)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file, "inter"));
        p2pDevices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file, "inter"));
    }


    /* simulation */
    Simulator::Stop(Seconds(seconds + 1));
    Simulator::Run();
    Simulator::Destroy();

    
    return 0;
}