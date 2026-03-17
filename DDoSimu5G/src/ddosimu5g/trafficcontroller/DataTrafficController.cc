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

#include "DataTrafficController.h"

using namespace ddosimu5g;

Define_Module(DataTrafficController);

/**
 * @brief Constructor
 */
DataTrafficController::DataTrafficController() {
    EV << "DataTrafficController constructor ended at time: " << simTime() << "\n";
}

/**
 * @brief Destructor
 */
DataTrafficController::~DataTrafficController() {
    EV << "DataTrafficController Deconstructor ended at time: " << simTime() << "\n";
}

/**
 * @brief Initialize module
 */
void DataTrafficController::initialize() {
    // Register signals
    infectionTimeSignal_ = registerSignal("infectionEventTime");
    attackInstantiatedSignal = registerSignal("attackInstantiated");
    attackStartedSignal = registerSignal("attackStarted");
    attackStoppedSignal = registerSignal("attackStopped");
    
    // Enable setting for traffic controller behaviour
    attackSlotStart = par("attackSlotStart").intValue();
    attackSlotEnd = par("attackSlotEnd").intValue();

    std::cout << "[DataTrafficController]:  Attack slot range: " << attackSlotStart << " to " << attackSlotEnd << std::endl;

    enableTrafficMod = par("enableTrafficMod").boolValue();
    trafficMod_pktSize = par("trafficMod_pktSize").intValue();
    trafficMod_dataRate = par("trafficMod_dataRate").doubleValue();
    
    // Check if attack apps are enabled
    bool enableAttackApps = par("enableAttackApps").boolValue();
    
    // Only load infection data if attack apps or traffic mod is enabled
    if (enableAttackApps || enableTrafficMod) {
        const char *filePath = par("infectionFilePath").stringValue();
        
        // Check if file path is actually provided
        if (filePath && strlen(filePath) > 0) {
            parseInfectionData(filePath);
            scheduleInfectionEvents();
            
            EV_INFO << "DataTrafficController: Infection schedule loaded from " << filePath << endl;
            std::cout << "DataTrafficController: Infection schedule loaded from " << filePath << std::endl;
        } else {
            throw cRuntimeError("DataTrafficController: enableAttackApps or enableTrafficMod is enabled, but infectionFilePath parameter is empty!");
        }
    } else {
        EV_INFO << "DataTrafficController: Both enableAttackApps and enableTrafficMod disabled. Skipping infection file loading." << endl;
        std::cout << "DataTrafficController: Both enableAttackApps and enableTrafficMod disabled. Skipping infection file loading." << std::endl;
    }
}

/**
 * @brief Parse infection data from JSON file
 * @param filePath Path to the infection data JSON file
 */
void DataTrafficController::parseInfectionData(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open())
        throw cRuntimeError("Unable to open infection data file: %s", filePath.c_str());
    
    file >> infectionData;
    EV << "Parsed infection data with " << infectionData["infectionData"].size() << " entries.\n";
    
    for (const auto &entry : infectionData["infectionData"]) {
        if (!entry.contains("malware_active_time") || !entry.contains("node_id")) {
            throw cRuntimeError("Invalid JSON structure in infection data.");
        }
        
        EV << "Node ID: " << entry["node_id"]
           << ", Malware Active Time: " << entry["malware_active_time"]
           << "\n";
    }
}

/**
 * @brief Schedule infection events based on parsed infection data
 */
void DataTrafficController::scheduleInfectionEvents() {
    for (const auto &entry : infectionData["infectionData"]) {
        simtime_t time = simtime_t(entry["malware_active_time"].get<double>());
        int nodeId = entry["node_id"];
        cMessage *msg = new cMessage("infectionEventTime");
        msg->setKind(nodeId); // Use kind to store nodeId
        scheduleAt(time, msg);
        
        if (time <= simTime()) {
            throw cRuntimeError("DataTrafficController Message scheduled in the past: %f <= %f", time.dbl(), simTime().dbl());
        }
    }
}

/**
 * @brief Handle incoming messages (infection events)
 * @param msg Pointer to the received message
 */
void DataTrafficController::handleMessage(omnetpp::cMessage *msg) {
    if (msg->isSelfMessage()) {
        int nodeId = msg->getKind();
        
        EV_INFO << "Processing infection event for UE[" << nodeId << "] at time " 
                << simTime() << endl;
        
        // Emit signal for infection
        emit(infectionTimeSignal_, nodeId);
        
        // Get mode flags from NED parameters
        bool enableAttackApps = par("enableAttackApps").boolValue();
        
        if (enableAttackApps) {
            // TC-002 MODE: Instantiate attack application
            EV_INFO << "Instantiating attack app for UE[" << nodeId << "]" << endl;
            
            try {
                instantiateAttackApp(nodeId, simTime());
                EV_INFO << "Successfully instantiated attack for UE[" << nodeId << "]" << endl;
            } catch (const std::exception& e) {
                EV_ERROR << "[DataTrafficController] Failed to instantiate attack app for UE[" << nodeId 
                         << "]: " << e.what() << endl;
            }
            
        } else if (enableTrafficMod) {
            // TC-001 MODE: Modify existing CBR traffic
            EV_INFO << "Modifying CBR traffic for UE[" << nodeId << "]" << endl;
            updateTraffic(nodeId);
            EV_INFO << "Update traffic is enabled. Entered ddosTraffic for nodeId: " << nodeId << endl;
            std::cout << "Update traffic is enabled. Enter ddosTraffic for nodeId" << std::endl;
            
        } else {
            // STATISTICS-ONLY MODE: No traffic changes
            EV_INFO << "Statistics-only mode: Infection recorded but no traffic changes for UE[" 
                    << nodeId << "]" << endl;
            std::cout << "Update DDoS traffic is disabled. Skipping ddosTraffic for nodeId" << std::endl;
        }
        
        delete msg;
    }
}

/**
 * @brief Finish method called at simulation end
 */
void DataTrafficController::finish() {
    EV << "DataTrafficController Simulation ended at time: " << simTime() << "\n";
}

/**
 * @brief Update traffic parameters for CBR sender (TC-001 legacy mode)
 * @param nodeId Node ID of the UE to update
 */
void DataTrafficController::updateTraffic(int nodeId) {
    // Construct the path to the CbrSender application
    std::string basePath = getParentModule()->getFullPath();
    std::string modulePath = basePath + ".cbrUe[" + std::to_string(nodeId) + "].app[0]";
    
    std::cout << "Module path: " << modulePath << std::endl;
    std::cout << "DataTrafficController - Processing self-message for nodeId: " << nodeId << std::endl;
    
    cModule *appModule = getModuleByPath(modulePath.c_str());
    if (!appModule)
        throw cRuntimeError("[DataTrafficController] CbrSender module not found for nodeId: %d", nodeId);
    
    // Dynamically update parameters
    appModule->par("PacketSize").setIntValue(trafficMod_pktSize);
    appModule->par("sampling_time").setDoubleValue(trafficMod_dataRate);
    
    EV_INFO << "Update traffic paket size:" << trafficMod_pktSize << endl;
    std::cout << "Update traffic paket size:" << trafficMod_pktSize << std::endl;
    
    EV_INFO << "Update traffic paket size:" << trafficMod_pktSize << endl;
    std::cout << "Update traffic paket size:" << trafficMod_pktSize << std::endl;
    
    // Notify the module to reinitialize these parameters
    cMessage *updateMsg = new cMessage("updateParams");
    sendDirect(updateMsg, appModule, "controlIn");
    std::cout << "Scheduled updateParams message for module: " << modulePath << std::endl;
}

/**
 * @brief Instantiate attack application for infected UE
 * @param ueIndex Index of the infected UE
 * @param infectionTime Simulation time when infection occurred
 */
void DataTrafficController::instantiateAttackApp(int ueIndex, simtime_t infectionTime) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "[DataTrafficController] Instantiating attack for UE[" << ueIndex << "]" << std::endl;
    std::cout << "  Infection Time: " << infectionTime << std::endl;
    std::cout << "========================================" << std::endl;
    
    // =====================================================
    // STEP 1: GET NETWORK AND UE MODULE
    // =====================================================
    cModule *network = getParentModule();
    if (!network) {
        throw cRuntimeError("Parent network module not found!");
    }
    
    const char* ueModuleName = par("ueModuleName").stringValue();
    cModule *ueModule = network->getSubmodule(ueModuleName, ueIndex);
    
    if (!ueModule) {
        throw cRuntimeError("UE module not found for index %d", ueIndex);
    }
    
    std::cout << "[DataTrafficController] Found UE module: " << ueModule->getFullPath() << std::endl;
    
    // =====================================================
    // STEP 2: FIND BASEATTACKAPP PLACEHOLDER
    // =====================================================
    std::cout << "[DataTrafficController] Searching for BaseAttackApp placeholder..." << std::endl;
    
    int targetSlot = -1;
    cModule *placeholderApp = nullptr;
    const char* profileFile = nullptr;
    
    for (int slot = attackSlotStart; slot <= attackSlotEnd; slot++) {
        cModule *existingApp = ueModule->getSubmodule("app", slot);
        
        if (!existingApp) {
            std::cerr << "  [ERROR] app[" << slot << "] doesn't exist!" << std::endl;
            continue;
        }
        
        const char* typeName = existingApp->getComponentType()->getName();
        
        if (strcmp(typeName, "BaseAttackApp") == 0) {
            double startTime = existingApp->par("startTime").doubleValue();
            
            if (startTime < 0) {
                targetSlot = slot;
                placeholderApp = existingApp;
                
                // Get attack profile file from placeholder
                if (existingApp->hasPar("attackProfileFile")) {
                    profileFile = existingApp->par("attackProfileFile").stringValue();
                }
                
                std::cout << " - Found unused BaseAttackApp placeholder in app[" << slot << "]" << std::endl;
                if (profileFile && strlen(profileFile) > 0) {
                    std::cout << "   Profile file: " << profileFile << std::endl;
                }
                break;
            }
        }
    }
    
    if (targetSlot < 0 || !placeholderApp) {
        std::cerr << "[DataTrafficController] ERROR: No available BaseAttackApp placeholders for UE[" << ueIndex << "]" << std::endl;
        throw cRuntimeError("No available BaseAttackApp placeholders for UE[%d]", ueIndex);
    }
    
    if (!profileFile || strlen(profileFile) == 0) {
        std::cerr << "[DataTrafficController] ERROR: Placeholder for UE[" << ueIndex << "] app[" << targetSlot 
                  << "] has no attackProfileFile parameter!" << std::endl;
        throw cRuntimeError("Placesocket.setDscp(52);holder for UE[%d] app[%d] has no attackProfileFile parameter!", ueIndex, targetSlot);
    }
    

    // =====================================================
    // STEP 3: SAVE GATE CONNECTIONS
    // =====================================================
    cGate *savedSocketInSource = nullptr;
    cGate *savedSocketOutDest = nullptr;
    
    if (placeholderApp->hasGate("socketIn")) {
        cGate *socketIn = placeholderApp->gate("socketIn");
        if (socketIn->isConnected() && socketIn->getPathStartGate()) {
            savedSocketInSource = socketIn->getPathStartGate();
        }
    }
    
    if (placeholderApp->hasGate("socketOut")) {
        cGate *socketOut = placeholderApp->gate("socketOut");
        if (socketOut->isConnected() && socketOut->getNextGate()) {
            savedSocketOutDest = socketOut->getNextGate();
        }
    }
    
    std::cout << "[DataTrafficController] Gate connections saved" << std::endl;
    
    // =====================================================
    // STEP 4: DELETE THE PLACEHOLDER MODULE
    // =====================================================
    std::cout << "[DataTrafficController] Deleting BaseAttackApp placeholder..." << std::endl;
    placeholderApp->callFinish();
    placeholderApp->deleteModule();
    std::cout << "[DataTrafficController] Placeholder deleted" << std::endl;
    
    // =====================================================
    // STEP 5: CREATE NEW ATTACK MODULE USING FACTORY
    // =====================================================
    std::cout << "[DataTrafficController] Calling BaseAttackApp::createFromProfile()..." << std::endl;
    std::cout << "  Profile: " << profileFile << std::endl;
    std::cout << "  Slot: " << targetSlot << std::endl;
    
    BaseAttackApp* attackApp = nullptr;
    int benignSlotEnd = attackSlotStart - 1;
    
    try {
        attackApp = BaseAttackApp::createFromProfile(profileFile, ueModule, targetSlot, benignSlotEnd);
    } catch (const std::exception& e) {
        std::cerr << "[DataTrafficController] ERROR in createFromProfile(): " << e.what() << std::endl;
        throw cRuntimeError("Failed to create attack app from profile: %s", e.what());
    }
    
    if (!attackApp) {
        throw cRuntimeError("BaseAttackApp::createFromProfile() returned nullptr!");
    }

    std::cout << "[DataTrafficController] - Attack app created: " << attackApp->getFullPath() << std::endl;
    

    // =====================================================
    // STEP 6: SET TIMING PARAMETER (ONLY!)
    // =====================================================
    // Note: All other parameters (attackType, destAddress, rates, etc.) 
    // are set by BaseAttackApp from the JSON profile
    
    // The timing will be calculated by BaseAttackApp from JSON dormantDuration + attackDuration
    // But we set startTime to trigger initialization
    attackApp->par("startTime") = infectionTime.dbl();
    
    std::cout << "[DataTrafficController] Attack timing set:" << std::endl;
    std::cout << "  Start (infection): " << infectionTime << std::endl;
    std::cout << "  Stop: Will be calculated by BaseAttackApp from JSON" << std::endl;
    
    // =====================================================
    // STEP 7: FINALIZE THE ATTACK MODULE
    // =====================================================
    std::cout << "[DataTrafficController] Calling finalizeParameters()..." << std::endl;
    
    try {
        attackApp->finalizeParameters();
        std::cout << "[DataTrafficController] - finalizeParameters() completed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DataTrafficController] ERROR in finalizeParameters(): " << e.what() << std::endl;
        if (attackApp) {
            attackApp->deleteModule();
            attackApp = nullptr;
        }
        throw;
    }
    
    std::cout << "[DataTrafficController] Calling buildInside()..." << std::endl;
    
    try {
        attackApp->buildInside();
        std::cout << "[DataTrafficController] - buildInside() completed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DataTrafficController] ERROR in buildInside(): " << e.what() << std::endl;
        if (attackApp) {
            attackApp->deleteModule();
            attackApp = nullptr;
        }
        throw;
    }

    std::cout << "[DataTrafficController] Attack module created: " << attackApp->getFullPath() << std::endl;
    
    // =====================================================
    // STEP 8: RECONNECT GATES
    // =====================================================
    if (savedSocketInSource && attackApp->hasGate("socketIn")) {
        savedSocketInSource->connectTo(attackApp->gate("socketIn"));
        std::cout << "  - Reconnected socketIn" << std::endl;
    }
    
    if (savedSocketOutDest && attackApp->hasGate("socketOut")) {
        attackApp->gate("socketOut")->connectTo(savedSocketOutDest);
        std::cout << "  - Reconnected socketOut" << std::endl;
    }
    
    // =====================================================
    // STEP 9: INITIALIZE THE ATTACK MODULE
    // =====================================================
    std::cout << "[DataTrafficController] Initializing attack module..." << std::endl;
    
    try {
        attackApp->callInitialize();
        std::cout << "[DataTrafficController] - Attack module initialized successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DataTrafficController] ERROR during attack module initialization: " << e.what() << std::endl;
        if (attackApp) {
            attackApp->deleteModule();
            attackApp = nullptr;
        }
        throw cRuntimeError("Failed to initialize attack module: %s", e.what());
    }

    std::cout << "  Attack module initialized" << std::endl;
    
    // Track assignment
    ueToAttackSlotMap[ueIndex] = targetSlot;
    emit(attackInstantiatedSignal, ueIndex);
    
    std::cout << "[DataTrafficController] SUCCESS! Created attack app in app[" << targetSlot 
              << "] for UE[" << ueIndex << "]" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    EV_INFO << "Successfully instantiated attack app in app[" << targetSlot 
            << "] for UE[" << ueIndex << "]" << endl;
}
