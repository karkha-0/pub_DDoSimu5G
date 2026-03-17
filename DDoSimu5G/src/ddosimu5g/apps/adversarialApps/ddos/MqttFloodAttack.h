//
// MqttFloodAttack.h - MQTT publish flood attack
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

#ifndef MQTTFLOODATTACK_H
#define MQTTFLOODATTACK_H

#include <inet/transportlayer/contract/tcp/TcpSocket.h>
#include <inet/common/packet/chunk/BytesChunk.h>
#include "ddosimu5g/apps/adversarialApps/BaseAttackApp.h"

using namespace inet;

namespace ddosimu5g {

class MqttFloodAttack : public BaseAttackApp {
private:
    TcpSocket socket;
    
    // Attack parameters
    double publishesPerSecond;
    int qos;  // 0, 1, or 2
    std::string topic;
    int payloadSize;
    bool retain;
    
    // Connection state
    bool connected = false;
    
    // Timing
    cMessage* sendTimer = nullptr;
    cMessage* connectTimer = nullptr;
    simtime_t sendInterval;
    
    // Local address
    L3Address localAddress;
    uint16_t packetId;

public:
    virtual ~MqttFloodAttack();
    
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage* msg) override;
    virtual void socketDataArrived(TcpSocket* socket, Packet* packet, bool urgent);
    virtual void socketEstablished(TcpSocket* socket);
    virtual void socketClosed(TcpSocket* socket);
    virtual void finish() override;
    
    virtual void startAttack() override;
    virtual void stopAttack() override;
    virtual void executeAttack() override;
    
private:
    void connectToBroker();
    void sendMqttConnect();
    void sendMqttPublish();
    void scheduleNextPublish();
    Packet* createMqttConnectPacket();
    Packet* createMqttPublishPacket();
    L3Address getLocalAddress();
};

} // namespace ddosium5g

#endif // MQTTFLOODATTACK_H
