//
// LwM2MFloodAttack.h - LwM2M/OMA DM flood attack
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

#ifndef LWM2MFLOODATTACK_H
#define LWM2MFLOODATTACK_H

#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/common/packet/chunk/BytesChunk.h>
#include "ddosimu5g/apps/adversarialApps/BaseAttackApp.h"

using namespace inet;

namespace ddosimu5g {

class LwM2MFloodAttack : public BaseAttackApp {
private:
    UdpSocket socket;
    
    // Attack parameters
    double requestsPerSecond;
    int operationType;  // 1=Register, 2=Update, 3=Deregister
    std::string endpoint;
    int lifetime;
    
    // Timing
    cMessage* sendTimer = nullptr;
    simtime_t sendInterval;
    
    // Local address
    L3Address localAddress;
    int localPort;
    uint16_t messageId;

public:
    virtual ~LwM2MFloodAttack();
    
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage* msg) override;
    virtual void finish() override;
    
    virtual void startAttack() override;
    virtual void stopAttack() override;
    virtual void executeAttack() override;
    
private:
    void sendLwM2MRequest();
    void scheduleNextRequest();
    Packet* createLwM2MPacket();
    L3Address getLocalAddress();
};

} // namespace ddosimu5g

#endif // LWM2MFLOODATTACK_H
