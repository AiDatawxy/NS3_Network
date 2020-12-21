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

NS_LOG_COMPONENT_DEFINE("sender_p2p_receiver_udp");

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


/* loss callback print */
static void RxDropPrint(const std::string & dropType, Ptr<const Packet> p) {
    if (dropType == "receiver") {
        NS_LOG_UNCOND("ReceiverRxDrop at " << Simulator::Now().GetSeconds());
    }
    else if (dropType == "inter")
        NS_LOG_UNCOND("InterRxDrop at " << Simulator::Now().GetSeconds());
}


int main(int argc, char *argv[]) {

    std::string verbose = "all";
    bool tracing = false;
    double seconds = 10.0;

    std::string p2pDataRate = "5Mbps";
    std::string p2pDelay = "50ms";

    double senderInterval = 1.0;
    uint32_t senderPacketSize = 1024;

    std::string receiverRanVarMin = "0.8";
    std::string receiverRanVarMax = "1.0";
    double receiverErrorRate = 0.001;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable tracing", tracing);
    cmd.AddValue("seconds", "Simulation duration seconds", seconds);
    cmd.AddValue("p2pDataRate", "Point to point DataRate", p2pDataRate);
    cmd.AddValue("p2pDelay", "Point to point Delay", p2pDelay);
    cmd.AddValue("senderInterval", "Send interval", senderInterval);
    cmd.AddValue("senderPacketSize", "Send packet size", senderPacketSize);
    cmd.AddValue("receiverRanVarMin", "Receiver RanVar Min", receiverRanVarMin);
    cmd.AddValue("receiverRanVarMax", "Receiver RanVar Max", receiverRanVarMax);
    cmd.AddValue("receiverErrorRate", "Rate in receiver RateErrorModel", receiverErrorRate);
    cmd.Parse(argc, argv);
    uint32_t senderMaxPackets = int(seconds) - 1;

    Time::SetResolution(Time::NS);

    if (verbose == "all") {
        LogComponentEnable("sender_p2p_receiver_udp", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
    }
    else if (verbose == "info") {
        LogComponentEnable("sender_p2p_receiver_udp", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
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
    UdpEchoServerHelper echoReceiver(9);
    ApplicationContainer receiverApps = echoReceiver.Install(p2pNodes.Get(1));
    receiverApps.Start(Seconds(1.0));
    receiverApps.Stop(Seconds(seconds + 1));

    // sender
    UdpEchoClientHelper echoSender(p2pInterfaces.GetAddress(1), 9);
    echoSender.SetAttribute("MaxPackets", UintegerValue(senderMaxPackets));
    echoSender.SetAttribute("Interval", TimeValue(Seconds(senderInterval)));
    echoSender.SetAttribute("PacketSize", UintegerValue(senderPacketSize));
    ApplicationContainer senderApps = echoSender.Install(p2pNodes.Get(0));
    senderApps.Start(Seconds(2.0));
    senderApps.Stop(Seconds(seconds));


    /* loss */
    // receiver
    std::string receiverRanVar = "ns3::UniformRandomVariable[Min=" + receiverRanVarMin + "|Max=" + receiverRanVarMax + "]";
    Ptr<RateErrorModel> receiverEm = CreateObjectWithAttributes<RateErrorModel>("RanVar", StringValue(receiverRanVar), "ErrorRate", DoubleValue(receiverErrorRate));
    p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(receiverEm));


    /* tracing */
    if (tracing) {
        AsciiTraceHelper ascii;
        p2p.EnableAsciiAll(ascii.CreateFileStream("scratch/sender_p2p_receiver_udp_p2p.tr"));
        p2p.EnablePcapAll("scratch/sender_p2p_receiver_udp_p2p");

        PcapHelper pcapHelper;
        Ptr<PcapFileWrapper> file = pcapHelper.CreateFile("scratch/sender_p2p_receiver_udp_drop.pcap", std::ios::out, PcapHelper::DLT_PPP);
        p2pDevices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file, "receiver"));
    }
    else {
        p2pDevices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDropPrint, "receiver"));
    }


    /* simulation */
    Simulator::Stop(Seconds(seconds + 1));
    Simulator::Run();
    Simulator::Destroy();

    
    return 0;
}