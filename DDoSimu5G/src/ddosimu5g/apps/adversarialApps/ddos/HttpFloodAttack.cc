//
// HttpFloodAttack - HTTP flood DDoS attack implementation
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

#include "HttpFloodAttack.h"


using namespace ddosimu5g;

namespace {
    struct HttpFloodRegistrar {
        HttpFloodRegistrar() {
            AttackRegistry::registerAttack("http_flood", 
                "ddosimu5g.apps.adversarialApps.ddos.HttpFloodAttack");
            std::cout << "[HttpFloodAttack] ✓ Registered successfully" << std::endl;
        }
    };
    static HttpFloodRegistrar registrar;
}

namespace ddosimu5g {

Define_Module(HttpFloodAttack);

HttpFloodAttack::~HttpFloodAttack() {
}

void HttpFloodAttack::initialize(int stage) {
    BaseAttackApp::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        // Read HTTP flood specific parameters
        // Override from JSON config if available
        if (!config.attackType.empty()) {
            numConnections = config.numConnections;
            httpMethod = config.httpMethod;
            userAgent = config.userAgent;
            keepAlive = config.keepAlive;
            contentLength = config.contentLength;
            
            // Parse request paths from JSON
            cStringTokenizer tokenizer(config.requestPaths.c_str(), ",");
            while (tokenizer.hasMoreTokens()) {
                requestPaths.push_back(tokenizer.nextToken());
            }
            
            std::cout << "[HttpFloodAttack] Using JSON config:" << std::endl;
            std::cout << "  - Connections: " << numConnections << std::endl;
            std::cout << "  - Method: " << httpMethod << std::endl;
            std::cout << "  - Paths: " << requestPaths.size() << std::endl;
        } else {
            // Fallback to NED parameters (if JSON not used)
            numConnections = par("numConnections");
            httpMethod = par("httpMethod").stdstringValue();
            userAgent = par("userAgent").stdstringValue();
            contentLength = par("contentLength");
            keepAlive = par("keepAlive");
            
            // Parse request paths from NED
            const char* pathsParam = par("requestPaths").stringValue();
            cStringTokenizer tokenizer(pathsParam, ",");
            while (tokenizer.hasMoreTokens()) {
                requestPaths.push_back(tokenizer.nextToken());
            }
        }
        
        if (requestPaths.empty()) {
            requestPaths.push_back("/");  // Default to root
        }
        
        connectTimer = new cMessage("establishConnections");
        
        EV << "[HttpFloodAttack] Configured: " << currentRate << " req/s, "
           << numConnections << " connections, method=" << httpMethod << endl;
        std::cout << "[HttpFloodAttack] Configured: " << currentRate << " req/s, "
                  << numConnections << " connections, method=" << httpMethod << std::endl;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Get local address
        localAddress = getLocalAddress();
        
        EV << "[HttpFloodAttack] Ready. Local: " << localAddress 
           << ", Target: " << destAddress << ":" << destPort << endl;
        std::cout << "[HttpFloodAttack] Ready. Local: " << localAddress 
                  << ", Target: " << destAddress << ":" << destPort << std::endl;
    }
}

void HttpFloodAttack::startAttack() {
    if (!isConfigured || destAddress.isUnspecified()) {
        EV_ERROR << "[HttpFloodAttack] Cannot start - invalid configuration" << endl;
        return;
    }
    
    EV << "[HttpFloodAttack] Starting HTTP flood attack" << endl;
    EV << "  Target: " << destAddress << ":" << destPort << endl;
    EV << "  Method: " << httpMethod << endl;
    EV << "  Connections: " << numConnections << endl;
    EV << "  Request rate: " << currentRate << " req/s" << endl;
    EV << "  Keep-Alive: " << (keepAlive ? "yes" : "no") << endl;
    
    std::cout << "[HttpFloodAttack] Starting flood:" << std::endl;
    std::cout << "  Target: " << destAddress << ":" << destPort << std::endl;
    std::cout << "  Connections: " << numConnections << " x IoT device" << std::endl;
    std::cout << "  Rate: " << currentRate << " req/s (distributed across connections)" << std::endl;
    std::cout << "  Keep-Alive: " << (keepAlive ? "enabled" : "disabled") << std::endl;
    std::cout << "  Method: " << httpMethod << " (IoT telemetry pattern)" << std::endl;
    
    // Call parent to initialize timers and state
    BaseAttackApp::startAttack();
    
    // Establish TCP connections first
    scheduleAt(simTime(), connectTimer);
}

void HttpFloodAttack::stopAttack() {
    EV << "[HttpFloodAttack] Stopping HTTP flood. Sent " << packetsSent 
       << " packets (" << bytesSent << " bytes)" << endl;
    std::cout << "[HttpFloodAttack] Stopping HTTP flood. Sent " << packetsSent 
              << " packets (" << bytesSent << " bytes)" << std::endl;

    // Call parent FIRST - it will cancel timers and set isActive=false
    BaseAttackApp::stopAttack();
    
    // Subclass cleanup - close all connections
    for (auto socket : connections) {
        socket->close();
        socketMap.removeSocket(socket);
        delete socket;
    }
    connections.clear();
    connectionsEstablished = 0;
    
    cancelAndDelete(connectTimer);
    connectTimer = nullptr;
}

void HttpFloodAttack::sendAttackPacket() {
    if (!isActive || connectionsEstablished < numConnections) {
        return;
    }
    
    sendHttpRequest();
}

void HttpFloodAttack::handleMessageWhenUp(cMessage* msg) {
    if (msg == connectTimer) {
        establishConnections();
    }
    else if (msg->isSelfMessage()) {
        BaseAttackApp::handleMessageWhenUp(msg);
    }
    else {
        // Handle socket messages
        TcpSocket* socket = check_and_cast_nullable<TcpSocket*>(
            socketMap.findSocketFor(msg));
        
        if (socket) {
            socket->processMessage(msg);
        } else {
            // Message for unknown socket - likely already deleted/renewed
            EV_WARN << "[HttpFloodAttack] Received message for unknown socket (discarding)" << endl;
            delete msg;
        }
    }
}

void HttpFloodAttack::establishConnections() {
    EV << "[HttpFloodAttack] Establishing " << numConnections 
       << " TCP connections..." << endl;
    std::cout << "[HttpFloodAttack] Establishing " << numConnections 
              << " TCP connections..." << std::endl;
    
    for (int i = 0; i < numConnections; i++) {
        TcpSocket* socket = new TcpSocket();
        socket->setOutputGate(gate("socketOut"));
        socket->bind(localAddress, -1);  // Use ephemeral port
        short dscp = (AttackMarkers::TOS_HTTP_FLOOD >> 2) & 0x3F;
        socket->setDscp(dscp);
        socket->setTimeToLive(64);  // Match benign TTL (INET default is 32)
        
        // Set callbacks
        socket->setCallback(this);
        
        // Connect to target
        socket->connect(destAddress, destPort);
        
        connections.push_back(socket);
        socketMap.addSocket(socket);
        
        EV_DETAIL << "[HttpFloodAttack] Initiating connection #" << i << endl;
        std::cout << "[HttpFloodAttack] Initiating connection #" << i << std::endl;
    }
}

void HttpFloodAttack::socketEstablished(TcpSocket* socket) {
    // Only increment if this socket wasn't already established
    if (establishedSockets.find(socket) == establishedSockets.end()) {
        establishedSockets.insert(socket);
        connectionsEstablished++;
    }

    // Log the TCP 3-way handshake packets that INET's TCP stack
    // already sent on our behalf (SYN + completing ACK).
    // Without these entries the label CSV under-counts vs. the PCAP,
    // because pcap_to_csv flow-labels every packet on the 5-tuple.
    logAttackPacket("TX", "TCP_SYN",       localAddress, socket->getLocalPort(),
                    destAddress, destPort, 40);   // SYN  (no payload)
    logAttackPacket("TX", "TCP_ACK_HS",    localAddress, socket->getLocalPort(),
                    destAddress, destPort, 40);   // completing ACK

    EV << "[HttpFloodAttack] Connection established (" 
       << connectionsEstablished << "/" << numConnections << ")" << endl;
    std::cout << "[HttpFloodAttack] Connection established (" 
              << connectionsEstablished << "/" << numConnections << ")" << std::endl;
    
    // Start sending requests once all connections are established
    if (connectionsEstablished >= numConnections && !sendTimer->isScheduled()) {
        EV << "[HttpFloodAttack] All connections ready. Starting HTTP flood." << endl;
        std::cout << "[HttpFloodAttack] All connections ready. Starting HTTP flood." << std::endl;
        scheduleNextAttack();  // Use BaseAttackApp's scheduling
    }
}

void HttpFloodAttack::socketDataArrived(TcpSocket* socket, Packet* packet, bool urgent) {
    // Consume and discard HTTP responses
    // EV_DETAIL << "[HttpFloodAttack] Received " << packet->getByteLength() 
    //           << " bytes response (discarded)" << endl;
    // std::cout << "[HttpFloodAttack] Received response (" << packet->getByteLength() 
    //           << " bytes, discarded)" << std::endl;
    
    // Label incoming HTTP responses before discarding
    
    // For TCP, socket info is most reliable - always use it
    // (Tags may be absent for certain packet types, but socket always knows connection state)
    L3Address srcAddr = socket->getRemoteAddress();  // Server address
    L3Address dstAddr = socket->getLocalAddress();   // Our address
    int srcPort = socket->getRemotePort();           // Server port
    int dstPort = socket->getLocalPort();            // Our port
    
    // Log RX packet as benign (server response to our attack traffic)
    logAttackPacket("RX", "HTTP_RESPONSE", srcAddr, srcPort, 
                   dstAddr, dstPort, packet->getByteLength(),
                   false, "", "benign");  // Last parameter = benign
    
    delete packet;
}

void HttpFloodAttack::socketClosed(TcpSocket* socket) {
    EV_WARN << "[HttpFloodAttack] Connection closed" << endl;

    // Log the FIN-ACK that INET's TCP stack sent to close the connection.
    logAttackPacket("TX", "TCP_FIN",  localAddress, socket->getLocalPort(),
                    destAddress, destPort, 40);

    // Only decrement if this socket was actually established
    if (establishedSockets.find(socket) != establishedSockets.end()) {
        establishedSockets.erase(socket);
        connectionsEstablished--;
        std::cout << "[HttpFloodAttack] Connection closed (established connections: " 
                  << connectionsEstablished << "/" << numConnections << ")" << std::endl;
        
        // Reconnect to maintain connection pool (was previously working)
        if (isActive && connectionFailures < 50) {  // Limit reconnect attempts
            EV << "[HttpFloodAttack] Reconnecting..." << endl;
            socket->renewSocket();
            socket->bind(localAddress, -1);
            socket->connect(destAddress, destPort);
        }
    } else {
        // Socket closed before being established (connection failed)
        std::cout << "[HttpFloodAttack] Connection closed before establishment (never connected)" << std::endl;
    }
}

void HttpFloodAttack::socketFailure(TcpSocket* socket, int code) {
    connectionFailures++;
    
    EV_ERROR << "[HttpFloodAttack] Connection failed (code " << code << ")" << endl;
    std::cout << "[HttpFloodAttack] Connection FAILED (code " << code 
              << ", total failures: " << connectionFailures << ")" << std::endl;
    
    // Remove from established set if it was there
    if (establishedSockets.find(socket) != establishedSockets.end()) {
        establishedSockets.erase(socket);
        connectionsEstablished--;
    }
    
    // Stop reconnecting after too many failures (likely server unavailable)
    if (connectionFailures >= 20) {
        EV_WARN << "[HttpFloodAttack] Too many failures (" << connectionFailures 
                << "). Target likely unavailable. Stopping attack." << endl;
        std::cout << "[HttpFloodAttack] ERROR: Too many connection failures. "
                  << "Target server may not exist or port not listening." << std::endl;
        isActive = false;
    } else if (isActive && connectionFailures < 10) {
        // Only retry a few times for initial failures
        EV << "[HttpFloodAttack] Retrying connection..." << endl;
        socket->renewSocket();
        socket->bind(localAddress, -1);
        socket->connect(destAddress, destPort);
    }
}

void HttpFloodAttack::sendHttpRequest() {
    TcpSocket* socket = getAvailableConnection();
    if (!socket) {
        EV_WARN << "[HttpFloodAttack] No available connection for request" << endl;
        std::cerr << "[HttpFloodAttack] No available connection for request" << std::endl;
        return;
    }
    
    // Create HTTP request packet
    Packet* packet;
    if (httpMethod == "GET") {
        packet = createHttpGetRequest();
    } else if (httpMethod == "POST") {
        packet = createHttpPostRequest();
    } else {
        EV_ERROR << "[HttpFloodAttack] Unknown HTTP method: " << httpMethod << endl;
        std::cerr << "[HttpFloodAttack] Unknown HTTP method: " << httpMethod << std::endl;
        return;
    }
    
    // Send request
    socket->send(packet);
    
    // Log attack packet
    logAttackPacket("TX", "HTTP", localAddress, socket->getLocalPort(),
                    destAddress, destPort, packet->getByteLength());

    // ONE LINE with extra info
    char extra[128];
    sprintf(extra, "(%s %s)", httpMethod.c_str(), getNextPath().c_str());
    printProgress("HTTP", extra);
}

Packet* HttpFloodAttack::createHttpGetRequest() {
    std::string path = getNextPath();
    
    // Build HTTP GET request
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << destAddress.str() << "\r\n";
    request << "User-Agent: " << userAgent << "\r\n";
    request << "Accept: */*\r\n";
    
    if (keepAlive) {
        request << "Connection: keep-alive\r\n";
    } else {
        request << "Connection: close\r\n";
    }
    
    request << "\r\n";
    
    // Create packet
    char packetName[64];
    sprintf(packetName, "HttpGet-%d", packetsSent);
    Packet* packet = new Packet(packetName);
    
    std::string reqStr = request.str();
    int reqSize = reqStr.length();
    const auto& payload = makeShared<ByteCountChunk>(B(reqSize));
    packet->insertAtBack(payload);
    
    return packet;
}

Packet* HttpFloodAttack::createHttpPostRequest() {
    std::string path = getNextPath();
    
    // Build HTTP POST request
    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n";
    request << "Host: " << destAddress.str() << "\r\n";
    request << "User-Agent: " << userAgent << "\r\n";
    request << "Content-Type: application/x-www-form-urlencoded\r\n";
    request << "Content-Length: " << contentLength << "\r\n";
    
    if (keepAlive) {
        request << "Connection: keep-alive\r\n";
    } else {
        request << "Connection: close\r\n";
    }
    
    request << "\r\n";
    
    // Create packet
    char packetName[64];
    sprintf(packetName, "HttpPost-%d", packetsSent);
    Packet* packet = new Packet(packetName);
    
    // Calculate total size (headers + body)
    std::string reqStr = request.str();
    int totalSize = reqStr.length() + contentLength;
    
    const auto& payload = makeShared<ByteCountChunk>(B(totalSize));
    packet->insertAtBack(payload);
    
    return packet;
}

TcpSocket* HttpFloodAttack::getAvailableConnection() {
    // Round-robin selection of available connections
    for (size_t i = 0; i < connections.size(); i++) {
        TcpSocket* socket = connections[i];
        if (socket->getState() == TcpSocket::CONNECTED) {
            return socket;
        }
    }
    return nullptr;
}

std::string HttpFloodAttack::getNextPath() {
    std::string path = requestPaths[currentPathIndex];
    currentPathIndex = (currentPathIndex + 1) % requestPaths.size();
    return path;
}

L3Address HttpFloodAttack::getLocalAddress() {
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
        std::cerr << "[HttpFloodAttack] ERROR: No valid IPv4 address found!" << std::endl;
    } 
    
    return addr;
}

void HttpFloodAttack::finish() {
    double avgRequestSize = (packetsSent > 0) ? 
        (double)bytesSent / packetsSent : 0;
    
    EV << "[HttpFloodAttack] Attack finished:" << endl;
    EV << "  Requests sent: " << packetsSent << endl;
    EV << "  Bytes sent: " << bytesSent << endl;
    EV << "  Avg request size: " << avgRequestSize << " bytes" << endl;
    EV << "  Connections used: " << connections.size() << endl;
    EV << "  Method: " << httpMethod << endl;
    std::cout << "[HttpFloodAttack] Attack finished:" << std::endl;
    std::cout << "  Requests sent: " << packetsSent << std::endl;
    std::cout << "  Bytes sent: " << bytesSent << std::endl;
    std::cout << "  Avg request size: " << avgRequestSize << " bytes" << std::endl;
    std::cout << "  Connections used: " << connections.size() << std::endl;
    std::cout << "  Method: " << httpMethod << std::endl;

    recordScalar("httpRequestsSent", packetsSent);
    recordScalar("avgRequestSize", avgRequestSize);
    recordScalar("numConnections", connections.size());
    
    BaseAttackApp::finish();
}

} // namespace ddosimu5g
