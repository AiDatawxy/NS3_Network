#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/random-variable-stream-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("sender_p2p_receiver_tcp");

// ======================================================
//
//  sender          receiver
//  n0      ---     n1
//          p2p
//          10.1.1.x
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

    std::string senderOnTime = "1.0";
    std::string senderOffTime = "1.0";
    uint32_t senderPacketSize = 1024;
    std::string senderDataRate = "1Mbps";

    std::string receiverRanVarMin = "0.8";
    std::string receiverRanVarMax = "1.0";
    double receiverErrorRate = 0.001;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable tracing", tracing);
    cmd.AddValue("seconds", "Simulation duration seconds", seconds);
    cmd.AddValue("p2pDataRate", "Point to point DataRate", p2pDataRate);
    cmd.AddValue("p2pDelay", "Point to point Delay", p2pDelay);
    cmd.AddValue("senderOnTime", "Sender OnOffTime OnTime", senderOnTime);
    cmd.AddValue("senderOffTime", "Sender OnOffTime OffTime", senderOffTime);
    cmd.AddValue("senderPacketSize", "Send packet size", senderPacketSize);
    cmd.AddValue("senderDataRate", "Send DataRate", senderDataRate);
    cmd.AddValue("receiverRanVarMin", "Receiver RanVar Min", receiverRanVarMin);
    cmd.AddValue("receiverRanVarMax", "Receiver RanVar Max", receiverRanVarMax);
    cmd.AddValue("receiverErrorRate", "Rate in receiver RateErrorModel", receiverErrorRate);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    if (verbose == "all") {
        LogComponentEnable("sender_p2p_receiver_tcp", LOG_LEVEL_ALL);
        LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    }
    else if (verbose == "info") {
        LogComponentEnable("sender_p2p_receiver_tcp", LOG_LEVEL_INFO);
        LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }


    /* nodes topology */
    NS_LOG_INFO("Creating Topology");
    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(p2pDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(p2pDelay));

    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(p2pNodes);


    /* network protocol stack */
    InternetStackHelper stack;
    stack.Install(p2pNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);


    /* application */
    // receiver
    uint16_t sinkPort = 8080;
    ApplicationContainer receiverApps;
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    receiverApps.Add(packetSinkHelper.Install(p2pNodes.Get(1)));
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
    AddressValue remoteAddress(InetSocketAddress(p2pInterfaces.GetAddress(1), sinkPort));
    onOffSender.SetAttribute("Remote", remoteAddress);
    senderApps.Add(onOffSender.Install(p2pNodes.Get(0)));
    senderApps.Start(Seconds(2.0));
    senderApps.Stop(Seconds(seconds));


    /* loss */
    // receiver
    std::string receiverRanVar = "ns3::UniformRandomVariable[Min=" + receiverRanVarMin + "|Max=" + receiverRanVarMax + "]";
    Ptr<RateErrorModel> receiverEm = CreateObjectWithAttributes<RateErrorModel>("RanVar", StringValue(receiverRanVar), "ErrorRate", DoubleValue(receiverErrorRate));
    p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(receiverEm));


    /* tracing */
    if (tracing) {
        // AsciiTraceHelper ascii;
        // p2p.EnableAsciiAll(ascii.CreateFileStream("scratch/sender_p2p_receiver_tcp_p2p.tr"));
        // p2p.EnablePcapAll("scratch/sender_p2p_receiver_tcp_p2p");
        p2p.EnablePcap("scratch/sender_p2p_receiver_tcp", p2pNodes.Get(0)->GetId(), 0, true);

        PcapHelper pcapHelper;
        Ptr<PcapFileWrapper> file = pcapHelper.CreateFile("scratch/sender_p2p_receiver_tcp_drop.pcap", std::ios::out, PcapHelper::DLT_PPP);
        p2pDevices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file, "receiver"));
    }


    /* simulation */
    Simulator::Stop(Seconds(seconds + 1));
    Simulator::Run();
    Simulator::Destroy();

    
    return 0;
}