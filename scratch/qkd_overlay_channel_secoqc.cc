/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 LIPTEL.ieee.org
 *
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
 * Author: Miralem Mehic <miralem.mehic@ieee.org>

// Network topology
//                                --n4(qkd)-
//                               /          \
//                              n3           \
//                             /              \
//         n0(qkd)---n1---n2(qkd)           n6(qkd)---n7---n8(qkd)
//                            \               /
//                             \             /
//                              \--n5(qkd)--/            
//
 */
#include <fstream>
#include "ns3/core-module.h" 
#include "ns3/qkd-helper.h" 
#include "ns3/qkd-app-charging-helper.h"
#include "ns3/qkd-graph-manager.h" 
#include "ns3/applications-module.h"
#include "ns3/internet-module.h" 
#include "ns3/flow-monitor-module.h" 
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/gnuplot.h" 
#include "ns3/qkd-send.h"

#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"

#include "ns3/aodvq-module.h" 
#include "ns3/dsdvq-module.h"  

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("QKD_CHANNEL_TEST");
  

uint32_t m_bytes_total = 0; 
uint32_t m_bytes_received = 0; 
uint32_t m_bytes_sent = 0; 
uint32_t m_packets_received = 0; 
double m_time = 0;
 
void
SentPacket(std::string context, Ptr<const Packet> p){
     
    m_bytes_sent += p->GetSize();  
}

void
ReceivedPacket(std::string context, Ptr<const Packet> p, const Address& addr){
     
    m_bytes_received += p->GetSize(); 
    m_bytes_total += p->GetSize(); 
    m_packets_received++;

}

void
Ratio(uint32_t m_bytes_sent, uint32_t m_packets_sent ){
    std::cout << "Sent (bytes):\t" <<  m_bytes_sent
    << "\tReceived (bytes):\t" << m_bytes_received 
    << "\nSent (Packets):\t" <<  m_packets_sent
    << "\tReceived (Packets):\t" << m_packets_received 
    
    << "\nRatio (bytes):\t" << (float)m_bytes_received/(float)m_bytes_sent
    << "\tRatio (packets):\t" << (float)m_packets_received/(float)m_packets_sent << "\n";
}
int main (int argc, char *argv[])
{
    Packet::EnablePrinting(); 
    PacketMetadata::Enable ();
    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO ("Create nodes.");
    NodeContainer n;
    n.Create (9); 

    /*
    // Network topology
    //                                --n4(qkd)-
    //                               /          \
    //                              n3           \
    //                             /              \
    //         n0(qkd)---n1---n2(qkd)           n6(qkd)---n7---n8(qkd)
    //                            \               /
    //                             \             /
    //                              \--n5(qkd)--/            
    //
    */

    //QKD link 0-1 consists of two underlay links 0-1 and 1-2
    NodeContainer n0n1 = NodeContainer (n.Get(0), n.Get (1));
    NodeContainer n1n2 = NodeContainer (n.Get(1), n.Get (2));
    
    NodeContainer n2n3 = NodeContainer (n.Get(2), n.Get (3));
    NodeContainer n3n4 = NodeContainer (n.Get(3), n.Get (4));

    //QKD link 2-5 consists of two underlay links 2-5
    NodeContainer n2n5 = NodeContainer (n.Get(2), n.Get (5));
    NodeContainer n4n6 = NodeContainer (n.Get(4), n.Get (6));
    NodeContainer n5n6 = NodeContainer (n.Get(5), n.Get (6));
    NodeContainer n6n7 = NodeContainer (n.Get(6), n.Get (7));
    NodeContainer n7n8 = NodeContainer (n.Get(7), n.Get (8));  

    NodeContainer qkdNodes = NodeContainer ();
    qkdNodes.Add(n.Get (0));
    qkdNodes.Add(n.Get (2));
    qkdNodes.Add(n.Get (4));
    qkdNodes.Add(n.Get (5));
    qkdNodes.Add(n.Get (6));
    qkdNodes.Add(n.Get (8));

    //Underlay network - set routing protocol (if any)

    //Enable OLSR
    //OlsrHelper routingProtocol;
    //DsdvHelper routingProtocol; 

    InternetStackHelper internet;
    //internet.SetRoutingHelper (routingProtocol);
    internet.Install (n);

    // Set Mobility for all nodes
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject <ListPositionAllocator>();
    
    // You can play with locations if you need netanim graph
    positionAlloc ->Add(Vector(0, 200, 0)); // node0 
    positionAlloc ->Add(Vector(200, 200, 0)); // node1
    positionAlloc ->Add(Vector(400, 200, 0)); // node2 
    positionAlloc ->Add(Vector(600, 400, 0)); // node3 
    positionAlloc ->Add(Vector(800, 600, 0)); // node4
    positionAlloc ->Add(Vector(800, 0, 0)); // node5 
    positionAlloc ->Add(Vector(601, 200, 0)); // node6 
    positionAlloc ->Add(Vector(800, 200, 0)); // node7
    positionAlloc ->Add(Vector(1000, 200, 0)); // node8 
    
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(n);
       
    // We create the channels first without any IP addressing information
    NS_LOG_INFO ("Create channels.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    //p2p.SetChannelAttribute ("Delay", StringValue ("100ps")); 

    NetDeviceContainer d0d1 = p2p.Install (n0n1); 
    NetDeviceContainer d1d2 = p2p.Install (n1n2);
    NetDeviceContainer d2d3 = p2p.Install (n2n3);
    NetDeviceContainer d3d4 = p2p.Install (n3n4);
    NetDeviceContainer d4d6 = p2p.Install (n4n6);
    NetDeviceContainer d2d5 = p2p.Install (n2n5);
    NetDeviceContainer d5d6 = p2p.Install (n5n6);
    NetDeviceContainer d6d7 = p2p.Install (n6n7);
    NetDeviceContainer d7d8 = p2p.Install (n7n8);

    NS_LOG_INFO ("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;

    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);

    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign (d2d3);

    ipv4.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i4 = ipv4.Assign (d3d4);

    ipv4.SetBase ("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i4i6 = ipv4.Assign (d4d6);

    ipv4.SetBase ("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i5 = ipv4.Assign (d2d5);

    ipv4.SetBase ("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer i5i6 = ipv4.Assign (d5d6);

    ipv4.SetBase ("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer i6i7 = ipv4.Assign (d6d7);

    ipv4.SetBase ("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer i7i8 = ipv4.Assign (d7d8);

    // Create router nodes, initialize routing database and set up the routing
    // tables in the nodes.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
 
    //Overlay network - set routing protocol

    //Enable Overlay Routing
    AodvqHelper routingOverlayProtocol; 
    //DsdvqHelper routingOverlayProtocol; 

    //
    // Explicitly create the channels required by the topology (shown above)...
    //
    QKDHelper QHelper;  

    //install QKD Managers on the nodes
    QHelper.SetRoutingHelper (routingOverlayProtocol);
    QHelper.InstallQKDManager (qkdNodes); 
        
    //Create QKDNetDevices and create QKDbuffers
    Ipv4InterfaceAddress va02_0 (Ipv4Address ("11.0.1.1"), Ipv4Mask ("255.255.255.0"));
    Ipv4InterfaceAddress va02_2 (Ipv4Address ("11.0.1.2"), Ipv4Mask ("255.255.255.0"));

    Ipv4InterfaceAddress va24_2 (Ipv4Address ("11.0.2.1"), Ipv4Mask ("255.255.255.0"));
    Ipv4InterfaceAddress va24_4 (Ipv4Address ("11.0.2.2"), Ipv4Mask ("255.255.255.0"));

    Ipv4InterfaceAddress va25_2 (Ipv4Address ("11.0.3.1"), Ipv4Mask ("255.255.255.0"));
    Ipv4InterfaceAddress va25_5 (Ipv4Address ("11.0.3.2"), Ipv4Mask ("255.255.255.0"));

    Ipv4InterfaceAddress va46_4 (Ipv4Address ("11.0.4.1"), Ipv4Mask ("255.255.255.0"));
    Ipv4InterfaceAddress va46_6 (Ipv4Address ("11.0.4.2"), Ipv4Mask ("255.255.255.0"));

    Ipv4InterfaceAddress va56_5 (Ipv4Address ("11.0.5.1"), Ipv4Mask ("255.255.255.0"));
    Ipv4InterfaceAddress va56_6 (Ipv4Address ("11.0.5.2"), Ipv4Mask ("255.255.255.0"));

    Ipv4InterfaceAddress va68_6 (Ipv4Address ("11.0.6.1"), Ipv4Mask ("255.255.255.0"));
    Ipv4InterfaceAddress va68_8 (Ipv4Address ("11.0.6.2"), Ipv4Mask ("255.255.255.0"));
    
    //create QKD connection between nodes 0 and 2
    NetDeviceContainer qkdNetDevices02 = QHelper.InstallOverlayQKD (
        d0d1.Get(0), d1d2.Get(1), 
        va02_0, va02_2,  
        108576,     //min
        208576,     //thr - will be set automatically
        1085760,    //max
        208576     //current    //20485770
    );
    //Create graph to monitor buffer changes
    QHelper.AddGraph(n.Get(0), d1d2.Get (0), "myGraph02"); //srcNode, destinationAddress, BufferTitle

    //create QKD connection between nodes 2 and 4
    NetDeviceContainer qkdNetDevices24 = QHelper.InstallOverlayQKD (
        d2d3.Get(0), d3d4.Get(1), 
        va24_2, va24_4,  
        108576,     //min
        208576,     //thr - will be set automatically
        1085760,    //max
        208576     //current    //88576
    );
    //Create graph to monitor buffer changes
    QHelper.AddGraph(n.Get(2), d3d4.Get (0), "myGraph24"); //srcNode, destinationAddress, BufferTitle
 
    //create QKD connection between nodes 2 and 5
    NetDeviceContainer qkdNetDevices25 = QHelper.InstallOverlayQKD (
        d2d5.Get(0), d2d5.Get(1), 
        va25_2, va25_5,  
        108576,     //min
        208576,     //thr - will be set automatically
        1085760,    //max
        208576     //current    //88576
    );
    //Create graph to monitor buffer changes
    QHelper.AddGraph(n.Get(2), d2d5.Get (0), "myGraph25"); //srcNode, destinationAddress, BufferTitle
    
    //create QKD connection between nodes 4 and 6
    NetDeviceContainer qkdNetDevices46 = QHelper.InstallOverlayQKD (
        d4d6.Get(0), d4d6.Get(1), 
        va46_4, va46_6,  
        108576,     //min
        208576,     //thr - will be set automatically
        1085760,    //max
        208576     //current    //88576
    );
    //Create graph to monitor buffer changes
    QHelper.AddGraph(n.Get(4), d4d6.Get (0), "myGraph46"); //srcNode, destinationAddress, BufferTitle
 
    //create QKD connection between nodes 5 and 6
    NetDeviceContainer qkdNetDevices56 = QHelper.InstallOverlayQKD (
        d5d6.Get(0), d5d6.Get(1), 
        va56_5, va56_6,  
        108576,     //min
        208576,     //thr - will be set automatically
        1085760,    //max
        208576     //current    //88576
    );
    //Create graph to monitor buffer changes
    QHelper.AddGraph(n.Get(5), d5d6.Get (0), "myGraph56"); //srcNode, destinationAddress, BufferTitle
 
    //create QKD connection between nodes 6 and 8
    NetDeviceContainer qkdNetDevices68 = QHelper.InstallOverlayQKD (
        d6d7.Get(0), d7d8.Get(1), 
        va68_6, va68_8,  
        108576,     //min
        208576,     //thr - will be set automatically
        1085760,    //max
        208576     //current    //88576
    );
    //Create graph to monitor buffer changes
    QHelper.AddGraph(n.Get(6), d6d7.Get (0), "myGraph68"); //srcNode, destinationAddress, BufferTitle
 

    NS_LOG_INFO ("Create Applications."); 

    /* QKD APPs for charing */
    //Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (2536)); 
    
    QKDAppChargingHelper qkdChargingApp02("ns3::VirtualTcpSocketFactory", va02_0.GetLocal (), va02_2.GetLocal (), 200000);
    ApplicationContainer qkdChrgApps02 = qkdChargingApp02.Install ( qkdNetDevices02.Get (0), qkdNetDevices02.Get (1) );
    qkdChrgApps02.Start (Seconds (10.));
    qkdChrgApps02.Stop (Seconds (120.));
    
    QKDAppChargingHelper qkdChargingApp24("ns3::VirtualTcpSocketFactory", va24_2.GetLocal (), va24_4.GetLocal (), 200000);
    ApplicationContainer qkdChrgApps24 = qkdChargingApp24.Install ( qkdNetDevices24.Get (0), qkdNetDevices24.Get (1) );
    qkdChrgApps24.Start (Seconds (10.));
    qkdChrgApps24.Stop (Seconds (120.));
    
    QKDAppChargingHelper qkdChargingApp25("ns3::VirtualTcpSocketFactory", va25_2.GetLocal (), va25_5.GetLocal (), 200000);
    ApplicationContainer qkdChrgApps25 = qkdChargingApp25.Install ( qkdNetDevices25.Get (0), qkdNetDevices25.Get (1) );
    qkdChrgApps25.Start (Seconds (10.));
    qkdChrgApps25.Stop (Seconds (120.));

    QKDAppChargingHelper qkdChargingApp46("ns3::VirtualTcpSocketFactory", va46_4.GetLocal (), va46_6.GetLocal (), 200000);
    ApplicationContainer qkdChrgApps46 = qkdChargingApp46.Install ( qkdNetDevices46.Get (0), qkdNetDevices46.Get (1) );
    qkdChrgApps46.Start (Seconds (10.));
    qkdChrgApps46.Stop (Seconds (120.));

    QKDAppChargingHelper qkdChargingApp56("ns3::VirtualTcpSocketFactory", va56_5.GetLocal (), va56_6.GetLocal (), 200000);
    ApplicationContainer qkdChrgApps56 = qkdChargingApp56.Install ( qkdNetDevices56.Get (0), qkdNetDevices56.Get (1) );
    qkdChrgApps56.Start (Seconds (10.));
    qkdChrgApps56.Stop (Seconds (120.));

    QKDAppChargingHelper qkdChargingApp68("ns3::VirtualTcpSocketFactory", va68_6.GetLocal (), va68_8.GetLocal (), 200000);
    ApplicationContainer qkdChrgApps68 = qkdChargingApp68.Install ( qkdNetDevices68.Get (0), qkdNetDevices68.Get (1) );
    qkdChrgApps68.Start (Seconds (10.));
    qkdChrgApps68.Stop (Seconds (120.));
    


    /* Create user's traffic between v0 and v1 */
    /* Create sink app  */
    uint16_t sinkPort = 8080;
    QKDSinkAppHelper packetSinkHelper ("ns3::VirtualUdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install (n.Get (8));
    sinkApps.Start (Seconds (55.));
    sinkApps.Stop (Seconds (170.));
    
    //Create source app  
    Address sinkAddress (InetSocketAddress (va68_8.GetLocal (), sinkPort));
    Address sourceAddress (InetSocketAddress (va02_0.GetLocal (), sinkPort));
    Ptr<Socket> overlaySocket = Socket::CreateSocket (n.Get (0), VirtualUdpSocketFactory::GetTypeId ());
 
    Ptr<QKDSend> app = CreateObject<QKDSend> ();
    app->Setup (overlaySocket, sourceAddress, sinkAddress, 1040, 0, DataRate ("50kbps"));
    n.Get (0)->AddApplication (app);
    app->SetStartTime (Seconds (55.));
    app->SetStopTime (Seconds (170.)); 
    
    //////////////////////////////////////
    ////         STATISTICS
    //////////////////////////////////////

    //if we need we can create pcap files
    p2p.EnablePcapAll ("QKD_vnet_test"); 
    QHelper.EnablePcapAll ("QKD_overlay_vnet_test");

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::QKDSend/Tx", MakeCallback(&SentPacket));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::QKDSink/Rx", MakeCallback(&ReceivedPacket));
 
    Simulator::Stop ( Seconds (100) );
    Simulator::Run ();

   Ratio(app->sendDataStats(), app->sendPacketStats());

    //Finally print the graphs
    QHelper.PrintGraphs();
    Simulator::Destroy ();
}
