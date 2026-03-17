//
// TcpProtocolHandler.cc - TCP protocol handler implementation
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

#include "ddosimu5g/apps/dynamic/protocols/TcpProtocolHandler.h"

#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include <iostream>
#include "ddosimu5g/apps/dynamic/DynamicTrafficSender.h"

namespace ddosimu5g {

// Ephemeral port counter — each TCP/HTTP handler gets a unique port.
// Starts at 10000 to avoid collisions with well-known and app-specific ports.
int TcpProtocolHandler::s_nextEphemeralPort = 10000;

TcpProtocolHandler::TcpProtocolHandler(DynamicTrafficSender* parent,
                                       std::shared_ptr<TrafficLabeler> labeler,
                                       int appId)
    : ProtocolHandler(parent, labeler, appId), requestCount(0), connected(false) {
    
    requestTimer = new omnetpp::cMessage("tcpRequestTimer");
    requestTimer->setContextPointer(this);
    
    // Initialize TCP socket
    tcpSocket.setOutputGate(parent->gate("socketOut"));
    tcpSocket.setCallback(this);
}

TcpProtocolHandler::~TcpProtocolHandler() {
    stop();
    
    tcpSocket.setCallback(nullptr);  // Clear callback before closing
    
    if (requestTimer && requestTimer->isScheduled()) {
        parent->cancelEvent(requestTimer);
    }
    delete requestTimer;
    requestTimer = nullptr;
    if (tcpSocket.isOpen()) {
        tcpSocket.close();
    }
}

void TcpProtocolHandler::start(const nlohmann::json& config) {
    EV_INFO << "TcpProtocolHandler[" << appId << "] starting..." << std::endl;
    
    // Parse configuration
    destAddress = config.value("destAddress", "server");
    destPort = config.value("destPort", 80);
    requestLength = config.value("requestLength", 512);
    numRequests = config.value("numRequests", 10);
    requestInterval = config.value("requestInterval", 1.0);
    keepAlive = config.value("keepAlive", false);
    
    double startTimeVal = config.value("startTime", 0.0);
    double duration = config.value("duration", 100.0);
    startTime = omnetpp::simTime() + startTimeVal;
    endTime = startTime + duration;
    
    currentTrafficType = config.value("trafficType", "unknown");
    currentLabel = config.value("label", "benign");
    
    isActive = true;
    requestCount = 0;
    connected = false;
    
    // --- Renew the TCP socket to avoid reuse of stale socket state ---
    // Detach old socket (clear callback, don't send messages on potentially
    // closed socket).
    tcpSocket.setCallback(nullptr);
    // Construct a fresh TcpSocket in-place via move-assign from a temporary.
    // This ensures INET's TCP layer sees a brand-new connection id.
    tcpSocket = inet::TcpSocket();
    tcpSocket.setOutputGate(parent->gate("socketOut"));
    tcpSocket.setCallback(this);
    
    // --- Use a globally unique ephemeral port ---
    localPort = s_nextEphemeralPort++;
    
    std::cout << "[TcpProtocolHandler] App[" << appId << "] starting TCP traffic to " 
              << destAddress << ":" << destPort << " (requestLen=" << requestLength 
              << ", numRequests=" << numRequests << ", interval=" << requestInterval 
              << ", keepAlive=" << (keepAlive ? "true" : "false")
              << ", duration=" << duration << ", localPort=" << localPort << ")" << std::endl;
    
    // Connect to server
    inet::L3Address destAddr = inet::L3AddressResolver().resolve(destAddress.c_str());
    tcpSocket.bind(localPort);
    tcpSocket.connect(destAddr, destPort);
}

void TcpProtocolHandler::stop() {
    if (!isActive) return;
    
    EV_INFO << "TcpProtocolHandler[" << appId << "] stopping..." << std::endl;
    
    if (requestTimer && requestTimer->isScheduled()) {
        parent->cancelEvent(requestTimer);
    }
    
    if (tcpSocket.isOpen()) {
        tcpSocket.close();
    }
    
    isActive = false;
    connected = false;
    
    std::cout << "[TcpProtocolHandler] App[" << appId << "] stopped. Sent " 
              << packetsSent << " requests, received " << packetsReceived << " responses" << std::endl;
}

void TcpProtocolHandler::handleMessage(omnetpp::cMessage* msg) {
    if (msg == requestTimer) {
        // Check if we've exceeded end time
        if (omnetpp::simTime() >= endTime) {
            EV_INFO << "TcpProtocolHandler[" << appId << "] reached end time, stopping" << std::endl;
            stop();
            return;
        }
        
        // In normal (non-keepAlive) mode, also stop after numRequests
        if (!keepAlive && requestCount >= numRequests) {
            EV_INFO << "TcpProtocolHandler[" << appId << "] reached request limit, stopping" << std::endl;
            stop();
            return;
        }
        
        if (connected) {
            sendRequest();
        }
    }
}

void TcpProtocolHandler::processSocketMessage(omnetpp::cMessage* msg) {
    //std::cout << "[TcpProtocolHandler] App[" << appId << "] processSocketMessage called with: "
    //          << msg->getName() << std::endl;

    // Let the socket process incoming messages (triggers callbacks)
    tcpSocket.processMessage(msg);
    //std::cout << "[TcpProtocolHandler] App[" << appId << "] socket.processMessage completed" << std::endl;
}

void TcpProtocolHandler::sendRequest() {
    // Create TCP request packet
    char packetName[100];
    snprintf(packetName, sizeof(packetName), "TCP-request-app%d-req%d", appId, requestCount);
    inet::Packet* packet = new inet::Packet(packetName);
    
    // Add payload
    const auto& payload = inet::makeShared<inet::ByteCountChunk>(inet::B(requestLength));
    packet->insertAtBack(payload);
    packet->addTag<inet::CreationTimeTag>()->setCreationTime(omnetpp::simTime());
    
    // Set IP options: TTL=64, DF flag=1
    packet->addTag<inet::HopLimitReq>()->setHopLimit(64);
    packet->addTag<inet::FragmentationReq>()->setDontFragment(true);
    
    // Send request
    tcpSocket.send(packet);
    
    //logPacketToCSV("TCP", packet, true);
    logPacketToCSV("TCP", destAddress, destPort, requestLength, localPort, packet->getCreationTime());

    packetsSent++;
    requestCount++;
    
    // Log to CSV
    //logPacketToCSV("TCP", destAddress, destPort, requestLength, localPort);
    
    EV_INFO << "TcpProtocolHandler[" << appId << "] sent request #" << requestCount << std::endl;
    
    // Schedule next request:
    //  - keepAlive mode: keep going until endTime (ignore numRequests)
    //  - normal mode:    stop after numRequests OR endTime, whichever first
    bool moreToSend = keepAlive
                      ? (omnetpp::simTime() < endTime)
                      : (requestCount < numRequests && omnetpp::simTime() < endTime);
    if (moreToSend) {
        scheduleRequest();
    }
}

void TcpProtocolHandler::scheduleRequest() {
    parent->scheduleAt(omnetpp::simTime() + requestInterval, requestTimer);
}

void TcpProtocolHandler::socketDataArrived(inet::TcpSocket* socket, inet::Packet* packet, bool urgent) {
    packetsReceived++;
    
    EV_INFO << "TcpProtocolHandler[" << appId << "] received response #" 
            << packetsReceived << " (" << packet->getByteLength() << " bytes)" << std::endl;
    
    //std::cout << "[TcpProtocolHandler] App[" << appId << "] RECEIVED TCP response #" << packetsReceived
    //          << " (" << packet->getByteLength() << " bytes) at t=" << omnetpp::simTime()
    //          << " - logging to CSV..." << std::endl;
    
    // Log response to CSV
    logPacketToCSV("TCP_RESPONSE", destAddress, destPort, packet->getByteLength(), localPort,packet->getCreationTime());
    //logPacketToCSV("TCP_RESPONSE", packet, false);

    //std::cout << "[TcpProtocolHandler] App[" << appId << "] CSV logging completed" << std::endl;
    
    delete packet;
}

void TcpProtocolHandler::socketAvailable(inet::TcpSocket* socket, inet::TcpAvailableInfo* availableInfo) {
    // Not used for client
    delete availableInfo;
}

void TcpProtocolHandler::socketEstablished(inet::TcpSocket* socket) {
    connected = true;
    EV_INFO << "TcpProtocolHandler[" << appId << "] connection established" << std::endl;
    
    std::cout << "[TcpProtocolHandler] App[" << appId << "] TCP connection established at t=" 
              << omnetpp::simTime() << std::endl;
    
    // Start sending requests
    scheduleRequest();
}

void TcpProtocolHandler::socketPeerClosed(inet::TcpSocket* socket) {
    EV_INFO << "TcpProtocolHandler[" << appId << "] peer closed connection" << std::endl;
    connected = false;
    
    if (socket->isOpen()) {
        socket->close();
    }
}

void TcpProtocolHandler::socketClosed(inet::TcpSocket* socket) {
    EV_INFO << "TcpProtocolHandler[" << appId << "] connection closed" << std::endl;
    connected = false;
}

void TcpProtocolHandler::socketFailure(inet::TcpSocket* socket, int code) {
    EV_WARN << "TcpProtocolHandler[" << appId << "] connection failed with code " << code << std::endl;
    connected = false;
}

void TcpProtocolHandler::socketStatusArrived(inet::TcpSocket* socket, inet::TcpStatusInfo* status) {
    delete status;
}

void TcpProtocolHandler::socketDeleted(inet::TcpSocket* socket) {
    connected = false;
}

} // namespace ddosimu5g
