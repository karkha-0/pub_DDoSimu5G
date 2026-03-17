//
// UdpFloodAttack - UDP flood DDoS attack implementation
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

#ifndef UDPFLOODATTACK_H
#define UDPFLOODATTACK_H

#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/common/packet/chunk/ByteCountChunk.h>
#include <inet/networklayer/contract/IInterfaceTable.h>
#include <inet/networklayer/ipv4/Ipv4InterfaceData.h>
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/transportlayer/common/L4PortTag_m.h>

#include "ddosimu5g/apps/adversarialApps/BaseAttackApp.h"
#include "ddosimu5g/trafficcontroller/AttackRegistry.h"
#include "AttackMarkers.h"

using namespace inet;

namespace ddosimu5g {

/**
 * @brief UDP flood DDoS attack implementation
 * 
 * Sends high-rate UDP packets to overwhelm target. Supports source
 * IP spoofing for amplification attacks. Rate controlled by BaseAttackApp.
 */
class UdpFloodAttack : public BaseAttackApp {
private:
    UdpSocket socket;
    
    // Attack parameters
    int packetSize;
    double packetsPerSecond;
    bool enableSpoofing;
    double jitter;
    
    // Local address info
    L3Address localAddress;
    int localPort;

public:
    virtual ~UdpFloodAttack();
    
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage* msg) override;
    virtual void finish() override;
    
    // Attack lifecycle
    virtual void startAttack() override;
    virtual void stopAttack() override;
    //virtual void executeAttack() override;
    virtual void sendAttackPacket() override;
    
private:
    void sendFloodPacket();
    L3Address getLocalAddress();
    L3Address generateSpoofedSource();
};

} // namespace ddosimu5g

#endif // UDPFLOODATTACK_H
