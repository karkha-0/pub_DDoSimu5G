//
// DnsReceiverHandler.h - DNS receiver handler
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

#ifndef __DDOSIMU5G_DNSRECEIVERHANDLER_H
#define __DDOSIMU5G_DNSRECEIVERHANDLER_H

#include "ddosimu5g/apps/dynamic/protocols/receiver/ReceiverProtocolHandler.h"
#include "ddosimu5g/apps/adversarialApps/ddos/AttackTracker.h"  // Attack tracker for DNS amplification bots
#include "ddosimu5g/apps/dynamic/DynamicTrafficReceiver.h"

#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"

#include <iostream>
#include <arpa/inet.h>  // For htons


namespace ddosimu5g {

/**
 * @brief DNS receiver handler - DNS query responder
 * 
 * Receives DNS queries and sends DNS responses (typically 2x query size)
 */
class DnsReceiverHandler : public ReceiverProtocolHandler, public inet::UdpSocket::ICallback {
public:
    DnsReceiverHandler(DynamicTrafficReceiver* parent, int port);
    virtual ~DnsReceiverHandler();
    
    // ReceiverProtocolHandler interface
    std::string getProtocolName() const override { return "DNS"; }
    bool processMessage(omnetpp::cMessage* msg) override;
    
    // UdpSocket::ICallback interface
    void socketDataArrived(inet::UdpSocket *socket, inet::Packet *packet) override;
    void socketErrorArrived(inet::UdpSocket *socket, inet::Indication *indication) override;
    void socketClosed(inet::UdpSocket *socket) override;

private:
    inet::UdpSocket dnsSocket;
    
    void sendDnsResponse(inet::Packet* query);
    uint16_t extractQueryType(inet::Packet* query); 
};

} // namespace ddosimu5g

#endif
