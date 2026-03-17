//
// HttpProtocolHandler.cc - HTTP protocol handler implementation
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

#include "HttpProtocolHandler.h"

namespace ddosimu5g {

HttpProtocolHandler::HttpProtocolHandler(DynamicTrafficSender* parent,
                                         std::shared_ptr<TrafficLabeler> labeler,
                                         int appId)
    : TcpProtocolHandler(parent, labeler, appId), replyLength(0) {
}

void HttpProtocolHandler::socketDataArrived(inet::TcpSocket* socket, inet::Packet* packet, bool urgent) {
    packetsReceived++;
    
    EV_INFO << "HttpProtocolHandler[" << appId << "] received HTTP response #" 
            << packetsReceived << " (" << packet->getByteLength() << " bytes)" << std::endl;
    
    // Log HTTP response to CSV as benign traffic
    logPacketToCSV("HTTP_RESPONSE", destAddress, destPort, packet->getByteLength(), localPort, packet->getCreationTime());
    
    delete packet;
}

void HttpProtocolHandler::start(const nlohmann::json& config) {
    EV_INFO << "HttpProtocolHandler[" << appId << "] starting..." << std::endl;
    
    // Parse HTTP-specific configuration
    replyLength = config.value("replyLength", 1024);
    httpMethod = config.value("httpMethod", "GET");
    userAgent = config.value("userAgent", "Generic/1.0");
    contentType = config.value("contentType", "application/json");
    
    // Parse request paths (can be string or array)
    if (config.contains("requestPaths")) {
        if (config["requestPaths"].is_array()) {
            for (const auto& path : config["requestPaths"]) {
                requestPaths.push_back(path.get<std::string>());
            }
        } else if (config["requestPaths"].is_string()) {
            // Comma-separated paths
            std::string pathsStr = config["requestPaths"].get<std::string>();
            std::stringstream ss(pathsStr);
            std::string path;
            while (std::getline(ss, path, ',')) {
                // Trim whitespace
                path.erase(0, path.find_first_not_of(" \t"));
                path.erase(path.find_last_not_of(" \t") + 1);
                if (!path.empty()) {
                    requestPaths.push_back(path);
                }
            }
        }
    }
    
    if (requestPaths.empty()) {
        requestPaths.push_back("/");  // Default to root
    }
    
    // Call parent start (handles TCP connection and scheduling)
    TcpProtocolHandler::start(config);
    
    std::cout << "[HttpProtocolHandler] App[" << appId << "] starting HTTP traffic "
              << "(method=" << httpMethod << ", paths=" << requestPaths.size() 
              << ", agent=" << userAgent.substr(0, 30) << "...)" << std::endl;
}

void HttpProtocolHandler::sendRequest() {
    // Get next path (round-robin)
    std::string path = getNextPath();
    
    // Build realistic HTTP request
    std::ostringstream httpRequest;
    httpRequest << httpMethod << " " << path << " HTTP/1.1\r\n";
    httpRequest << "Host: " << destAddress << "\r\n";
    httpRequest << "User-Agent: " << userAgent << "\r\n";
    httpRequest << "Accept: */*\r\n";
    
    if (httpMethod == "POST") {
        int headerSize = httpRequest.str().length();
        int bodySize = (requestLength > headerSize + 50) ? (requestLength - headerSize - 50) : 0;
        
        httpRequest << "Content-Type: " << contentType << "\r\n";
        httpRequest << "Content-Length: " << bodySize << "\r\n";
        httpRequest << "Connection: keep-alive\r\n";
        httpRequest << "\r\n";
        // Body would go here (represented by total packet size)
    } else {
        httpRequest << "Connection: keep-alive\r\n";
        httpRequest << "\r\n";
    }
    
    // Create HTTP request packet
    char packetName[100];
    snprintf(packetName, sizeof(packetName), "HTTP-%s-app%d-req%d", 
             httpMethod.c_str(), appId, requestCount);
    inet::Packet* packet = new inet::Packet(packetName);
    
    // Use actual request size (ensure minimum matches HTTP headers)
    int actualSize = std::max((int)httpRequest.str().length(), requestLength);
    const auto& payload = inet::makeShared<inet::ByteCountChunk>(inet::B(actualSize));
    packet->insertAtBack(payload);
    packet->addTag<inet::CreationTimeTag>()->setCreationTime(omnetpp::simTime());
    
    // Set IP options: TTL=64, DF flag=1
    packet->addTag<inet::HopLimitReq>()->setHopLimit(64);
    packet->addTag<inet::FragmentationReq>()->setDontFragment(true);
    
    // Send request
    tcpSocket.send(packet);
    
    //logPacketToCSV("HTTP", packet, true);
    logPacketToCSV("HTTP", destAddress, destPort, actualSize, localPort, packet->getCreationTime());

    packetsSent++;
    requestCount++;
    
    // Log to CSV as HTTP (not TCP)
    //logPacketToCSV("HTTP", destAddress, destPort, actualSize, localPort);
    
    EV_INFO << "HttpProtocolHandler[" << appId << "] sent HTTP " << httpMethod 
            << " request #" << requestCount << " (" << path << ")" << std::endl;
    
    // Schedule next request:
    //  - keepAlive mode: keep going until endTime (ignore numRequests)
    //  - normal mode:    stop after numRequests OR endTime, whichever first
    bool moreToSend = keepAlive
                      ? (omnetpp::simTime() < endTime)
                      : (requestCount < numRequests && omnetpp::simTime() < endTime);
    if (moreToSend) {
        scheduleRequest();
    }
}

std::string HttpProtocolHandler::getNextPath() {
    std::string path = requestPaths[currentPathIndex];
    currentPathIndex = (currentPathIndex + 1) % requestPaths.size();
    return path;
}

} // namespace ddosimu5g
