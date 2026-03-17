//
// ProtocolHandler.cc - Base protocol handler implementation
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

#include "ddosimu5g/apps/dynamic/protocols/ProtocolHandler.h"

#include "ddosimu5g/apps/dynamic/DynamicTrafficSender.h"
#include "inet/networklayer/common/L3AddressResolver.h"

namespace ddosimu5g {

/*
void ProtocolHandler::logPacketToCSV(const std::string& protocol,
                                     const std::string& destAddr,
                                     int destPort,
                                     int packetSize,
                                     int srcPort,
                                     simtime_t timestamp) {

    inet::L3Address destIP = inet::L3AddressResolver().resolve(destAddr.c_str());                                        

    logPacketToCSV(protocol, destIP.str(), destPort, packetSize, "", srcPort, timestamp);
}
*/

/**/
void ProtocolHandler::logPacketToCSV(const std::string& protocol,
                                     const std::string& destAddr,
                                     int destPort,
                                     int packetSize,
                                     //const std::string& srcAddr,
                                     int srcPort,
                                     simtime_t timestamp) {
    if (!labeler || !labeler->isOpen()) {
        std::cout << "[ProtocolHandler] WARNING: labeler not available for logging!" << std::endl;
        return;
    }
    
    LabelEntry entry;
    //entry.timestamp = omnetpp::simTime();
    entry.timestamp = timestamp;  
    entry.srcModule = parent->getFullPath();
    
    // Get source IP if not provided
    // if (srcAddr.empty()) {
    //     // Try to resolve from parent module
    //     inet::L3AddressResolver resolver;
    //     inet::L3Address addr = resolver.addressOf(parent->getParentModule());
    //     entry.srcIP = addr.str();
    // } else {
    //     entry.srcIP = srcAddr;
    // }

    inet::L3AddressResolver resolver;
    inet::L3Address addr = resolver.addressOf(parent->getParentModule());
    entry.srcIP = addr.str();

    entry.srcPort = srcPort;
    
    // Determine direction and packet number based on protocol type
    bool isReceived = (protocol.find("REPLY") != std::string::npos || 
                      protocol.find("RESPONSE") != std::string::npos ||
                      protocol.find("ANSWER") != std::string::npos);
    
    inet::L3Address destIP = inet::L3AddressResolver().resolve(destAddr.c_str()); 

    if (isReceived) {
        entry.direction = "RX";
        entry.packetNumber = packetsReceived;
        
        // For RX packets: swap src/dest to match network perspective
        // PCAP shows: server → client
        // Labels need: src=server, dest=client (not client→server)
        entry.destAddress = entry.srcIP;   // Client becomes destination
        entry.destPort = srcPort;          // Client port becomes dest port
        entry.srcIP = destIP.str();            // Server becomes source
        entry.srcPort = destPort;          // Server port becomes src port
    } else {
        entry.direction = "TX";
        entry.packetNumber = packetsSent;
        
        // For TX packets: keep as-is (already correct)
        entry.destAddress = destIP.str();
        entry.destPort = destPort;
    }
    
    entry.trafficType = currentTrafficType;
    entry.protocol = protocol;
    entry.packetSize = packetSize;
    entry.label = currentLabel;
    
    //std::cout << "[ProtocolHandler] Logging to CSV: " << entry.direction << " "
    //          << protocol << " " << packetSize << " bytes" << std::endl;
    
    bool result = labeler->logPacket(entry);
    
    if (!result) {
        std::cout << "[ProtocolHandler] ERROR: Failed to log packet to CSV!" << std::endl;
    }
}

/* UNUSED - LOGGING FROM PACKET
void ProtocolHandler::logPacketToCSV(const std::string& protocol, 
                                     inet::Packet* packet,
                                     bool isTX) {
    if (!labeler || !labeler->isOpen()) {
        std::cout << "[ProtocolHandler] WARNING: labeler not available for logging!" << std::endl;
        return;
    }
    
    if (packet == nullptr) {
        std::cout << "[ProtocolHandler] ERROR: packet is nullptr!" << std::endl;
        return;
    }
    
    LabelEntry entry;
    
    // TEST ALL TIMING METHODS
    simtime_t creation = packet->getCreationTime();
    simtime_t sending = packet->getSendingTime();
    simtime_t arrival = packet->getArrivalTime();
    simtime_t timestamp = packet->getTimestamp();
    simtime_t duration = packet->getDuration();
    
    std::cout << "[TIMING TEST] " << (isTX ? "TX" : "RX") 
              << " creation=" << creation 
              << " sending=" << sending 
              << " arrival=" << arrival 
              << " timestamp=" << timestamp
              << " duration=" << duration << std::endl;
    
    // Use creationTime for now
    entry.timestamp = packet->getCreationTime();

    // EXTRACT IP ADDRESSES from packet tags
    auto srcAddrInd = packet->findTag<inet::L3AddressInd>();
    auto dstAddrReq = packet->findTag<inet::L3AddressReq>();
    
    if (isTX) {
        // TX: packet being sent, use Req tags (what we're requesting)
        if (dstAddrReq) {
            entry.destAddress = dstAddrReq->getDestAddress().str();
        }
        // Source from parent module
        inet::L3AddressResolver resolver;
        inet::L3Address addr = resolver.addressOf(parent->getParentModule());
        entry.srcIP = addr.str();
        
        entry.direction = "TX";
        entry.packetNumber = packetsSent;
    } else {
        // RX: Try both Ind and Req tags
        if (srcAddrInd) {
            entry.srcIP = srcAddrInd->getSrcAddress().str();
            entry.destAddress = srcAddrInd->getDestAddress().str();
        } else if (dstAddrReq) {
            // Fallback: sometimes Req tags exist on RX
            entry.srcIP = dstAddrReq->getSrcAddress().str();
            entry.destAddress = dstAddrReq->getDestAddress().str();
        } else {
            std::cout << "[ERROR] RX packet has NO address tags!" << std::endl;
        }
        
        entry.direction = "RX";
        entry.packetNumber = packetsReceived;
    }
    
    // EXTRACT PORTS from packet tags
    auto srcPortInd = packet->findTag<inet::L4PortInd>();
    auto dstPortReq = packet->findTag<inet::L4PortReq>();
    
    if (isTX && dstPortReq) {
        entry.destPort = dstPortReq->getDestPort();
        entry.srcPort = dstPortReq->getSrcPort();  // Local port
    } else if (!isTX) {
        // RX: Try both tag types
        if (srcPortInd) {
            entry.srcPort = srcPortInd->getSrcPort();
            entry.destPort = srcPortInd->getDestPort();
        } else if (dstPortReq) {
            entry.srcPort = dstPortReq->getSrcPort();
            entry.destPort = dstPortReq->getDestPort();
        } else {
            std::cout << "[ERROR] RX packet has NO port tags!" << std::endl;
        }
    }
    
    // EXTRACT PACKET SIZE
    entry.packetSize = packet->getByteLength();
    
    // Set other fields
    entry.srcModule = parent->getFullPath();
    entry.trafficType = currentTrafficType;
    entry.protocol = protocol;
    entry.label = currentLabel;
    
    std::cout << "[ProtocolHandler] Packet-based logging: " << entry.direction 
              << " " << protocol << " " << entry.srcIP << ":" << entry.srcPort 
              << " -> " << entry.destAddress << ":" << entry.destPort 
              << " (" << entry.packetSize << "B) at t=" << entry.timestamp << std::endl;
    
    bool result = labeler->logPacket(entry);
    
    if (!result) {
        std::cout << "[ProtocolHandler] ERROR: Failed to log packet to CSV!" << std::endl;
    }
}
*/

} // namespace ddosimu5g
