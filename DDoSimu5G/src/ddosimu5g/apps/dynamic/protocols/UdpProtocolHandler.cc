//
// UdpProtocolHandler.cc - UDP protocol handler implementation
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

#include "ddosimu5g/apps/dynamic/protocols/UdpProtocolHandler.h"

#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include <iostream>
#include "ddosimu5g/apps/dynamic/DynamicTrafficSender.h"

namespace ddosimu5g {

UdpProtocolHandler::UdpProtocolHandler(DynamicTrafficSender* parent,
                                       std::shared_ptr<TrafficLabeler> labeler,
                                       int appId)
    : ProtocolHandler(parent, labeler, appId), burstCount(0) {
    
    sendTimer = new omnetpp::cMessage("udpSendTimer");
    sendTimer->setContextPointer(this);  // Store handler pointer for routing
    
    // Initialize and bind UDP socket
    udpSocket.setOutputGate(parent->gate("socketOut"));
    udpSocket.setCallback(this);
    localPort = 1000 + parent->getId() * 10 + appId;
    udpSocket.bind(localPort);
}

UdpProtocolHandler::~UdpProtocolHandler() {
    std::cout << "[UdpProtocolHandler] App[" << appId << "] DESTRUCTOR called" << std::endl;
    
    // Clear callback BEFORE stopping to prevent any callbacks during cleanup
    std::cout << "[UdpProtocolHandler] App[" << appId << "] clearing socket callback..." << std::endl;
    udpSocket.setCallback(nullptr);
    
    stop();
    
    if (sendTimer && sendTimer->isScheduled()) {
        std::cout << "[UdpProtocolHandler] App[" << appId << "] canceling scheduled timer..." << std::endl;
        parent->cancelEvent(sendTimer);
    }
    if (sendTimer) {
        std::cout << "[UdpProtocolHandler] App[" << appId << "] deleting timer..." << std::endl;
        delete sendTimer;
        sendTimer = nullptr;
    }
    // UdpSocket auto-closes on destruction, no explicit close() needed
    std::cout << "[UdpProtocolHandler] App[" << appId << "] DESTRUCTOR completed" << std::endl;
}

void UdpProtocolHandler::start(const nlohmann::json& config) {
    EV_INFO << "UdpProtocolHandler[" << appId << "] starting..." << std::endl;
    
    // Parse configuration
    destAddress = config.value("destAddress", "server");
    destPort = config.value("destPort", 5000);
    messageLength = config.value("messageLength", 512);
    sendInterval = config.value("sendInterval", 1.0);
    burstSize = config.value("burstSize", 1);
    
    double startTimeVal = config.value("startTime", 0.0);
    double duration = config.value("duration", 100.0);
    startTime = omnetpp::simTime() + startTimeVal;
    endTime = startTime + duration;
    
    currentTrafficType = config.value("trafficType", "unknown");
    currentLabel = config.value("label", "benign");
    
    isActive = true;
    burstCount = 0;
    
    std::cout << "[UdpProtocolHandler] App[" << appId << "] starting UDP traffic to " 
              << destAddress << ":" << destPort << " (length=" << messageLength 
              << ", interval=" << sendInterval << ", burst=" << burstSize
              << ", duration=" << duration << ")" << std::endl;
    
    // Schedule first send with random jitter
    double delay = omnetpp::uniform(parent->getRNG(0), 0.0, sendInterval);
    parent->scheduleAt(omnetpp::simTime() + delay, sendTimer);
}

void UdpProtocolHandler::stop() {
    if (!isActive) return;
    
    EV_INFO << "UdpProtocolHandler[" << appId << "] stopping..." << std::endl;
    
    if (sendTimer && sendTimer->isScheduled()) {
        parent->cancelEvent(sendTimer);
    }
    
    isActive = false;
    
    std::cout << "[UdpProtocolHandler] App[" << appId << "] stopped. Sent " 
              << packetsSent << " packets" << std::endl;
}

void UdpProtocolHandler::handleMessage(omnetpp::cMessage* msg) {
    if (msg == sendTimer) {
        // Check if we've exceeded end time
        if (omnetpp::simTime() >= endTime) {
            EV_INFO << "UdpProtocolHandler[" << appId << "] reached endTime, stopping" << std::endl;
            stop();
            return;
        }
        
        sendPacket();
        scheduleSend();
    }
}

void UdpProtocolHandler::processSocketMessage(omnetpp::cMessage* msg) {
    // Let the socket process incoming messages (triggers callbacks)
    udpSocket.processMessage(msg);
}

void UdpProtocolHandler::sendPacket() {
    // Create packet
    char packetName[100];
    snprintf(packetName, sizeof(packetName), "UDP-app%d-pkt%d", appId, packetsSent);
    inet::Packet* packet = new inet::Packet(packetName);
    
    // Add payload
    const auto& payload = inet::makeShared<inet::ByteCountChunk>(inet::B(messageLength));
    packet->insertAtBack(payload);
    packet->addTag<inet::CreationTimeTag>()->setCreationTime(omnetpp::simTime());
    
    // Set IP options: TTL=64, DF flag=1
    packet->addTag<inet::HopLimitReq>()->setHopLimit(64);
    packet->addTag<inet::FragmentationReq>()->setDontFragment(true);
    
    // Send packet
    inet::L3Address destAddr = inet::L3AddressResolver().resolve(destAddress.c_str());

    udpSocket.sendTo(packet, destAddr, destPort);
    
    //logPacketToCSV("UDP", packet, true);
    logPacketToCSV("UDP", destAddress, destPort, messageLength, localPort, packet->getCreationTime());

    packetsSent++;
    
    // Log to CSV
    //logPacketToCSV("UDP", destAddress, destPort, messageLength, localPort);
    
    
    EV_INFO << "UdpProtocolHandler[" << appId << "] sent packet #" << packetsSent << std::endl;
}

void UdpProtocolHandler::scheduleSend() {
    burstCount++;
    
    if (burstCount >= burstSize) {
        // End of burst - schedule next burst
        burstCount = 0;
        parent->scheduleAt(omnetpp::simTime() + sendInterval, sendTimer);
    } else {
        // Continue burst with minimal delay
        parent->scheduleAt(omnetpp::simTime() + 0.001, sendTimer);
    }
}

void UdpProtocolHandler::socketDataArrived(inet::UdpSocket* socket, inet::Packet* packet) {
    packetsReceived++;
    
    EV_INFO << "UdpProtocolHandler[" << appId << "] received packet #" 
            << packetsReceived << " (size=" << packet->getByteLength() << ")" << std::endl;
    
    //std::cout << "[UdpProtocolHandler] App[" << appId << "] RECEIVED packet #" << packetsReceived
    //          << " (size=" << packet->getByteLength() << ") - logging to CSV..." << std::endl;
    
    // Log reply to CSV
    logPacketToCSV("UDP_REPLY", destAddress, destPort, packet->getByteLength(),localPort, packet->getCreationTime() );
    //logPacketToCSV("UDP_REPLY", packet, false);

    //std::cout << "[UdpProtocolHandler] App[" << appId << "] CSV logging completed" << std::endl;
    
    delete packet;
}

void UdpProtocolHandler::socketErrorArrived(inet::UdpSocket* socket, inet::Indication* indication) {
    EV_WARN << "UdpProtocolHandler[" << appId << "] socket error" << std::endl;
    delete indication;
}

void UdpProtocolHandler::socketClosed(inet::UdpSocket* socket) {
    EV_INFO << "UdpProtocolHandler[" << appId << "] socket closed" << std::endl;
}

} // namespace ddosimu5g
