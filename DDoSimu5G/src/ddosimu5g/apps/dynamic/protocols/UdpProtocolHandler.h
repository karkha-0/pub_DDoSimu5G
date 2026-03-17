//
// UdpProtocolHandler.h - UDP protocol handler
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

#ifndef __DDOSIMU5G_UDPPROTOCOLHANDLER_H
#define __DDOSIMU5G_UDPPROTOCOLHANDLER_H

#include "ddosimu5g/apps/dynamic/protocols/ProtocolHandler.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/TimeTag_m.h"

namespace ddosimu5g {

/**
 * @brief UDP protocol handler for constant bitrate or bursty traffic
 * 
 * Generates UDP packets according to JSON configuration:
 * - messageLength: Packet size in bytes
 * - sendInterval: Time between packets (seconds)
 * - burstSize: Number of packets per burst (optional)
 */
class UdpProtocolHandler : public ProtocolHandler, public inet::UdpSocket::ICallback {
public:
    UdpProtocolHandler(DynamicTrafficSender* parent,
                      std::shared_ptr<TrafficLabeler> labeler,
                      int appId);
    
    virtual ~UdpProtocolHandler();
    
    // ProtocolHandler interface
    void start(const nlohmann::json& config) override;
    void stop() override;
    void handleMessage(omnetpp::cMessage* msg) override;
    void processSocketMessage(omnetpp::cMessage* msg) override;
    std::string getProtocolName() const override { return "UDP"; }
    
    // UdpSocket::ICallback interface
    void socketDataArrived(inet::UdpSocket *socket, inet::Packet *packet) override;
    void socketErrorArrived(inet::UdpSocket *socket, inet::Indication *indication) override;
    void socketClosed(inet::UdpSocket *socket) override;

private:
    inet::UdpSocket udpSocket;                     // UDP socket
    omnetpp::cMessage* sendTimer;                  // Timer for periodic sends
    
    // Configuration parameters
    std::string destAddress;                       // Destination address/hostname
    int destPort;                                  // Destination port
    int localPort;                                 // Local port for binding
    int messageLength;                             // Packet size (bytes)
    double sendInterval;                           // Inter-packet interval (seconds)
    int burstSize;                                 // Packets per burst
    omnetpp::simtime_t startTime;                  // Start time
    omnetpp::simtime_t endTime;                    // End time (startTime + duration)
    
    int burstCount;                                // Current burst counter
    
    void sendPacket();                             // Send single UDP packet
    void scheduleSend();                           // Schedule next send
};

} // namespace ddosimu5g

#endif
