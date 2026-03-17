//
// TcpReceiverHandler.h - TCP receiver handler
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

#ifndef __DDOSIMU5G_TCPRECEIVERHANDLER_H
#define __DDOSIMU5G_TCPRECEIVERHANDLER_H

#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/socket/SocketTag_m.h"
#include "inet/common/TimeTag_m.h"
#include <map>
#include "ddosimu5g/apps/dynamic/protocols/receiver/ReceiverProtocolHandler.h"

namespace ddosimu5g {

/**
 * @brief TCP receiver handler - generic TCP server
 * 
 * Accepts TCP connections and sends replies to incoming data
 */
class TcpReceiverHandler : public ReceiverProtocolHandler, public inet::TcpSocket::ICallback {
public:
    TcpReceiverHandler(DynamicTrafficReceiver* parent, int port, bool echoMode = true);
    virtual ~TcpReceiverHandler();
    
    // ReceiverProtocolHandler interface
    std::string getProtocolName() const override { return "TCP"; }
    bool processMessage(omnetpp::cMessage* msg) override;
    
    // TcpSocket::ICallback interface
    void socketDataArrived(inet::TcpSocket *socket, inet::Packet *packet, bool urgent) override;
    void socketAvailable(inet::TcpSocket *socket, inet::TcpAvailableInfo *availableInfo) override;
    void socketEstablished(inet::TcpSocket *socket) override;
    void socketPeerClosed(inet::TcpSocket *socket) override;
    void socketClosed(inet::TcpSocket *socket) override;
    void socketFailure(inet::TcpSocket *socket, int code) override;
    void socketStatusArrived(inet::TcpSocket *socket, inet::TcpStatusInfo *status) override;
    void socketDeleted(inet::TcpSocket *socket) override;

protected:
    inet::TcpSocket serverSocket;              // Listening socket
    std::map<int, inet::TcpSocket*> clientSockets;  // Active client connections
    bool echoMode;
    int replyMultiplier;  // Reply size = request size * multiplier
    
    virtual void sendReply(inet::TcpSocket* socket, inet::Packet* request);
    void shutdownSockets();  // Cleanup sockets gracefully
};

} // namespace ddosimu5g

#endif
