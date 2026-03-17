//
// DynamicTrafficSender.h - Dynamic multi-protocol traffic generator
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

#ifndef APPS_DYNAMIC_DYNAMICTRAFFICSENDER_H_
#define APPS_DYNAMIC_DYNAMICTRAFFICSENDER_H_

#include <omnetpp.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <ctime>
#include "nlohmann/json.hpp"
#include <map>
#include <memory>
#include <string>
#include "inet/applications/base/ApplicationBase.h"
#include "inet/common/socket/SocketTag_m.h"

#include "ddosimu5g/apps/dynamic/protocols/ProtocolHandler.h"
#include "ddosimu5g/apps/dynamic/trafficlabel/TrafficLabeler.h"
#include "ddosimu5g/apps/dynamic/protocols/DnsProtocolHandler.h"
#include "ddosimu5g/apps/dynamic/protocols/HttpProtocolHandler.h"
#include "ddosimu5g/apps/dynamic/protocols/TcpProtocolHandler.h"
#include "ddosimu5g/apps/dynamic/protocols/UdpProtocolHandler.h"

using namespace omnetpp;
using namespace inet;

namespace ddosimu5g {

/**
 * @brief Dynamic traffic orchestrator with modular protocol handlers
 * 
 * This module acts as a container that instantiates and manages protocol
 * handlers based on JSON traffic schedules. Each UE has 10 app instances
 * (app[0-9]), and each app runs one protocol handler assigned via app_id.
 * 
 * Architecture:
 * - TrafficLabeler: Thread-safe CSV logging (shared across all apps per UE)
 * - ProtocolHandler: Base interface for all protocols
 * - UdpProtocolHandler: UDP traffic generation
 * - DnsProtocolHandler: DNS query-response
 * - TcpProtocolHandler: TCP connection and data transfer
 * - HttpProtocolHandler: HTTP request-response
 * 
 * To add new protocols (MQTT, CoAP, HTTPS, etc.):
 * 1. Create new handler class extending ProtocolHandler
 * 2. Add case in createProtocolHandler()
 * 3. Update JSON with new type and app_id
 */
class DynamicTrafficSender : public ApplicationBase {
public:
    DynamicTrafficSender() : currentHandler(nullptr), id(0), appId(0) {}
    virtual ~DynamicTrafficSender();

    // Access for protocol handlers
    int getId() const { return id; }
    std::string getFullPath() const override { return ApplicationBase::getFullPath(); }
    omnetpp::cGate* gate(const char* name) { return ApplicationBase::gate(name); }
    void scheduleAt(simtime_t t, cMessage* msg) override { ApplicationBase::scheduleAt(t, msg); }
    cMessage* cancelEvent(cMessage* msg) override { return ApplicationBase::cancelEvent(msg); }
    void cancelAndDelete(cMessage* msg) override { ApplicationBase::cancelAndDelete(msg); }

protected:
    // OMNeT++ lifecycle
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    
    // ApplicationBase lifecycle
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

private:
    // Module parameters
    int id;                                        // Module ID
    std::string trafficScheduleFile;               // JSON schedule path
    std::string labelFilePath;                     // CSV label file path
    
    // Traffic labeling
    std::shared_ptr<TrafficLabeler> labeler;      // Thread-safe CSV logger
    
    // Protocol handling
    ProtocolHandler* currentHandler;               // Current active handler
    
    // Traffic schedule
    nlohmann::json trafficSchedule;               // Parsed JSON schedule
    int appId;                                     // App index from module (0-9)
    
    // Helper methods
    void parseTrafficSchedule();                   // Load and parse JSON
    ProtocolHandler* createProtocolHandler(const std::string& type);  // Factory method
    void startTrafficFromSchedule();               // Start traffic for this app_id
};

} // namespace ddosimu5g

#endif /* APPS_DYNAMIC_DYNAMICTRAFFICSENDER_H_ */
