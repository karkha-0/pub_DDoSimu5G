//
// UdpReceiverHandler.h - UDP receiver handler
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

#ifndef __DDOSIMU5G_UDPRECEIVERHANDLER_H
#define __DDOSIMU5G_UDPRECEIVERHANDLER_H

#include "ddosimu5g/apps/dynamic/protocols/receiver/ReceiverProtocolHandler.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"

namespace ddosimu5g {

/**
 * @brief UDP receiver handler - simple echo server
 * 
 * Receives UDP packets and echoes them back with configurable size multiplier
 */
class UdpReceiverHandler : public ReceiverProtocolHandler, public inet::UdpSocket::ICallback {
public:
    UdpReceiverHandler(DynamicTrafficReceiver* parent, int port, bool echoMode = true);
    virtual ~UdpReceiverHandler();
    
    // ReceiverProtocolHandler interface
    std::string getProtocolName() const override { return "UDP"; }
    bool processMessage(omnetpp::cMessage* msg) override;
    
    // UdpSocket::ICallback interface
    void socketDataArrived(inet::UdpSocket *socket, inet::Packet *packet) override;
    void socketErrorArrived(inet::UdpSocket *socket, inet::Indication *indication) override;
    void socketClosed(inet::UdpSocket *socket) override;

private:
    inet::UdpSocket udpSocket;
    bool echoMode;
    int replyMultiplier;  // Reply size = request size * multiplier
    
    void sendReply(inet::Packet* receivedPacket);
};

} // namespace ddosimu5g

#endif
