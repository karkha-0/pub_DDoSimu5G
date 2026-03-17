//
// ReceiverProtocolHandler.h - Base receiver protocol handler interface
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

#ifndef __DDOSIMU5G_RECEIVERPROTOCOLHANDLER_H
#define __DDOSIMU5G_RECEIVERPROTOCOLHANDLER_H

#include <omnetpp.h>
#include "inet/common/packet/Packet.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include <memory>
#include "ddosimu5g/apps/dynamic/trafficlabel/TrafficLabeler.h"

namespace ddosimu5g {

// Forward declaration
class DynamicTrafficReceiver;

/**
 * @brief Base interface for receiver-side protocol handlers
 * 
 * Handles incoming packets and sends protocol-specific replies.
 * Each protocol (UDP, DNS, TCP, HTTP, MQTT, CoAP) implements this
 * interface with its own reply logic.
 */
class ReceiverProtocolHandler {
public:
    ReceiverProtocolHandler(DynamicTrafficReceiver* parent, int port)
        : parent(parent), port(port), packetsReceived(0), packetsSent(0) {}
    
    virtual ~ReceiverProtocolHandler() = default;
    
    /**
     * @brief Get protocol name (UDP, DNS, TCP, HTTP, etc.)
     * @return Protocol identifier string
     */
    virtual std::string getProtocolName() const = 0;
    
    /**
     * @brief Process incoming socket message
     * @param msg Message from socket layer
     * @return true if message was processed, false otherwise
     */
    virtual bool processMessage(omnetpp::cMessage* msg) = 0;
    
    /**
     * @brief Get listening port number
     * @return Port number
     */
    int getPort() const { return port; }
    
    /**
     * @brief Get packets received counter
     * @return Number of packets received
     */
    int getPacketsReceived() const { return packetsReceived; }
    
    /**
     * @brief Get packets sent counter
     * @return Number of packets sent
     */
    int getPacketsSent() const { return packetsSent; }

protected:
    DynamicTrafficReceiver* parent;           // Parent receiver module
    int port;                                  // Listening port
    int packetsReceived;                       // Received packet counter
    int packetsSent;                           // Sent packet counter
};

} // namespace ddosimu5g

#endif
