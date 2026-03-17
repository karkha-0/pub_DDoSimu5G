//
// TcpSynFloodAttack - TCP SYN flood attack implementation
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

#ifndef TCPSYNFLOODATTACK_H
#define TCPSYNFLOODATTACK_H

#include <inet/transportlayer/tcp_common/TcpHeader_m.h>
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include <inet/transportlayer/tcp_common/TcpCrcInsertionHook.h>

#include "ddosimu5g/apps/adversarialApps/BaseAttackApp.h"
#include "ddosimu5g/trafficcontroller/AttackRegistry.h"

using namespace inet;
using namespace inet::tcp;

namespace ddosimu5g {

/**
 * @brief TCP SYN flood attack using raw packet injection
 * 
 * Bypasses normal TCP stack to send SYN packets directly via MessageDispatcher.
 * Supports source IP spoofing and multi-port targeting.
 */
class TcpSynFloodAttack : public BaseAttackApp {
private:
    // Attack parameters
    bool enableSpoofing;
    bool randomizeDestPort;
    std::vector<int> targetPorts;
    simtime_t attackStartTime;

    // Local address and interface ID for routing
    L3Address localAddress;
    int currentSrcPort;
    int cellularInterfaceId = -1;  // For MessageDispatcher routing
    
    void sendSynPacket();
    L3Address getLocalAddress();
    L3Address generateSpoofedSource();
    int getNextSrcPort();
    int getTargetPort();

public:
    virtual ~TcpSynFloodAttack();
    
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage* msg) override;
    virtual void finish() override;
    
    virtual void startAttack() override;
    virtual void stopAttack() override;
    virtual void sendAttackPacket() override;
};

} // namespace ddosimu5g

#endif // TCPSYNFLOODATTACK_H
