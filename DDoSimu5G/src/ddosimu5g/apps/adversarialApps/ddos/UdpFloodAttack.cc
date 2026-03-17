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

#include "UdpFloodAttack.h"

using namespace ddosimu5g;

// =====================================================
// SELF-REGISTRATION: Register this attack type
// =====================================================
namespace {
    struct UdpFloodRegistrar {
        UdpFloodRegistrar() {
            try {
                AttackRegistry::registerAttack("udp_flood", "ddosimu5g.apps.adversarialApps.ddos.UdpFloodAttack");
                std::cout << "[UdpFloodAttack] ✓ Registered successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[UdpFloodAttack] ERROR during registration: " << e.what() << std::endl;
            }
        }
    };
    static UdpFloodRegistrar registrar;
}

namespace ddosimu5g {

Define_Module(UdpFloodAttack);

UdpFloodAttack::~UdpFloodAttack() {
}

void UdpFloodAttack::initialize(int stage) {
    BaseAttackApp::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        // Read UDP flood specific parameters
        packetSize = par("packetSize");
        packetsPerSecond = par("packetsPerSecond");
        enableSpoofing = par("enableSpoofing");
        jitter = par("jitter");
        localPort = par("localPort");

        // Override with JSON config if available
        if (!config.attackType.empty()) {
            packetSize = config.packetSize;
            enableSpoofing = config.enableSpoofing;
            
            std::cout << "[UdpFloodAttack] Overriding from JSON: packetSize=" 
                      << packetSize << ", spoofing=" << enableSpoofing << std::endl;
        }
        
        EV << "[UdpFloodAttack] Configured: " << packetsPerSecond << " pps, "
           << packetSize << " bytes, spoof=" << enableSpoofing << endl;
        std::cout << "[UdpFloodAttack] Configured: " << packetsPerSecond << " pps, "
                  << packetSize << " bytes, spoof=" << enableSpoofing << std::endl;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Get local address
        localAddress = getLocalAddress();
        
        // Bind socket
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localAddress, localPort);
        short dscp = (AttackMarkers::TOS_UDP_FLOOD >> 2) & 0x3F;
        socket.setDscp(dscp);
        socket.setTimeToLive(64);  // Match benign TTL (INET default is 32)
        
        EV << "[UdpFloodAttack] Socket bound to " << localAddress 
           << ":" << localPort << endl;
        std::cout << "[UdpFloodAttack] Socket bound to " << localAddress 
                  << ":" << localPort << std::endl;
    }
}

void UdpFloodAttack::startAttack() {
    if (!isConfigured) {
        EV_ERROR << "[UdpFloodAttack] Cannot start - not configured" << endl;
        std::cerr << "[UdpFloodAttack] ERROR: Cannot start - not configured" << std::endl;
        return;
    }
    
    EV << "[UdpFloodAttack] Starting UDP flood: " << destAddress 
       << ":" << destPort << " @ " << currentRate << " pps" << endl;
    std::cout << "[UdpFloodAttack] Starting UDP flood: " << destAddress 
              << ":" << destPort << " @ " << currentRate << " pps" << std::endl;

    // Call parent - it will create sendTimer and set up style system
    BaseAttackApp::startAttack();
    
    // Send first packet immediately
    sendFloodPacket();
    
    // Schedule next packet using BaseAttackApp's method
    scheduleNextAttack();
    
    std::cout << "[UdpFloodAttack] First packet sent, next scheduled" << std::endl;
}

void UdpFloodAttack::stopAttack() {
    EV << "[UdpFloodAttack] Stopping UDP flood. Sent " << packetsSent 
       << " packets (" << bytesSent << " bytes)" << endl;
    std::cout << "[UdpFloodAttack] Stopping UDP flood. Sent " << packetsSent 
              << " packets (" << bytesSent << " bytes)" << std::endl;

    // Call parent FIRST - it will cancel timers and set isActive=false
    BaseAttackApp::stopAttack();
    
    // Subclass cleanup
    socket.close();
}

void UdpFloodAttack::sendAttackPacket() {
    
    if (!isActive) {
        EV << "[UdpFloodAttack] executeAttack called but not active" << endl;
        std::cout << "[UdpFloodAttack] executeAttack called but not active" << std::endl;
        return;
    }
    
    // Send packet
    sendFloodPacket();
}

void UdpFloodAttack::handleMessageWhenUp(cMessage* msg) {
    if (msg->arrivedOn("socketIn")) {
        // Label incoming UDP responses before discarding
        Packet* packet = dynamic_cast<Packet*>(msg);
        if (packet) {
            // Extract packet info for logging - use findTag for safety
            auto l3AddressInd = packet->findTag<L3AddressInd>();
            auto l4PortInd = packet->findTag<L4PortInd>();
            
            // Get addresses - from tags if available, otherwise from known attack config
            L3Address srcAddr = l3AddressInd ? l3AddressInd->getSrcAddress() : destAddress;
            L3Address dstAddr = l3AddressInd ? l3AddressInd->getDestAddress() : localAddress;
            
            // Get ports - from tags if available, otherwise from known attack config
            int srcPort = l4PortInd ? l4PortInd->getSrcPort() : destPort;
            int dstPort = l4PortInd ? l4PortInd->getDestPort() : localPort;
            
            // Log RX packet with best available info
            logAttackPacket("RX", "UDP_REPLY", srcAddr, srcPort, 
                          dstAddr, dstPort, packet->getByteLength(), 
                          false, "", "benign");
        }
        delete msg;
    } else {
        BaseAttackApp::handleMessageWhenUp(msg);
    }
}

void UdpFloodAttack::sendFloodPacket() {
    // Create UDP flood packet
    char packetName[64];
    sprintf(packetName, "UdpFlood-%d", packetsSent);
    Packet* packet = new Packet(packetName);
    
    // Add payload
    const auto& payload = makeShared<ByteCountChunk>(B(packetSize));
    packet->insertAtBack(payload);
    
    // Set spoofed source (if enabled)
    L3Address spoofedSrc = enableSpoofing ? generateSpoofedSource() : L3Address();
    
    emit(packetSentSignal, packetSize);
    emit(bytesSentSignal, (long)packetSize);

    // Send packet
    socket.sendTo(packet, destAddress, destPort);
    
    logAttackPacket("TX", "UDP", 
                    localAddress,                        // Real source IP (bot's IP)
                    localPort, 
                    destAddress, 
                    destPort, 
                    packetSize,
                    enableSpoofing,                      // Spoofing flag
                    spoofedSrc.str().c_str());           // Spoofed IP
    
    
    // ONE LINE - replaces 43 lines of logging code!
    char extra[64];
    sprintf(extra, "(spoof=%d)", enableSpoofing ? 1 : 0);
    printProgress("UDP", extra);
}

L3Address UdpFloodAttack::getLocalAddress() {
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
        std::cerr << "[UdpFloodAttack] ERROR: No valid IPv4 address found!" << std::endl;
    } 
    
    return addr;
}

L3Address UdpFloodAttack::generateSpoofedSource() {
    // Generate random IP address for spoofing
    uint32_t randomIp = intrand(0x7FFFFFFF);  // Avoid reserved ranges
    return Ipv4Address(randomIp);
}

void UdpFloodAttack::finish() {
    EV << "[UdpFloodAttack] Attack finished:" << endl;
    EV << "  Total packets: " << packetsSent << endl;
    EV << "  Total bytes: " << bytesSent << endl;
    EV << "  Avg rate: " << (packetsSent / duration.dbl()) << " pps" << endl;
    EV << "  Avg throughput: " << (bytesSent * 8 / duration.dbl() / 1e6) << " Mbps" << endl;
    
    recordScalar("avgPacketRate", packetsSent / duration.dbl());
    recordScalar("avgThroughputMbps", bytesSent * 8 / duration.dbl() / 1e6);
    
    BaseAttackApp::finish();
}

} // namespace ddosimu5g
