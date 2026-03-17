//
// DataTrafficController - Orchestrates DDoS attack simulation timing
//
// Class developed by EIT, Lund University, Karim Khalil PhD
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

#ifndef SRC_DATATRAFFICCONTROLLER_H_
#define SRC_DATATRAFFICCONTROLLER_H_

#include <omnetpp.h>
#include "nlohmann/json.hpp"
#include <fstream>
#include <map>

#include "inet/networklayer/common/L3AddressResolver.h"
#include "../apps/adversarialApps/BaseAttackApp.h"

using namespace omnetpp;
using json = nlohmann::json;

/**
 * @brief DataTrafficController orchestrates DDoS attack simulation timing
 * This class is now a simple orchestrator that:
 * 1. Reads when infections should happen (from infectionData.json)
 * 2. Parse infection schedule JSON
 * 3. At infection time, calls BaseAttackApp::createFromProfile()
 * 4. Sets timing parameters and initializes the created attack app
 * 5. Handle legacy CBR traffic modification (TC-001 mode)
 */
class DataTrafficController : public cSimpleModule {
private:
    /**
     * @brief Slot range for attack applications
     */
    const int ATTACK_SLOT_START = 10; ///< First index for attack app placeholders
    const int ATTACK_SLOT_END = 19;   ///< Last index for attack app placeholders
    int attackSlotStart;  // Read from NED parameter
    int attackSlotEnd;    // Read from NED parameter
    
    /**
     * @brief Infection schedule data from JSON
     */
    nlohmann::json infectionData;
    
    /**
     * @brief Map UE index to attack slot for tracking
     */
    std::map<int, int> ueToAttackSlotMap;
    
    /**
     * @brief Instantiate attack application for infected UE
     * @param ueIndex Index of the infected UE
     * @param infectionTime Simulation time when infection occurred
     * 
     * This method:
     * 1. Finds BaseAttackApp placeholder module in slots 10-19
     * 2. Reads attackProfileFile parameter from placeholder
     * 3. Deletes placeholder
     * 4. Calls BaseAttackApp::createFromProfile() factory
     * 5. Sets startTime parameter (infection + dormant duration)
     * 6. Finalizes and initializes the created attack module
     */
    void instantiateAttackApp(int ueIndex, simtime_t infectionTime);

protected:
    // Signals
    omnetpp::simsignal_t infectionTimeSignal_;       ///< Signal emitted when infection occurs
    omnetpp::simsignal_t attackInstantiatedSignal;   ///< Signal emitted when attack app created
    omnetpp::simsignal_t attackStartedSignal;        ///< Signal emitted when attack starts
    omnetpp::simsignal_t attackStoppedSignal;        ///< Signal emitted when attack stops
    
    // Legacy CBR modification parameters (TC-001 mode)
    bool enableTrafficMod = false;  ///< Enable legacy CBR traffic modification
    int trafficMod_pktSize = 512;   ///< Packet size for CBR modification (bytes)
    double trafficMod_dataRate = 1; ///< Sampling rate for CBR modification (seconds)
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    /**
     * @brief Parse infection data from JSON file
     * @param filePath Path to the infection data JSON file
     */
    void parseInfectionData(const std::string &filePath);
    
    /**
     * @brief Schedule infection events based on parsed infection data
     * 
     * Creates self-messages for each infection event scheduled at
     * the specified malware_active_time.
     */
    void scheduleInfectionEvents();
    
    /**
     * @brief Update traffic parameters for CBR sender (TC-001 legacy mode)
     * @param nodeId Node ID of the UE to update
     * 
     * Legacy method for modifying existing CBR application parameters.
     * Only used when enableTrafficMod=true and enableAttackApps=false.
     */
    void updateTraffic(int nodeId);

public:
    DataTrafficController();
    virtual ~DataTrafficController();
};

#endif // SRC_DATATRAFFICCONTROLLER_H_
