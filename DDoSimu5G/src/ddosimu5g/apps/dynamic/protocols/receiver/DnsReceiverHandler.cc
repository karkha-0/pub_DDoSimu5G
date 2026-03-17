//
// DnsReceiverHandler.cc - DNS receiver handler implementation
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

#include "DnsReceiverHandler.h"

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
#define DNS_QR_RESPONSE 0x8000  // Response
#define DNS_OPCODE_QUERY 0x0000 // Standard query
#define DNS_AA          0x0400  // Authoritative answer
#define DNS_RD          0x0100  // Recursion desired
#define DNS_RA          0x0080  // Recursion available

DnsReceiverHandler::DnsReceiverHandler(DynamicTrafficReceiver* parent, int port)
    : ReceiverProtocolHandler(parent, port) {
    
    // Initialize and bind DNS socket
    dnsSocket.setOutputGate(parent->gate("socketOut"));
    dnsSocket.bind(port);
    dnsSocket.setCallback(this);
    
    EV_INFO << "DnsReceiverHandler listening on port " << port << std::endl;
    std::cout << "[DnsReceiverHandler] Listening on port " << port << std::endl;
}

DnsReceiverHandler::~DnsReceiverHandler() {
    dnsSocket.setCallback(nullptr);
}

bool DnsReceiverHandler::processMessage(omnetpp::cMessage* msg) {
    if (dnsSocket.belongsToSocket(msg)) {
        dnsSocket.processMessage(msg);
        return true;
    }
    return false;
}

void DnsReceiverHandler::socketDataArrived(inet::UdpSocket* socket, inet::Packet* packet) {
    packetsReceived++;

    auto srcAddrTag = packet->getTag<inet::L3AddressInd>();
    inet::L3Address srcAddr = srcAddrTag->getSrcAddress();
    
    // EV_INFO << "DnsReceiverHandler received query (" << packet->getByteLength() 
    //         << " bytes) on port " << port << std::endl;
    // std::cout << "[DnsReceiverHandler] Port " << port << " received DNS query (" 
    //           << packet->getByteLength() << " bytes) at t=" << omnetpp::simTime() << std::endl;

    if (AttackTracker::getInstance()->isDnsAmplificationBot(srcAddr)) {
        // std::cout << "\n DNS AMPLIFICATION ATTACK DETECTED (from registered bot)" << std::endl;
        // std::cout << "  - Bot: " << srcAddr << std::endl;
        // std::cout << "  - Response suppressed" << std::endl;
        
        delete packet;
        return;
    }

    sendDnsResponse(packet);
    
    delete packet;
}

void DnsReceiverHandler::sendDnsResponse(inet::Packet* query) {
    // Extract source address and port from query
    auto srcAddrTag = query->getTag<inet::L3AddressInd>();
    auto srcPortTag = query->getTag<inet::L4PortInd>();
    inet::L3Address srcAddr = srcAddrTag->getSrcAddress();
    int srcPort = srcPortTag->getSrcPort();
    
    // Parse query to extract transaction ID
    auto queryChunk = query->peekDataAt(inet::B(0), inet::B(12));  // Read first 12 bytes (DNS header)
    std::vector<uint8_t> queryBytes;
    auto bytesChunk = inet::dynamicPtrCast<const inet::BytesChunk>(queryChunk);
    if (bytesChunk) {
        queryBytes = bytesChunk->getBytes();
    }
    
    uint16_t queryId = 0;
    if (queryBytes.size() >= 2) {
        queryId = (queryBytes[0] << 8) | queryBytes[1];
    }
    
    // PARSE QUERY TYPE from DNS query
    uint16_t queryType = extractQueryType(query);

    // Build proper DNS response message
    std::vector<uint8_t> dnsData;
    
    // DNS Response Header (12 bytes)
    DnsHeader header;
    header.id = htons(queryId);  // Same transaction ID as query
    header.flags = htons(DNS_QR_RESPONSE | DNS_OPCODE_QUERY | DNS_AA | DNS_RD | DNS_RA);
    header.qdcount = htons(1);  // Echo the question
    header.ancount = htons(1);  // 1 answer
    header.nscount = htons(0);  // 0 authority RRs
    header.arcount = htons(0);  // 0 additional RRs
    
    // Add header to data
    uint8_t* headerBytes = reinterpret_cast<uint8_t*>(&header);
    dnsData.insert(dnsData.end(), headerBytes, headerBytes + sizeof(DnsHeader));
    
    // Question section (echo from query)
    // QNAME: example.com
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
    
    // Answer section
    // NAME: pointer to question (0xC00C)
    dnsData.push_back(0xC0);
    dnsData.push_back(0x0C);
    
    // TYPE: A (1)
    dnsData.push_back(0x00);
    dnsData.push_back(0x01);
    
    // CLASS: IN (1)
    dnsData.push_back(0x00);
    dnsData.push_back(0x01);
    
    // TTL: 300 seconds
    dnsData.push_back(0x00);
    dnsData.push_back(0x00);
    dnsData.push_back(0x01);
    dnsData.push_back(0x2C);
    
    // RDLENGTH: 4 (IPv4 address)
    dnsData.push_back(0x00);
    dnsData.push_back(0x04);
    
    // RDATA: IP address (e.g., 93.184.216.34 - example.com)
    dnsData.push_back(93);
    dnsData.push_back(184);
    dnsData.push_back(216);
    dnsData.push_back(34);
    
    // AMPLIFY BASED ON QUERY TYPE
    int amplificationFactor;
    if (queryType == 255) {  // DNS ANY query
        amplificationFactor = 28;  // Large amplification 
        EV_INFO << "[DnsReceiverHandler] ANY query detected - large response" << endl;
    } else if (queryType == 1) {  // DNS A query (IPv4)
        amplificationFactor = 2;   // Normal response size
        EV_INFO << "[DnsReceiverHandler] A query detected - normal response" << endl;
    } else if (queryType == 28) {  // DNS AAAA query (IPv6)
        amplificationFactor = 2;   // Normal response size
    } else {
        amplificationFactor = 2;   // Default
    }

    int targetSize = query->getByteLength() * amplificationFactor;
    while (dnsData.size() < (size_t)targetSize) {
        dnsData.push_back(0x00);  // Simulates large TXT/NS/SOA records
    }
    
    inet::Packet* response = new inet::Packet("DnsResponse");
    const auto& payload = inet::makeShared<inet::BytesChunk>(dnsData);
    response->insertAtBack(payload);
    
    // Set IP options: TTL=64, DF flag=1
    response->addTag<inet::HopLimitReq>()->setHopLimit(64);
    response->addTag<inet::FragmentationReq>()->setDontFragment(true);
    
    // Send response back to client
    dnsSocket.sendTo(response, srcAddr, srcPort);
    
    packetsSent++;
    
    EV_INFO << "DnsReceiverHandler sent response (" << dnsData.size() << " bytes) to " 
            << srcAddr << ":" << srcPort << std::endl;
    //std::cout << "[DnsReceiverHandler] Port " << port << " sent DNS response ("
    //          << dnsData.size() << " bytes)" << std::endl;
}

uint16_t DnsReceiverHandler::extractQueryType(inet::Packet* query) {
    // DNS query structure:
    // Header (12 bytes) + QNAME (variable) + QTYPE (2 bytes) + QCLASS (2 bytes)
    
    // Read entire query
    auto queryChunk = query->peekDataAt(inet::B(0), query->getDataLength());
    std::vector<uint8_t> queryBytes;
    auto bytesChunk = inet::dynamicPtrCast<const inet::BytesChunk>(queryChunk);
    if (bytesChunk) {
        queryBytes = bytesChunk->getBytes();
    }
    
    if (queryBytes.size() < 14) {  // Minimum: 12 (header) + 1 (name) + 1 (null) + 2 (type)
        return 1;  // Default to A query
    }
    
    // Skip header (12 bytes) and find QTYPE
    size_t pos = 12;
    
    // Skip QNAME (ends with 0x00)
    while (pos < queryBytes.size() && queryBytes[pos] != 0x00) {
        pos++;
    }
    pos++;  // Skip the 0x00
    
    // Read QTYPE (2 bytes, big-endian)
    if (pos + 2 <= queryBytes.size()) {
        uint16_t qtype = (queryBytes[pos] << 8) | queryBytes[pos + 1];
        return qtype;
    }
    
    return 1;  // Default to A query
}

void DnsReceiverHandler::socketErrorArrived(inet::UdpSocket* socket, inet::Indication* indication) {
    EV_WARN << "DnsReceiverHandler socket error on port " << port << std::endl;
    delete indication;
}

void DnsReceiverHandler::socketClosed(inet::UdpSocket* socket) {
    EV_INFO << "DnsReceiverHandler socket closed on port " << port << std::endl;
}

} // namespace ddosimu5g
