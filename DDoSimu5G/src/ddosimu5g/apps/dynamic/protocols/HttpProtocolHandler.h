//
// HttpProtocolHandler.h - HTTP protocol handler
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

#ifndef __DDOSIMU5G_HTTPPROTOCOLHANDLER_H
#define __DDOSIMU5G_HTTPPROTOCOLHANDLER_H

#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include <iostream>

#include "ddosimu5g/apps/dynamic/DynamicTrafficSender.h"
#include "ddosimu5g/apps/dynamic/protocols/TcpProtocolHandler.h"

namespace ddosimu5g {

/**
 * @brief HTTP protocol handler for request-response patterns
 * 
 * Extends TcpProtocolHandler with HTTP-specific behavior:
 * - requestLength: HTTP request size (GET/POST)
 * - replyLength: Expected HTTP response size
 * - numRequests: Number of HTTP requests to send
 * - requestInterval: Time between requests
 */
class HttpProtocolHandler : public TcpProtocolHandler {
public:
    HttpProtocolHandler(DynamicTrafficSender* parent,
                       std::shared_ptr<TrafficLabeler> labeler,
                       int appId);
    
    virtual ~HttpProtocolHandler() = default;
    
    // Override protocol name
    std::string getProtocolName() const override { return "HTTP"; }
    
    // Start with HTTP-specific configuration
    void start(const nlohmann::json& config) override;
    
    // Override to label HTTP responses (not generic TCP)
    void socketDataArrived(inet::TcpSocket* socket, inet::Packet* packet, bool urgent) override;

protected:
    int replyLength;  // Expected HTTP response size
    
    // HTTP request details for realistic traffic
    std::string httpMethod;           // "GET" or "POST"
    std::vector<std::string> requestPaths;  // Request paths
    std::string userAgent;            // Browser/device identifier
    std::string contentType;          // "application/json", "text/html"
    int currentPathIndex = 0;         // Round-robin path selection
    
    // Override to use "HTTP" protocol name in logging
    void sendRequest() override;
    
    // Helper to get next path
    std::string getNextPath();
};

} // namespace ddosimu5g

#endif
