//
// DnsProtocolHandler.h - DNS protocol handler
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

#ifndef __DDOSIMU5G_DNSPROTOCOLHANDLER_H
#define __DDOSIMU5G_DNSPROTOCOLHANDLER_H

#include "ddosimu5g/apps/dynamic/protocols/ProtocolHandler.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/TimeTag_m.h"

namespace ddosimu5g {

/**
 * @brief DNS protocol handler for query-response patterns
 * 
 * Simulates DNS queries with configurable query/response sizes:
 * - queryLength: DNS query size in bytes
 * - responseLength: Expected DNS response size
 * - queryInterval: Time between queries
 */
class DnsProtocolHandler : public ProtocolHandler, public inet::UdpSocket::ICallback {
public:
    DnsProtocolHandler(DynamicTrafficSender* parent,
                      std::shared_ptr<TrafficLabeler> labeler,
                      int appId);
    
    virtual ~DnsProtocolHandler();
    
    // ProtocolHandler interface
    void start(const nlohmann::json& config) override;
    void stop() override;
    void handleMessage(omnetpp::cMessage* msg) override;
    void processSocketMessage(omnetpp::cMessage* msg) override;
    std::string getProtocolName() const override { return "DNS"; }
    
    // UdpSocket::ICallback interface
    void socketDataArrived(inet::UdpSocket *socket, inet::Packet *packet) override;
    void socketErrorArrived(inet::UdpSocket *socket, inet::Indication *indication) override;
    void socketClosed(inet::UdpSocket *socket) override;

private:
    inet::UdpSocket udpSocket;                     // UDP socket (DNS uses UDP)
    omnetpp::cMessage* queryTimer;                 // Timer for periodic queries
    
    // Configuration parameters
    std::string destAddress;                       // DNS server address
    int destPort;                                  // DNS server port (typically 53)
    int localPort;                                 // Local port (0 for auto)
    int queryLength;                               // Query size (bytes)
    int responseLength;                            // Expected response size
    double queryInterval;                          // Inter-query interval (seconds)
    omnetpp::simtime_t startTime;                  // Start time
    omnetpp::simtime_t endTime;                    // End time
    
    bool waitingForReply;                          // Waiting for DNS response
    
    void sendQuery();                              // Send DNS query
    void scheduleQuery();                          // Schedule next query
};

} // namespace ddosimu5g

#endif
