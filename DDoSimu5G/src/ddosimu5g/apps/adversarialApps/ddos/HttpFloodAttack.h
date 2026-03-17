//
// HttpFloodAttack.h - HTTP flood attack
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

#ifndef HTTPFLOODATTACK_H
#define HTTPFLOODATTACK_H

#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/contract/IInterfaceTable.h>
#include <inet/networklayer/ipv4/Ipv4InterfaceData.h>
#include <inet/common/packet/chunk/ByteCountChunk.h>
#include <inet/common/packet/Packet.h>
#include <sstream>
#include <set>
#include <inet/transportlayer/contract/tcp/TcpSocket.h>
#include <inet/common/socket/SocketMap.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/transportlayer/common/L4PortTag_m.h>

#include "ddosimu5g/apps/adversarialApps/BaseAttackApp.h"
#include "ddosimu5g/trafficcontroller/AttackRegistry.h"
#include "AttackMarkers.h"

using namespace inet;

namespace ddosimu5g {

/**
 * @brief HTTP flood attack 
 * 
 * Establishes multiple TCP connections and floods target with HTTP
 * GET/POST requests. Supports realistic User-Agent headers for IoT
 * device simulation (Tesla, SmartMeter, DJI drones).
 */
class HttpFloodAttack : public BaseAttackApp, public TcpSocket::ICallback {
private:
    std::vector<TcpSocket*> connections;
    SocketMap socketMap;
    
    // Attack parameters
    int numConnections;
    std::string httpMethod;  // "GET" or "POST"
    std::vector<std::string> requestPaths;
    std::string userAgent;
    int contentLength;  // For POST requests
    bool keepAlive;
    
    // Connection management
    int connectionsEstablished = 0;
    std::set<TcpSocket*> establishedSockets;  // Track which sockets successfully connected
    int connectionFailures = 0;
    
    cMessage* connectTimer = nullptr;
    
    // Local address
    L3Address localAddress;
    int currentPathIndex = 0;

public:
    virtual ~HttpFloodAttack();
    
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage* msg) override;
    virtual void finish() override;
    
    // TcpSocket::ICallback methods
    virtual void socketDataArrived(TcpSocket* socket, Packet* packet, bool urgent) override;
    virtual void socketAvailable(TcpSocket* socket, TcpAvailableInfo* availableInfo) override { delete availableInfo; }
    virtual void socketEstablished(TcpSocket* socket) override;
    virtual void socketPeerClosed(TcpSocket* socket) override { }
    virtual void socketClosed(TcpSocket* socket) override;
    virtual void socketFailure(TcpSocket* socket, int code) override;
    virtual void socketStatusArrived(TcpSocket* socket, TcpStatusInfo* status) override { delete status; }
    virtual void socketDeleted(TcpSocket* socket) override { }
    
    virtual void startAttack() override;
    virtual void stopAttack() override;
    virtual void sendAttackPacket() override;
    
private:
    void establishConnections();
    void sendHttpRequest();
    Packet* createHttpGetRequest();
    Packet* createHttpPostRequest();
    TcpSocket* getAvailableConnection();
    std::string getNextPath();
    L3Address getLocalAddress();
};

} // namespace ddosimu5g

#endif // HTTPFLOODATTACK_H
