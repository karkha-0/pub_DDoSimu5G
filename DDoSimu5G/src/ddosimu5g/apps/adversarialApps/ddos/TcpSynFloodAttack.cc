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

#include "TcpSynFloodAttack.h"
#include "AttackMarkers.h"

using namespace ddosimu5g;

namespace {
    struct TcpSynRegistrar {
        TcpSynRegistrar() {
            AttackRegistry::registerAttack("tcp_syn_flood", 
                "ddosimu5g.apps.adversarialApps.ddos.TcpSynFloodAttack");
            std::cout << "[TcpSynFloodAttack] ✓ Registered successfully" << std::endl;
        }
    };
    static TcpSynRegistrar registrar;
}

namespace ddosimu5g {

Define_Module(TcpSynFloodAttack);

TcpSynFloodAttack::~TcpSynFloodAttack() {
}

void TcpSynFloodAttack::initialize(int stage) {
    BaseAttackApp::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {      
        // Use destPort from BaseAttackApp (set from JSON profile)
        // This ensures the port from attack profile JSON is used
        if (destPort > 0 && destPort < 65536) {
            targetPorts.push_back(destPort);
        } else {
            // Fallback: Try to parse from NED parameter if destPort not set
            const char* portsParam = par("targetPorts").stringValue();
            if (strlen(portsParam) > 0) {
                cStringTokenizer tokenizer(portsParam);
                while (tokenizer.hasMoreTokens()) {
                    int port = atoi(tokenizer.nextToken());
                    if (port > 0 && port < 65536) {
                        targetPorts.push_back(port);
                    }
                }
            }
            // Last resort default
            if (targetPorts.empty()) {
                targetPorts.push_back(9998);  // Default to TCP SYN port 9998 if not specified
            }
        }
        
        currentSrcPort = 10000 + intrand(50000);  // Random starting port
        enableSpoofing = false;
        randomizeDestPort = false;

        EV << "[TcpSynFloodAttack] Configured: spoof=" << enableSpoofing 
           << ", targetPort=" << (targetPorts.empty() ? 0 : targetPorts[0]) << endl;
        std::cout << "[TcpSynFloodAttack] Configured: spoof=" << enableSpoofing 
           << ", targetPort=" << (targetPorts.empty() ? 0 : targetPorts[0]) << std::endl;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Get local address
        localAddress = getLocalAddress();

        if (localAddress.isUnspecified()) {
            EV_ERROR << "[TcpSynFloodAttack] Failed to get local address" << endl;
            std::cerr << "[TcpSynFloodAttack] ERROR: Failed to get local address" << std::endl;
            return;
        }
        
        // Get cellular interface ID for MessageDispatcher routing
        IInterfaceTable* ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        NetworkInterface* cellularIf = ift->findInterfaceByName("cellular");
        if (cellularIf) {
            cellularInterfaceId = cellularIf->getInterfaceId();
            std::cout << "[TcpSynFloodAttack] Found cellular interface ID: " << cellularInterfaceId << std::endl;
        } else {
            EV_ERROR << "[TcpSynFloodAttack] Failed to find cellular interface" << endl;
            std::cerr << "[TcpSynFloodAttack] ERROR: Failed to find cellular interface" << std::endl;
            return;
        }
        
        // Dynamically connect rawOut to MessageDispatcher (nl)
        cModule* ueModule = getParentModule();
        cModule* nlModule = ueModule->getSubmodule("nl");
        if (nlModule && gate("rawOut")) {
            cGate* rawOut = gate("rawOut");
            cGate* nlIn = nlModule->getOrCreateFirstUnconnectedGate("in", 0, false, true);
            rawOut->connectTo(nlIn);
            std::cout << "[TcpSynFloodAttack] Connected rawOut --> nl.in[" << nlIn->getIndex() << "]" << std::endl;
            // Also connect MessageDispatcher output to our rawIn so we can observe/drop replies
            if (gate("rawIn")) {
                cGate* nlOut = nlModule->getOrCreateFirstUnconnectedGate("out", 0, false, true);
                cGate* rawIn = gate("rawIn");
                nlOut->connectTo(rawIn);
                std::cout << "[TcpSynFloodAttack] Connected nl.out[" << nlOut->getIndex() << "] --> rawIn" << std::endl;
            }
        } else {
            EV_ERROR << "[TcpSynFloodAttack] Failed to connect to MessageDispatcher" << endl;
            std::cerr << "[TcpSynFloodAttack] ERROR: Failed to connect rawOut to nl" << std::endl;
            return;
        }
        
        EV << "[TcpSynFloodAttack] Ready. Local: " << localAddress 
           << ", Target: " << destAddress << endl;
        std::cout << "[TcpSynFloodAttack] Ready. Local: " << localAddress 
          << ", Target: " << destAddress << std::endl;
    }
}

void TcpSynFloodAttack::startAttack() {
    if (!isConfigured || destAddress.isUnspecified()) {
        EV_ERROR << "[TcpSynFloodAttack] Cannot start - invalid configuration" << endl;
        return;
    }
    
    if (localAddress.isUnspecified()) {
        localAddress = getLocalAddress();
        if (localAddress.isUnspecified()) {
            EV_ERROR << "[TcpSynFloodAttack] Cannot start - no local address" << endl;
            return;
        }
    }
    std::cout << "[TcpSynFloodAttack] Starting SYN flood via MessageDispatcher routing" << std::endl;
    
    BaseAttackApp::startAttack();
    attackStartTime = simTime();
    sendSynPacket();
    scheduleNextAttack();
}

void TcpSynFloodAttack::stopAttack() {
    std::cout << "[TcpSynFloodAttack] Stopping attack via MessageDispatcher" << std::endl;
    BaseAttackApp::stopAttack();
}

void TcpSynFloodAttack::sendAttackPacket() {
    if (!isActive) {
        EV << "[TcpSynFloodAttack] sendAttackPacket called but not active" << endl;
        std::cerr << "[TcpSynFloodAttack] ERROR: sendAttackPacket called but not active" << std::endl;  
        return;
    }
    sendSynPacket();
}

void TcpSynFloodAttack::handleMessageWhenUp(cMessage* msg) {
    // If a raw packet arrived from the MessageDispatcher (nl -> rawIn), inspect and optionally drop
    if (msg && msg->getArrivalGate() == gate("rawIn")) {
        Packet* pkt = check_and_cast<Packet*>(msg);
        const auto& ipHdr = pkt->peekAtFront<Ipv4Header>();
        if (ipHdr && ipHdr->getProtocolId() == IP_PROT_TCP) {
            // Duplicate packet cheaply for safe parsing without modifying the original
            Packet* copy = pkt->dup();
            auto ip = copy->removeAtFront<Ipv4Header>();
            if (ip && ip->getProtocolId() == IP_PROT_TCP) {
                auto tcp = copy->removeAtFront<tcp::TcpHeader>();
                bool isSynAck = tcp->getSynBit() && tcp->getAckBit();
                bool isRst = tcp->getRstBit();
                if (isSynAck || isRst) {
                    EV_INFO << "[TcpSynFloodAttack] Dropping TCP reply (SYN-ACK/RST) from " << ip->getSrcAddress() << endl;
                    // Log the received reply as benign before dropping
                    int pktSize = (int)pkt->getByteLength();
                    logAttackPacket("RX", "TCP",
                                    L3Address(ip->getSrcAddress()), (int)tcp->getSourcePort(),
                                    L3Address(ip->getDestAddress()), (int)tcp->getDestPort(),
                                    pktSize,
                                    false, "", "benign");

                    delete copy;
                    delete pkt; // consume the original
                    return;
                }
            }
            delete copy;
        }
        // Not a reply of interest -> fall through to default handler
    }

    BaseAttackApp::handleMessageWhenUp(msg);
}

void TcpSynFloodAttack::sendSynPacket() {
   
    // Create packet name
    char packetName[64];
    sprintf(packetName, "TcpSyn-%d", packetsSent);
    Packet* packet = new Packet(packetName);
    
    // Get addresses and ports
    L3Address srcAddr = enableSpoofing ? generateSpoofedSource() : localAddress;
    int srcPort = getNextSrcPort();
    int dstPort = getTargetPort();
    
    // ========================================
    // STEP 1: Create IPv4 Header (20 bytes)
    // ========================================
    const auto& ipHeader = makeShared<Ipv4Header>();
    ipHeader->setVersion(4);
    ipHeader->setHeaderLength(B(20));  // No IP options
    ipHeader->setTypeOfService(AttackMarkers::TOS_TCP_SYN_FLOOD);  // 0xD2 — TCP SYN flood marker
    ipHeader->setMoreFragments(false);
    ipHeader->setDontFragment(true);
    ipHeader->setFragmentOffset(0);
    ipHeader->setTimeToLive(64);
    ipHeader->setProtocolId(IP_PROT_TCP);  // Protocol = TCP (6)
    ipHeader->setSrcAddress(srcAddr.toIpv4());
    ipHeader->setDestAddress(destAddress.toIpv4());
    ipHeader->setTotalLengthField(B(40));  // 20 (IP) + 20 (TCP) = 40 bytes
    ipHeader->setCrcMode(CRC_COMPUTED);
    ipHeader->updateCrc();
    
    // ========================================
    // STEP 2: Create TCP Header (20 bytes)
    // ========================================
    const auto& tcpHeader = makeShared<tcp::TcpHeader>();
    tcpHeader->setSourcePort(srcPort);
    tcpHeader->setDestPort(dstPort);
    tcpHeader->setSequenceNo(intrand(0x7FFFFFFF));
    tcpHeader->setAckNo(0);
    tcpHeader->setSynBit(true);   // SYN flag ON
    tcpHeader->setAckBit(false);
    tcpHeader->setFinBit(false);
    tcpHeader->setRstBit(false);
    tcpHeader->setPshBit(false);
    tcpHeader->setUrgBit(false);
    tcpHeader->setWindow(65535);
    tcpHeader->setHeaderLength(B(20));  // TCP_MIN_HEADER_LENGTH = 20 bytes
    tcpHeader->setCrc(0);
    tcpHeader->setCrcMode(CRC_COMPUTED);

    // Compute TCP checksum for SYN (no payload). Use INET helper to
    // compute CRC on pseudo-header + TCP header. This avoids leaving
    // checksum zero and doesn't require pushing the packet through
    // the full stack here.
    {
        Packet tcpPayload("tcpPayload");
        // no payload inserted -> empty payload
        tcp::TcpCrcInsertionHook::insertCrc(&Protocol::ipv4, srcAddr, destAddress, tcpHeader, &tcpPayload);
    }
    
    // ========================================
    // STEP 3: Assemble packet [IP][TCP]
    // ========================================
    packet->insertAtBack(ipHeader);
    packet->insertAtBack(tcpHeader); 

    // ========================================
    // STEP 4: Add INET tags for MessageDispatcher routing
    // ========================================   
    // InterfaceReq - tells MessageDispatcher to use cellular interface
    packet->addTag<InterfaceReq>()->setInterfaceId(cellularInterfaceId);

    // ========================================
    // STEP 5: Send via rawOut to MessageDispatcher
    // ========================================
    // std::cout << "[TcpSynFloodAttack] Injecting SYN: " 
    //           << srcAddr << ":" << srcPort << " -> " 
    //           << destAddress << ":" << dstPort << std::endl;
    
    // Send via rawOut (connected to MessageDispatcher nl.in++)
    send(packet, "rawOut");
    
    //std::cout << "[TcpSynFloodAttack] Packet sent via rawOut to MessageDispatcher " << std::endl;
    
    // Log the attack
    logAttackPacket("TX", "TCP", localAddress, srcPort, 
                    destAddress, dstPort, 40, 
                    enableSpoofing, srcAddr.str().c_str());
    
    char extra[64];
    sprintf(extra, "(spoof=%d)", enableSpoofing ? 1 : 0);
    printProgress("TCP-SYN", extra);
}

L3Address TcpSynFloodAttack::getLocalAddress() {
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
        std::cerr << "[TcpSynFloodAttack] ERROR: No valid IPv4 address fond!" << std::endl;
    } 
    
    return addr;
}

L3Address TcpSynFloodAttack::generateSpoofedSource() {
    // Generate random IP address for spoofing
    uint32_t randomIp = intrand(0x7FFFFFFF);
    return Ipv4Address(randomIp);
}

int TcpSynFloodAttack::getNextSrcPort() {
    int port = currentSrcPort++;
    if (currentSrcPort > 60000) {
        currentSrcPort = 10000;  // Wrap around
    }
    return port;
}

int TcpSynFloodAttack::getTargetPort() {
    if (randomizeDestPort && !targetPorts.empty()) {
        return targetPorts[intrand(targetPorts.size())];
    }
    return targetPorts.empty() ? 80 : targetPorts[0];
}

void TcpSynFloodAttack::finish() {

    simtime_t elapsed = simTime() - attackStartTime;
    double rate = elapsed > 0 ? packetsSent / elapsed.dbl() : 0;    

    EV << "[TcpSynFloodAttack] Attack finished:" << endl;
    EV << "  SYN packets sent: " << packetsSent << endl;
    EV << "  Target: " << destAddress << endl;
    EV << "  Ports attacked: " << targetPorts.size() << endl;
    EV << "  Avg SYN rate: " << rate << " /s" << endl;

    std::cout << "[TcpSynFloodAttack] Attack finished:" << std::endl;
    std::cout << "  SYN packets sent: " << packetsSent << std::endl;
    std::cout << "  Target: " << destAddress << std::endl;
    std::cout << "  Ports attacked: " << targetPorts.size() << std::endl;
    std::cout << "  Avg SYN rate: " << rate << " /s" << std::endl;
    
    recordScalar("avgSynRate", rate);
    recordScalar("portsAttacked", (long)targetPorts.size());
    
    BaseAttackApp::finish();
}

} // namespace ddosimu5g
