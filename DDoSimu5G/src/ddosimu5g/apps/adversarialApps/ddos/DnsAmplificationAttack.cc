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

#include "DnsAmplificationAttack.h"

using namespace ddosimu5g;

namespace {
    struct DnsAmpRegistrar {
        DnsAmpRegistrar() {
            AttackRegistry::registerAttack("dns_amplification", 
                "ddosimu5g.apps.adversarialApps.ddos.DnsAmplificationAttack");
            std::cout << "[DnsAmplificationAttack] ✓ Registered successfully" << std::endl;
        }
    };
    static DnsAmpRegistrar registrar;
}

namespace ddosimu5g {

Define_Module(DnsAmplificationAttack);

DnsAmplificationAttack::~DnsAmplificationAttack() {
}

void DnsAmplificationAttack::initialize(int stage) {
    BaseAttackApp::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        // Read DNS amplification specific parameters
        victimAddressStr = par("victimAddress").stdstringValue();
        queryType = par("queryType");  // 255 = ANY for max amplification
        localPort = par("localPort");
        enableSpoofing = par("enableSpoofing");
        queryDomain = par("queryDomain").stdstringValue();
        amplificationFactor = par("amplificationFactor");

        if (!config.attackType.empty()) {
            enableSpoofing = config.enableSpoofing;

            if (!config.targets.empty()) {
                victimAddressStr = config.targets[0];
            }
            if (!config.queryDomain.empty()) {
                queryDomain = config.queryDomain;
            }
            if (config.amplificationFactor > 0) {
                amplificationFactor = config.amplificationFactor;
            }
            
            std::cout << "[DnsAmplificationAttack] Configured from JSON:" << std::endl;
            std::cout << "  - Victim: " << victimAddressStr << std::endl;
            std::cout << "  - Spoofing: " << enableSpoofing << std::endl;
            std::cout << "  - Query domain: " << queryDomain << std::endl;
            std::cout << "  - Amplification factor: " << amplificationFactor << "x" << std::endl;
        }

        // Parse DNS resolvers from config, fallback to NED parameter
        const char* resolversParam = nullptr;
        
        if (!config.openResolvers.empty()) {
            resolversParam = config.openResolvers.c_str();
            std::cout << "  - Open Resolvers (from JSON): " << resolversParam << std::endl;
        }
        else if (hasPar("openResolvers")) {
            resolversParam = par("openResolvers").stringValue();
            std::cout << "  - Open Resolvers (from NED): " << resolversParam << std::endl;
        }
        
        // Parse resolver addresses
        if (resolversParam && strlen(resolversParam) > 0) {
            cStringTokenizer tokenizer(resolversParam);
            while (tokenizer.hasMoreTokens()) {
                const char* resolverStr = tokenizer.nextToken();
                L3Address resolver = resolveDestAddress(resolverStr);
                if (!resolver.isUnspecified()) {
                    openResolvers.push_back(resolver);
                    EV_INFO << "    → Resolver: " << resolverStr 
                            << " → " << resolver << endl;
                    std::cout << "    → Resolver: " << resolverStr 
                              << " → " << resolver << std::endl;
                }
            }
        }
        
        if (openResolvers.empty()) {
            EV_WARN << "[DnsAmplificationAttack] No open resolvers configured!" << endl;
            std::cerr << "[DnsAmplificationAttack] No open resolvers configured!" << std::endl;
        }        
        
        EV << "[DnsAmplificationAttack] Configured: qps to " << openResolvers.size() << " resolvers" << endl;
        EV << "  Victim: " << victimAddressStr << endl;
        EV << "  Query domain: " << queryDomain << endl;
        EV << "  Expected amplification: " << amplificationFactor << "x" << endl;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Resolve victim address
        victimAddress = resolveDestAddress(victimAddressStr.c_str());
        if (victimAddress.isUnspecified()) {
            EV_ERROR << "[DnsAmplificationAttack] Failed to resolve victim: " 
                     << victimAddressStr << endl;
            return;
        }
        
        // Get local address and bind socket
        localAddress = getLocalAddress();
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localAddress, localPort);
        short dscp = (AttackMarkers::TOS_DNS_AMP >> 2) & 0x3F;
        socket.setDscp(dscp);
        socket.setTimeToLive(64);  // Match benign TTL (INET default is 32)
        
        EV << "[DnsAmplificationAttack] Ready. Victim: " << victimAddress 
           << ", Resolvers: " << openResolvers.size() << endl;
    }
}

void DnsAmplificationAttack::startAttack() {
    if (openResolvers.empty() || victimAddress.isUnspecified()) {
        EV_ERROR << "[DnsAmplificationAttack] Cannot start - invalid configuration" << endl;
        return;
    }
    
    EV << "[DnsAmplificationAttack] Starting DNS amplification attack" << endl;
    EV << "  Victim: " << victimAddress << endl;
    EV << "  Resolvers: " << openResolvers.size() << endl;
    
    AttackTracker::getInstance()->registerDnsAmplificationBot(localAddress);

    // Call parent - it will create sendTimer and set up style system
    BaseAttackApp::startAttack();

    // Send first query
    sendDnsQuery();
    
    // Schedule subsequent queries
    scheduleNextAttack();
}

void DnsAmplificationAttack::stopAttack() {
    EV << "[DnsAmplificationAttack] Stopping attack. Sent " << packetsSent 
       << " DNS queries" << endl;
    
    AttackTracker::getInstance()->unregisterDnsAmplificationBot(localAddress);

    // Call parent FIRST - it will cancel timers and set isActive=false
    BaseAttackApp::stopAttack();

    socket.close();
}

void DnsAmplificationAttack::sendAttackPacket() {
    if (!isActive) {
        EV << "[DnsAmplificationAttack] sendAttackPacket called but not active" << endl;
        std::cout << "[DnsAmplificationAttack] sendAttackPacket called but not active" << std::endl;
        return;
    }
    
    // Send DNS query
    sendDnsQuery();
}

void DnsAmplificationAttack::handleMessageWhenUp(cMessage* msg) {
    if (msg->arrivedOn("socketIn")) {
        // DNS responses (we don't care about them in attack mode)
        delete msg;
    } else {
        BaseAttackApp::handleMessageWhenUp(msg);
    }
}

void DnsAmplificationAttack::sendDnsQuery() {
    // Select next open resolver
    L3Address resolver = getNextResolver();

    Packet* packet = createDnsQuery(queryDomain.c_str(), queryType);

    if (!packet) {
        EV_ERROR << "[DnsAmplificationAttack] Failed to create DNS query packet" << endl;
        std::cerr << "[DnsAmplificationAttack] Failed to create DNS query packet" << endl;
        return;
    }
    
    socket.sendTo(packet, resolver, 53);  // DNS port

    logAttackPacket("TX", "UDP", 
                    localAddress,                    // Real source IP (bot)
                    localPort, 
                    resolver, 
                    53, 
                    packet->getByteLength(),
                    enableSpoofing,                  // Spoofing flag
                    victimAddress.str().c_str());    // Victim IP (metadata)
    
    char extra[256];
    sprintf(extra, "(victim=%s, domain=%s, amp=%.1fx)", 
            victimAddress.str().c_str(),
            queryDomain.c_str(),
            amplificationFactor);
    printProgress("DNS-AMP", extra);
}

Packet* DnsAmplificationAttack::createDnsQuery(const char* domain, int qtype) {
    // Create simplified DNS query packet
    // Format: Header (12 bytes) + Question section
    
    char packetName[64];
    sprintf(packetName, "DnsQuery-%d", packetsSent);
    Packet* packet = new Packet(packetName);
    
    // DNS header (12 bytes) + Question (variable)
    // For ANY query type (255), this generates large responses (amplification)
    int headerSize = 12;
    
    int domainLen = strlen(domain) + 1;  // +1 for null terminator
    cStringTokenizer counter(domain, ".");
    while (counter.hasMoreTokens()) {
        counter.nextToken();
        domainLen++;  // +1 for each label's length byte
    }
    
    int questionSize = domainLen + 4;    // +4 for qtype and qclass
    int totalSize = headerSize + questionSize;
    
    // Create DNS query payload
    std::vector<uint8_t> dnsData(totalSize);
    
    // DNS Header
    dnsData[0] = 0x00; dnsData[1] = 0x01;  // Transaction ID
    dnsData[2] = 0x01; dnsData[3] = 0x00;  // Flags: standard query
    dnsData[4] = 0x00; dnsData[5] = 0x01;  // Questions: 1
    dnsData[6] = 0x00; dnsData[7] = 0x00;  // Answer RRs: 0
    dnsData[8] = 0x00; dnsData[9] = 0x00;  // Authority RRs: 0
    dnsData[10] = 0x00; dnsData[11] = 0x00; // Additional RRs: 0

    // Encode domain name: "example.com" → 7example3com0
    int offset = 12;  // After header

    // Split domain by dots
    cStringTokenizer tokenizer(domain, ".");
    while (tokenizer.hasMoreTokens()) {
        const char* label = tokenizer.nextToken();
        int labelLen = strlen(label);

        // Bounds check
        if (offset + labelLen + 1 > totalSize) {
            EV_ERROR << "[DnsAmplificationAttack] Domain too long: " << domain << endl;
            delete packet;
            return nullptr;
        }
        
        dnsData[offset++] = labelLen;  // Length byte
        memcpy(&dnsData[offset], label, labelLen);
        offset += labelLen;
    }
    dnsData[offset++] = 0;  // Null terminator

    // Question type (ANY = 255 for max amplification)
    dnsData[offset++] = (qtype >> 8) & 0xFF;  // QTYPE high byte
    dnsData[offset++] = qtype & 0xFF;         // QTYPE low byte

    // Question class (IN = 1)
    dnsData[offset++] = 0x00;  // QCLASS high byte
    dnsData[offset++] = 0x01;  // QCLASS low byte
    
    const auto& payload = makeShared<BytesChunk>(dnsData);
    packet->insertAtBack(payload);
    
    return packet;
}

L3Address DnsAmplificationAttack::getNextResolver() {
    if (openResolvers.empty()) {
        return L3Address();
    }
    
    L3Address resolver = openResolvers[currentResolverIndex];
    currentResolverIndex = (currentResolverIndex + 1) % openResolvers.size();
    return resolver;
}

L3Address DnsAmplificationAttack::getLocalAddress() {
    L3Address addr;
    IInterfaceTable* ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
    
    for (int i = 0; i < ift->getNumInterfaces(); i++) {
        NetworkInterface* ie = ift->getInterface(i);
        if (ie->isLoopback()) continue;

        auto ipv4Data = ie->getProtocolData<Ipv4InterfaceData>();
        if (ipv4Data) {
            Ipv4Address ipv4Addr = ipv4Data->getIPAddress();
            std::cout << " - IPv4: " << ipv4Addr;
            
            if (!ipv4Addr.isUnspecified()) {
                addr = ipv4Addr;
                break;
            } 
        }
    }
    
    if (addr.isUnspecified()) {
        std::cerr << "[DnsAmplificationAttack] ERROR: No valid IPv4 address found!" << std::endl;
    } 

    return addr;
}

void DnsAmplificationAttack::finish() {
    double estimatedResponseBytes = bytesSent * amplificationFactor;
    
    EV << "[DnsAmplificationAttack] Attack finished:" << endl;
    EV << "  Queries sent: " << packetsSent << endl;
    EV << "  Query bytes: " << bytesSent << endl;
    EV << "  Query domain: " << queryDomain << endl;
    EV << "  Estimated response bytes: " << estimatedResponseBytes << endl;
    EV << "  Amplification factor: " << amplificationFactor << "x" << endl;
    EV << "  Victim: " << victimAddress << endl;
    
    recordScalar("queriesSent", packetsSent);
    recordScalar("queryBytesSent", bytesSent);
    recordScalar("amplificationFactor", amplificationFactor);
    recordScalar("estimatedAttackVolume", estimatedResponseBytes);
    
    BaseAttackApp::finish();
}


} // namespace ddosimu5g
