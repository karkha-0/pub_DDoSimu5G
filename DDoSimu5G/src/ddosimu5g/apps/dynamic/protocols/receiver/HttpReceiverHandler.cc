//
// HttpReceiverHandler.cc - HTTP receiver handler implementation
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

#include "ddosimu5g/apps/dynamic/protocols/receiver/HttpReceiverHandler.h"

#include <iostream>
#include "ddosimu5g/apps/dynamic/DynamicTrafficReceiver.h"

namespace ddosimu5g {

HttpReceiverHandler::HttpReceiverHandler(DynamicTrafficReceiver* parent, int port)
    : TcpReceiverHandler(parent, port, true) {
    
    // HTTP responses are typically much larger than requests
    replyMultiplier = 10;  // HTTP response 10x request size
    
    std::cout << "[HttpReceiverHandler] Listening on port " << port 
              << " (HTTP mode, reply multiplier=" << replyMultiplier << ")" << std::endl;
}

void HttpReceiverHandler::sendReply(inet::TcpSocket* socket, inet::Packet* request) {
    // HTTP-specific reply handling
    // In future, could parse HTTP headers and build proper HTTP response
    // For now, use larger multiplier to simulate HTTP response size
    
    int requestLength = request->getByteLength();
    int responseLength = requestLength * replyMultiplier;  // HTTP: large response
    
    inet::Packet* response = new inet::Packet("HttpResponse");
    const auto& payload = inet::makeShared<inet::ByteCountChunk>(inet::B(responseLength));
    response->insertAtBack(payload);
    response->addTag<inet::CreationTimeTag>()->setCreationTime(omnetpp::simTime());
    
    // CRITICAL: Add SocketReq tag for proper routing
    response->addTag<inet::SocketReq>()->setSocketId(socket->getSocketId());
    
    //std::cout << "[HttpReceiverHandler] Port " << port << " sending HTTP response ("
    //          << responseLength << " bytes for " << requestLength << " byte request), socket "
    //          << socket->getSocketId() << std::endl;
    
    socket->send(response);
    
    packetsSent++;
    
    EV_INFO << "HttpReceiverHandler sent HTTP response (" << responseLength << " bytes) on port " 
            << port << ", socket " << socket->getSocketId() << std::endl;
}

} // namespace ddosimu5g
