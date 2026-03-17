//
// DnsAmplificationAttack - DNS amplification/reflection attack implementation
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

#ifndef DNSAMPLIFICATIONATTACK_H
#define DNSAMPLIFICATIONATTACK_H

#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/common/packet/chunk/BytesChunk.h>
#include <inet/networklayer/ipv4/Ipv4InterfaceData.h>
#include <vector>

#include "ddosimu5g/apps/adversarialApps/BaseAttackApp.h"
#include "ddosimu5g/trafficcontroller/AttackRegistry.h"
#include "AttackTracker.h"
#include "AttackMarkers.h"

using namespace inet;

namespace ddosimu5g {

/**
 * @brief DNS amplification/reflection attack
 * 
 * Exploits open DNS resolvers to amplify attack traffic toward victim.
 * Sends queries with spoofed source (victim IP) to open resolvers.
 * Typical amplification factor: 28x-54x.
 */
class DnsAmplificationAttack : public BaseAttackApp {
private:
    UdpSocket socket;
    
    // Attack parameters
    L3Address victimAddress;
    std::string victimAddressStr;
    std::vector<L3Address> openResolvers;
    double queriesPerSecond;
    int queryType;  // 255 = ANY (max amplification)
    bool enableSpoofing = true;  // Always true for DNS amp
    std::string queryDomain;          // Domain to query
    double amplificationFactor;       // Expected amplification factor
    
    // Local address
    L3Address localAddress;
    int localPort;
    int currentResolverIndex = 0;
    
    

public:
    virtual ~DnsAmplificationAttack();
    
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage* msg) override;
    virtual void finish() override;
    
    virtual void startAttack() override;
    virtual void stopAttack() override;
    virtual void sendAttackPacket() override;
    
    
private:
    void sendDnsQuery();
    Packet* createDnsQuery(const char* domain, int qtype);
    L3Address getNextResolver();
    L3Address getLocalAddress();
};

} // namespace ddosimu5g

#endif // DNSAMPLIFICATIONATTACK_H
