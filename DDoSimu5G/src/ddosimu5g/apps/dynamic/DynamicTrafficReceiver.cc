//
// DynamicTrafficReceiver - Modular receiver implementation
//

//
// DynamicTrafficReceiver - Dynamic multi-protocol traffic receiver
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

#include "ddosimu5g/apps/dynamic/DynamicTrafficReceiver.h"

#include <iostream>
#include "ddosimu5g/apps/dynamic/protocols/receiver/DnsReceiverHandler.h"
#include "ddosimu5g/apps/dynamic/protocols/receiver/HttpReceiverHandler.h"
#include "ddosimu5g/apps/dynamic/protocols/receiver/TcpReceiverHandler.h"
#include "ddosimu5g/apps/dynamic/protocols/receiver/UdpReceiverHandler.h"

namespace ddosimu5g {

Define_Module(DynamicTrafficReceiver);

DynamicTrafficReceiver::DynamicTrafficReceiver() {
}

DynamicTrafficReceiver::~DynamicTrafficReceiver() {
    for (auto& entry : handlers) {
        delete entry.second;
    }
    handlers.clear();
}

void DynamicTrafficReceiver::initialize(int stage) {
    ApplicationBase::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        // Read configuration
        echoMode = par("echoMode");

        // Initialize signals
        packetReceivedSignal = registerSignal("packetReceived");
        packetSentSignal = registerSignal("packetSent");
        
        EV_INFO << "DynamicTrafficReceiver initialized (echoMode=" << echoMode << ")" << endl;
        //std::cout << "[DynamicTrafficReceiver] Initialized at " << getFullPath()
        //          << " (echoMode=" << echoMode << ")" << std::endl;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Setup protocol handlers for different services
        setupProtocolHandlers();
    }
}

void DynamicTrafficReceiver::setupProtocolHandlers() {
    // Create protocol-specific handlers based on .ini configuration
    
    // UDP echo server (port 5000)
    if (hasPar("udpPort")) {
        int udpPort = par("udpPort");
        if (udpPort > 0) {
            handlers[udpPort] = new UdpReceiverHandler(this, udpPort, echoMode);
            EV_INFO << "Created UdpReceiverHandler on port " << udpPort << endl;
        }
    }
    
    // DNS server (port 53)
    if (hasPar("dnsPort")) {
        int dnsPort = par("dnsPort");
        if (dnsPort > 0) {
            handlers[dnsPort] = new DnsReceiverHandler(this, dnsPort);
            EV_INFO << "Created DnsReceiverHandler on port " << dnsPort << endl;
        }
    }
    
    // TCP server (port 5001)
    if (hasPar("tcpPort")) {
        int tcpPort = par("tcpPort");
        if (tcpPort > 0) {
            handlers[tcpPort] = new TcpReceiverHandler(this, tcpPort, echoMode);
            EV_INFO << "Created TcpReceiverHandler on port " << tcpPort << endl;
        }
    }
    
    // HTTP server (port 80)
    if (hasPar("httpPort")) {
        int httpPort = par("httpPort");
        if (httpPort > 0) {
            handlers[httpPort] = new HttpReceiverHandler(this, httpPort);
            EV_INFO << "Created HttpReceiverHandler on port " << httpPort << endl;
        }
    }
    
    //std::cout << "[DynamicTrafficReceiver] Setup complete: " << handlers.size()
    //          << " protocol handlers registered" << std::endl;
}

void DynamicTrafficReceiver::handleMessageWhenUp(cMessage *msg) {
    //std::cout << "[DynamicTrafficReceiver] **MESSAGE ARRIVED**: " << msg->getName()
    //          << " from gate " << msg->getArrivalGate()->getName() << std::endl;
    
    // Socket messages need to be processed by their respective handlers
    // The handlers have set callbacks, so processing the message will trigger the callback
    
    // Try to identify which handler should process this message
    // Socket messages arrive on socketIn gate
    if (msg->arrivedOn("socketIn")) {
        //std::cout << "[DynamicTrafficReceiver] Socket message arrived, attempting to route..." << std::endl;
        
        // Check each handler's socket to see if it can process this message
        bool handled = false;
        for (auto& entry : handlers) {
            ReceiverProtocolHandler* handler = entry.second;
            
            // Try UDP handlers
            UdpReceiverHandler* udpHandler = dynamic_cast<UdpReceiverHandler*>(handler);
            if (udpHandler) {
                //std::cout << "[DynamicTrafficReceiver] Trying UDP handler on port " << entry.first << std::endl;
                if (udpHandler->processMessage(msg)) {
                    //std::cout << "[DynamicTrafficReceiver] Message processed by UDP handler" << std::endl;
                    handled = true;
                    break;
                }
            }
            
            // Try DNS handlers
            DnsReceiverHandler* dnsHandler = dynamic_cast<DnsReceiverHandler*>(handler);
            if (dnsHandler) {
                //std::cout << "[DynamicTrafficReceiver] Trying DNS handler on port " << entry.first << std::endl;
                if (dnsHandler->processMessage(msg)) {
                    //std::cout << "[DynamicTrafficReceiver] Message processed by DNS handler" << std::endl;
                    handled = true;
                    break;
                }
            }
            
            // Try TCP handlers (including HTTP which extends TCP)
            TcpReceiverHandler* tcpHandler = dynamic_cast<TcpReceiverHandler*>(handler);
            if (tcpHandler) {
                //std::cout << "[DynamicTrafficReceiver] Trying TCP handler on port " << entry.first << std::endl;
                if (tcpHandler->processMessage(msg)) {
                    //std::cout << "[DynamicTrafficReceiver] Message processed by TCP handler" << std::endl;
                    handled = true;
                    break;
                }
            }
        }
        
        if (!handled) {
            EV_WARN << "Socket message not handled by any protocol handler" << endl;
            std::cout << "[DynamicTrafficReceiver] **WARNING**: No handler processed message!" << std::endl;
            delete msg;
        }
    } else {
        EV_WARN << "Unexpected message arrival" << endl;
        std::cout << "[DynamicTrafficReceiver] **WARNING**: Unexpected message arrival!" << std::endl;
        delete msg;
    }
}

void DynamicTrafficReceiver::handleStartOperation(LifecycleOperation *operation) {
    EV_INFO << "DynamicTrafficReceiver started" << endl;
    std::cout << "[DynamicTrafficReceiver] Started and listening" << std::endl;
}

void DynamicTrafficReceiver::handleStopOperation(LifecycleOperation *operation) {
    EV_INFO << "DynamicTrafficReceiver stopping" << endl;

    // Handlers will clean up their own sockets in destructors
}

void DynamicTrafficReceiver::handleCrashOperation(LifecycleOperation *operation) {
    // Clean shutdown
    for (auto& entry : handlers) {
        delete entry.second;
    }
    handlers.clear();
}

void DynamicTrafficReceiver::finish() {
    EV_INFO << "DynamicTrafficReceiver finishing..." << endl;
    
    // Print statistics from all handlers
    for (auto& entry : handlers) {
        ReceiverProtocolHandler* handler = entry.second;
        std::cout << "[DynamicTrafficReceiver] " << handler->getProtocolName()
                  << " port " << handler->getPort() << ": "
                  << handler->getPacketsReceived() << " received, "
                  << handler->getPacketsSent() << " sent" << std::endl;
    }
    
    ApplicationBase::finish();
}

} // namespace ddosimu5g
