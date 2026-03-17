//
// TcpReceiverHandler.cc - TCP receiver handler implementation
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

#include "ddosimu5g/apps/dynamic/protocols/receiver/TcpReceiverHandler.h"

#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include <iostream>
#include "ddosimu5g/apps/dynamic/DynamicTrafficReceiver.h"

namespace ddosimu5g {

TcpReceiverHandler::TcpReceiverHandler(DynamicTrafficReceiver* parent, int port, bool echoMode)
    : ReceiverProtocolHandler(parent, port), echoMode(echoMode), replyMultiplier(10) {
    
    // Initialize and bind TCP server socket
    serverSocket.setOutputGate(parent->gate("socketOut"));
    serverSocket.bind(port);
    serverSocket.listen();
    serverSocket.setCallback(this);
    
    EV_INFO << "TcpReceiverHandler listening on port " << port << std::endl;
    std::cout << "[TcpReceiverHandler] Listening on port " << port 
              << " (echoMode=" << echoMode << ")" << std::endl;
}

TcpReceiverHandler::~TcpReceiverHandler() {
    std::cout << "[TcpReceiverHandler] DESTRUCTOR called for port " << port << std::endl;
    shutdownSockets();
    std::cout << "[TcpReceiverHandler] DESTRUCTOR finished for port " << port << std::endl;
}

void TcpReceiverHandler::shutdownSockets()
{
    std::cout << "[TcpReceiverHandler] SHUTDOWN: Starting socket cleanup for port " << port << std::endl;
    
    // 1) Stop server socket callbacks immediately
    serverSocket.setCallback(nullptr);
    std::cout << "[TcpReceiverHandler] SHUTDOWN: Server callback cleared for port " << port << std::endl;

    // 2) Clean up client sockets — just clear callback and delete.
    //    Do NOT call abort()/close() here: they send messages through gates
    //    that may already be torn down, and they trigger callbacks (socketClosed,
    //    socketDeleted) that modify clientSockets while we iterate it.
    std::cout << "[TcpReceiverHandler] SHUTDOWN: Cleaning " << clientSockets.size() 
              << " client sockets for port " << port << std::endl;
    for (auto& entry : clientSockets) {
        if (entry.second) {
            std::cout << "[TcpReceiverHandler] SHUTDOWN: Deleting socket " << entry.first << std::endl;
            entry.second->setCallback(nullptr);
            delete entry.second;
            entry.second = nullptr;
        }
    }
    clientSockets.clear();
    std::cout << "[TcpReceiverHandler] SHUTDOWN: All client sockets deleted for port " << port << std::endl;

    // 3) Do NOT call serverSocket.close() — same reason: gate may be gone.
    //    The TcpSocket destructor handles internal cleanup without sending messages.
    std::cout << "[TcpReceiverHandler] SHUTDOWN: Completed for port " << port << std::endl;
}

bool TcpReceiverHandler::processMessage(omnetpp::cMessage* msg) {
    // Check if message belongs to server socket
    if (serverSocket.belongsToSocket(msg)) {
        serverSocket.processMessage(msg);
        return true;
    }
    
    // Check if message belongs to any client socket
    for (auto& entry : clientSockets) {
        if (entry.second->belongsToSocket(msg)) {
            entry.second->processMessage(msg);
            return true;
        }
    }
    
    return false;
}

void TcpReceiverHandler::socketAvailable(inet::TcpSocket* socket, inet::TcpAvailableInfo* availableInfo) {
    // New connection request on server socket
    EV_INFO << "TcpReceiverHandler: New connection from " << availableInfo->getRemoteAddr() 
            << ":" << availableInfo->getRemotePort() << " on port " << port << std::endl;
    std::cout << "[TcpReceiverHandler] Port " << port << " accepting connection from " 
              << availableInfo->getRemoteAddr() << ":" << availableInfo->getRemotePort() << std::endl;
    
    // Create new socket for this client
    inet::TcpSocket* newSocket = new inet::TcpSocket(availableInfo);
    newSocket->setOutputGate(parent->gate("socketOut"));
    newSocket->setCallback(this);
    
    int socketId = newSocket->getSocketId();
    clientSockets[socketId] = newSocket;
    
    socket->accept(availableInfo->getNewSocketId());
    
    // Note: DO NOT delete availableInfo - TcpSocket constructor takes ownership
}

void TcpReceiverHandler::socketEstablished(inet::TcpSocket* socket) {
    EV_INFO << "TcpReceiverHandler: Connection established (socket " << socket->getSocketId() 
            << ") on port " << port << std::endl;
    std::cout << "[TcpReceiverHandler] Port " << port << " connection established, socket " 
              << socket->getSocketId() << std::endl;
}

void TcpReceiverHandler::socketDataArrived(inet::TcpSocket* socket, inet::Packet* packet, bool urgent) {
    packetsReceived++;
    
    EV_INFO << "TcpReceiverHandler received request (" << packet->getByteLength() 
            << " bytes) on port " << port << ", socket " << socket->getSocketId() << std::endl;
    //std::cout << "[TcpReceiverHandler] Port " << port << " received TCP data ("
    //          << packet->getByteLength() << " bytes) at t=" << omnetpp::simTime()
    //          << ", socket " << socket->getSocketId() << std::endl;
    
    if (echoMode) {
        sendReply(socket, packet);
    }
    
    delete packet;
}

void TcpReceiverHandler::sendReply(inet::TcpSocket* socket, inet::Packet* request) {
    // TCP reply is typically larger than request (for HTTP-like behavior)
    int replyLength = request->getByteLength() * replyMultiplier;
    
    inet::Packet* reply = new inet::Packet("TcpReply");
    const auto& payload = inet::makeShared<inet::ByteCountChunk>(inet::B(replyLength));
    reply->insertAtBack(payload);
    reply->addTag<inet::CreationTimeTag>()->setCreationTime(omnetpp::simTime());
    
    // CRITICAL: Add SocketReq tag for proper routing (learned from INET's TcpEchoApp)
    reply->addTag<inet::SocketReq>()->setSocketId(socket->getSocketId());
    
    // Set IP options: TTL=64, DF flag=1
    reply->addTag<inet::HopLimitReq>()->setHopLimit(64);
    reply->addTag<inet::FragmentationReq>()->setDontFragment(true);
    
    //std::cout << "[TcpReceiverHandler] Port " << port << " sending TCP reply ("
    //          << replyLength << " bytes), socket " << socket->getSocketId() << std::endl;
    
    socket->send(reply);
    
    packetsSent++;
    
    EV_INFO << "TcpReceiverHandler sent reply (" << replyLength << " bytes) on port " 
            << port << ", socket " << socket->getSocketId() << std::endl;
}

void TcpReceiverHandler::socketPeerClosed(inet::TcpSocket* socket) {
    EV_INFO << "TcpReceiverHandler: Peer closed connection (socket " << socket->getSocketId() 
            << ") on port " << port << std::endl;
    
    if (socket->getState() == inet::TcpSocket::PEER_CLOSED) {
        socket->close();
    }
}

void TcpReceiverHandler::socketClosed(inet::TcpSocket* socket) {
    EV_INFO << "TcpReceiverHandler: Connection closed (socket " << socket->getSocketId() 
            << ") on port " << port << std::endl;
    std::cout << "[TcpReceiverHandler] Port " << port << " connection closed, socket " 
              << socket->getSocketId() << std::endl;
    
    // Remove from map and delete
    auto it = clientSockets.find(socket->getSocketId());
    if (it != clientSockets.end()) {
        inet::TcpSocket* sockToDelete = it->second;
        clientSockets.erase(it);  // Remove from map first
        delete sockToDelete;      // Then delete (will trigger socketDeleted callback)
    }
}

void TcpReceiverHandler::socketFailure(inet::TcpSocket* socket, int code) {
    EV_WARN << "TcpReceiverHandler: Connection failed (socket " << socket->getSocketId() 
            << ") on port " << port << ", code=" << code << std::endl;
}

void TcpReceiverHandler::socketStatusArrived(inet::TcpSocket* socket, inet::TcpStatusInfo* status) {
    delete status;
}

void TcpReceiverHandler::socketDeleted(inet::TcpSocket* socket) {
    // Socket is being deleted (triggered by delete in socketClosed)
    // Just ensure it's removed from map - DON'T delete again
    std::cout << "[TcpReceiverHandler] Port " << port << " socketDeleted callback, socket " 
              << socket->getSocketId() << std::endl;
    
    auto it = clientSockets.find(socket->getSocketId());
    if (it != clientSockets.end()) {
        // Already removed from map in socketClosed, but check anyway
        clientSockets.erase(it);
    }
}

} // namespace ddosimu5g
