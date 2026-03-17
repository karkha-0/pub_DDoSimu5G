//
// DynamicTrafficReceiver.h - Dynamic multi-protocol traffic receiver
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

#ifndef APPS_DYNAMIC_DYNAMICTRAFFICRECEIVER_H_
#define APPS_DYNAMIC_DYNAMICTRAFFICRECEIVER_H_

#include <omnetpp.h>
#include "inet/applications/base/ApplicationBase.h"
#include <map>
#include <memory>
#include "ddosimu5g/apps/dynamic/protocols/receiver/ReceiverProtocolHandler.h"

using namespace omnetpp;
using namespace inet;

namespace ddosimu5g {

// Forward declarations
class ReceiverProtocolHandler;

class DynamicTrafficReceiver : public ApplicationBase {

protected:
    // Protocol handlers (modular, port-based routing)
    std::map<int, ReceiverProtocolHandler*> handlers;  // port -> handler
    
    // Configuration
    bool echoMode;  // Echo received data back
    
    // Signals
    simsignal_t packetReceivedSignal;
    simsignal_t packetSentSignal;

protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    
    // Setup protocol handlers
    void setupProtocolHandlers();

public:
    DynamicTrafficReceiver();
    virtual ~DynamicTrafficReceiver();
};

} // namespace ddosimu5g

#endif
