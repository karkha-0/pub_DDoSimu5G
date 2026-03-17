//
// TcpProtocolHandler.h - TCP protocol handler
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

#ifndef __DDOSIMU5G_TCPPROTOCOLHANDLER_H
#define __DDOSIMU5G_TCPPROTOCOLHANDLER_H

#include "ddosimu5g/apps/dynamic/protocols/ProtocolHandler.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/TimeTag_m.h"

namespace ddosimu5g {

/**
 * @brief TCP protocol handler for connection-based data transfer
 * 
 * Establishes TCP connection and sends data:
 * - requestLength: Size of data to send per request
 * - numRequests: Total number of requests to send
 * - requestInterval: Time between requests
 */
class TcpProtocolHandler : public ProtocolHandler, public inet::TcpSocket::ICallback {
public:
    TcpProtocolHandler(DynamicTrafficSender* parent,
                      std::shared_ptr<TrafficLabeler> labeler,
                      int appId);
    
    virtual ~TcpProtocolHandler();
    
    // ProtocolHandler interface
    void start(const nlohmann::json& config) override;
    void stop() override;
    void handleMessage(omnetpp::cMessage* msg) override;
    void processSocketMessage(omnetpp::cMessage* msg) override;
    std::string getProtocolName() const override { return "TCP"; }
    
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
    inet::TcpSocket tcpSocket;                     // TCP socket
    omnetpp::cMessage* requestTimer;               // Timer for periodic requests
    
    // Configuration parameters
    std::string destAddress;                       // Server address
    int destPort;                                  // Server port
    int localPort;                                 // Local port (0 for auto)
    int requestLength;                             // Request size (bytes)
    int numRequests;                               // Total requests to send
    double requestInterval;                        // Inter-request interval (seconds)
    bool keepAlive;                                // Keep connection open for full duration
    omnetpp::simtime_t startTime;                  // Start time
    omnetpp::simtime_t endTime;                    // End time
    
    int requestCount;                              // Current request counter
    bool connected;                                // Connection established
    
    static int s_nextEphemeralPort;                // Global ephemeral port counter
    
    virtual void sendRequest();                    // Send TCP request (virtual for HTTP override)
    void scheduleRequest();                        // Schedule next request
};

} // namespace ddosimu5g

#endif
