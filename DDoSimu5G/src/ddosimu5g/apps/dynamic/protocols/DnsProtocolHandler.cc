//
// DnsProtocolHandler.cc - DNS protocol handler implementation
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

#include "ddosimu5g/apps/dynamic/protocols/DnsProtocolHandler.h"

#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include <iostream>
#include <arpa/inet.h>  // For htons
#include "ddosimu5g/apps/dynamic/DynamicTrafficSender.h"

namespace ddosimu5g {

// DNS Header Structure (12 bytes)
struct DnsHeader {
    uint16_t id;          // Transaction ID
    uint16_t flags;       // Flags
    uint16_t qdcount;     // Number of questions
    uint16_t ancount;     // Number of answers
    uint16_t nscount;     // Number of authority RRs
    uint16_t arcount;     // Number of additional RRs
};

// DNS Flags
#define DNS_QR_QUERY    0x0000  // Query
#define DNS_QR_RESPONSE 0x8000  // Response
#define DNS_OPCODE_QUERY 0x0000 // Standard query
#define DNS_RD          0x0100  // Recursion desired

static uint16_t dnsTransactionId = 1;

DnsProtocolHandler::DnsProtocolHandler(DynamicTrafficSender* parent,
                                       std::shared_ptr<TrafficLabeler> labeler,
                                       int appId)
    : ProtocolHandler(parent, labeler, appId), waitingForReply(false) {
    
    queryTimer = new omnetpp::cMessage("dnsQueryTimer");
    queryTimer->setContextPointer(this);
    
    // Initialize and bind UDP socket
    udpSocket.setOutputGate(parent->gate("socketOut"));
    udpSocket.setCallback(this);
    localPort = 2000 + parent->getId() * 10 + appId;
    udpSocket.bind(localPort);
}

DnsProtocolHandler::~DnsProtocolHandler() {
    // Clear callback BEFORE stopping to prevent any callbacks during cleanup
    udpSocket.setCallback(nullptr);
    
    stop();
    
    if (queryTimer && queryTimer->isScheduled()) {
        parent->cancelEvent(queryTimer);
    }
    delete queryTimer;
    queryTimer = nullptr;
    // UdpSocket auto-closes on destruction, no explicit close() needed
}

void DnsProtocolHandler::start(const nlohmann::json& config) {
    EV_INFO << "DnsProtocolHandler[" << appId << "] starting..." << std::endl;
    
    // Parse configuration
    destAddress = config.value("destAddress", "dnsServer");
    destPort = config.value("destPort", 53);
    queryLength = config.value("queryLength", 64);
    responseLength = config.value("responseLength", 128);
    queryInterval = config.value("queryInterval", 5.0);
    
    double startTimeVal = config.value("startTime", 0.0);
    double duration = config.value("duration", 100.0);
    startTime = omnetpp::simTime() + startTimeVal;
    endTime = startTime + duration;
    
    currentTrafficType = config.value("trafficType", "unknown");
    currentLabel = config.value("label", "benign");
    
    isActive = true;
    waitingForReply = false;
    
    std::cout << "[DnsProtocolHandler] App[" << appId << "] starting DNS traffic to " 
              << destAddress << ":" << destPort << " (queryLen=" << queryLength 
              << ", interval=" << queryInterval << ", duration=" << duration << ")" << std::endl;
    
    // Schedule first query
    parent->scheduleAt(omnetpp::simTime() + omnetpp::uniform(parent->getRNG(0), 0.0, 0.1), queryTimer);
}

void DnsProtocolHandler::stop() {
    if (!isActive) return;
    
    EV_INFO << "DnsProtocolHandler[" << appId << "] stopping..." << std::endl;
    
    if (queryTimer && queryTimer->isScheduled()) {
        parent->cancelEvent(queryTimer);
    }
    
    isActive = false;
    waitingForReply = false;
    
    std::cout << "[DnsProtocolHandler] App[" << appId << "] stopped. Sent " 
              << packetsSent << " queries, received " << packetsReceived << " responses" << std::endl;
}

void DnsProtocolHandler::handleMessage(omnetpp::cMessage* msg) {
    if (msg == queryTimer) {
        // Check if we've exceeded end time
        if (omnetpp::simTime() >= endTime) {
            EV_INFO << "DnsProtocolHandler[" << appId << "] reached endTime, stopping" << std::endl;
            stop();
            return;
        }
        
        if (!waitingForReply) {
            sendQuery();
        }
    }
}

void DnsProtocolHandler::processSocketMessage(omnetpp::cMessage* msg) {
    // Let the socket process incoming messages (triggers callbacks)
    udpSocket.processMessage(msg);
}

void DnsProtocolHandler::sendQuery() {
    // Create DNS query packet
    char packetName[100];
    snprintf(packetName, sizeof(packetName), "DNS-query-app%d-pkt%d", appId, packetsSent);
    inet::Packet* packet = new inet::Packet(packetName);
    
    // Build proper DNS query message
    std::vector<uint8_t> dnsData;
    
    // DNS Header (12 bytes)
    DnsHeader header;
    header.id = htons(dnsTransactionId++);
    header.flags = htons(DNS_QR_QUERY | DNS_OPCODE_QUERY | DNS_RD);
    header.qdcount = htons(1);  // 1 question
    header.ancount = htons(0);  // 0 answers
    header.nscount = htons(0);  // 0 authority RRs
    header.arcount = htons(0);  // 0 additional RRs
    
    // Add header to data
    uint8_t* headerBytes = reinterpret_cast<uint8_t*>(&header);
    dnsData.insert(dnsData.end(), headerBytes, headerBytes + sizeof(DnsHeader));
    
    // DNS Question: example.com A record query
    // QNAME: length-prefixed labels
    dnsData.push_back(0x07);  // Length of "example"
    dnsData.push_back('e');
    dnsData.push_back('x');
    dnsData.push_back('a');
    dnsData.push_back('m');
    dnsData.push_back('p');
    dnsData.push_back('l');
    dnsData.push_back('e');
    dnsData.push_back(0x03);  // Length of "com"
    dnsData.push_back('c');
    dnsData.push_back('o');
    dnsData.push_back('m');
    dnsData.push_back(0x00);  // End of name
    
    // QTYPE: A (1)
    dnsData.push_back(0x00);
    dnsData.push_back(0x01);
    
    // QCLASS: IN (1)
    dnsData.push_back(0x00);
    dnsData.push_back(0x01);
    
    // Pad to desired query length if needed
    while (dnsData.size() < (size_t)queryLength) {
        dnsData.push_back(0x00);
    }
    
    // Create chunk with DNS data
    const auto& payload = inet::makeShared<inet::BytesChunk>(dnsData);
    packet->insertAtBack(payload);
    packet->addTag<inet::CreationTimeTag>()->setCreationTime(omnetpp::simTime());
    
    // Set IP options: TTL=64, DF flag=1
    packet->addTag<inet::HopLimitReq>()->setHopLimit(64);
    packet->addTag<inet::FragmentationReq>()->setDontFragment(true);
    
    // Send query
    inet::L3Address destAddr = inet::L3AddressResolver().resolve(destAddress.c_str());
    udpSocket.sendTo(packet, destAddr, destPort);
    
    //logPacketToCSV("UDP", packet, true);
    logPacketToCSV("DNS", destAddress, destPort, queryLength, localPort, packet->getCreationTime());

    packetsSent++;
    waitingForReply = true;
    
    // Log to CSV
    //logPacketToCSV("DNS", destAddress, destPort, queryLength, localPort);
    
    EV_INFO << "DnsProtocolHandler[" << appId << "] sent query #" << packetsSent << std::endl;
    
    // Don't schedule next query here - will be scheduled when response arrives
}

void DnsProtocolHandler::scheduleQuery() {
    if (!queryTimer->isScheduled()) {
        parent->scheduleAt(omnetpp::simTime() + queryInterval, queryTimer);
    }
}

void DnsProtocolHandler::socketDataArrived(inet::UdpSocket* socket, inet::Packet* packet) {
    packetsReceived++;
    waitingForReply = false;
    
    EV_INFO << "DnsProtocolHandler[" << appId << "] received response #" 
            << packetsReceived << std::endl;
    
    // Log response to CSV
    logPacketToCSV("DNS_RESPONSE", destAddress, destPort, packet->getByteLength(),localPort, packet->getCreationTime());
    //logPacketToCSV("DNS_RESPONSE", packet, false);
    
    delete packet;
    
    // Schedule next query if still within duration
    if (isActive && omnetpp::simTime() < endTime) {
        scheduleQuery();
    }
}

void DnsProtocolHandler::socketErrorArrived(inet::UdpSocket* socket, inet::Indication* indication) {
    EV_WARN << "DnsProtocolHandler[" << appId << "] socket error" << std::endl;
    delete indication;
}

void DnsProtocolHandler::socketClosed(inet::UdpSocket* socket) {
    EV_INFO << "DnsProtocolHandler[" << appId << "] socket closed" << std::endl;
}

} // namespace ddosimu5g
