//
// UdpReceiverHandler.cc - UDP receiver handler implementation
//
// Class developed by EIT, Lund University, Karim Khalil PhD
// Development assisted by AI tools (GitHub Copilot)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "ddosimu5g/apps/dynamic/protocols/receiver/UdpReceiverHandler.h"

#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include <iostream>
#include "ddosimu5g/apps/dynamic/DynamicTrafficReceiver.h"

namespace ddosimu5g {

UdpReceiverHandler::UdpReceiverHandler(DynamicTrafficReceiver* parent, int port, bool echoMode)
    : ReceiverProtocolHandler(parent, port), echoMode(echoMode), replyMultiplier(2) {
    
    // Initialize and bind UDP socket
    udpSocket.setOutputGate(parent->gate("socketOut"));
    udpSocket.bind(port);
    udpSocket.setCallback(this);
    
    EV_INFO << "UdpReceiverHandler listening on port " << port << std::endl;
    std::cout << "[UdpReceiverHandler] **CREATED** Listening on port " << port 
              << " (echoMode=" << echoMode << "), socket bound to gate" << std::endl;
}

UdpReceiverHandler::~UdpReceiverHandler() {
    std::cout << "[UdpReceiverHandler] DESTRUCTOR called for port " << port << std::endl;
    udpSocket.setCallback(nullptr);
}

bool UdpReceiverHandler::processMessage(omnetpp::cMessage* msg) {
    //std::cout << "[UdpReceiverHandler] processMessage called with " << msg->getName() << std::endl;
    if (udpSocket.belongsToSocket(msg)) {
        //std::cout << "[UdpReceiverHandler] Message belongs to this socket, processing..." << std::endl;
        udpSocket.processMessage(msg);
        return true;
    }
    std::cout << "[UdpReceiverHandler] Message does NOT belong to this socket" << std::endl;
    return false;
}

void UdpReceiverHandler::socketDataArrived(inet::UdpSocket* socket, inet::Packet* packet) {
    packetsReceived++;
    
    EV_INFO << "UdpReceiverHandler received packet (" << packet->getByteLength() 
            << " bytes) on port " << port << std::endl;
    //std::cout << "[UdpReceiverHandler] **RECEIVED** Port " << port << " got UDP packet ("
    //          << packet->getByteLength() << " bytes) at t=" << omnetpp::simTime()
    //          << " - echoMode=" << echoMode << std::endl;
    
    if (echoMode) {
        //std::cout << "[UdpReceiverHandler] Sending reply..." << std::endl;
        sendReply(packet);
        //std::cout << "[UdpReceiverHandler] Reply sent" << std::endl;
    }
    
    delete packet;
}

void UdpReceiverHandler::sendReply(inet::Packet* receivedPacket) {
    // Extract source address and port from received packet
    auto srcAddrTag = receivedPacket->getTag<inet::L3AddressInd>();
    auto srcPortTag = receivedPacket->getTag<inet::L4PortInd>();
    inet::L3Address srcAddr = srcAddrTag->getSrcAddress();
    int srcPort = srcPortTag->getSrcPort();
    
    //std::cout << "[UdpReceiverHandler] sendReply: src=" << srcAddr << ":" << srcPort << std::endl;
    
    // Create reply (typically larger than request)
    int replyLength = receivedPacket->getByteLength() * replyMultiplier;
    
    inet::Packet* reply = new inet::Packet("UdpReply");
    const auto& payload = inet::makeShared<inet::ByteCountChunk>(inet::B(replyLength));
    reply->insertAtBack(payload);
    
    // Set IP options: TTL=64, DF flag=1
    reply->addTag<inet::HopLimitReq>()->setHopLimit(64);
    reply->addTag<inet::FragmentationReq>()->setDontFragment(true);
    
    //std::cout << "[UdpReceiverHandler] Calling udpSocket.sendTo with " << replyLength
    //          << " bytes to " << srcAddr << ":" << srcPort << std::endl;
    
    // Send reply back to sender
    udpSocket.sendTo(reply, srcAddr, srcPort);
    
    //std::cout << "[UdpReceiverHandler] udpSocket.sendTo completed" << std::endl;
    
    packetsSent++;
    
    EV_INFO << "UdpReceiverHandler sent reply (" << replyLength << " bytes) to " 
            << srcAddr << ":" << srcPort << std::endl;
    //std::cout << "[UdpReceiverHandler] Port " << port << " sent UDP reply ("
    //          << replyLength << " bytes)" << std::endl;
}

void UdpReceiverHandler::socketErrorArrived(inet::UdpSocket* socket, inet::Indication* indication) {
    EV_WARN << "UdpReceiverHandler socket error on port " << port << std::endl;
    delete indication;
}

void UdpReceiverHandler::socketClosed(inet::UdpSocket* socket) {
    EV_INFO << "UdpReceiverHandler socket closed on port " << port << std::endl;
}

} // namespace ddosimu5g
