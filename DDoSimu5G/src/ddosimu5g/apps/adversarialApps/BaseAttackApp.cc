//
// BaseAttackApp - Abstract base class for all DDoS attack implementations
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

#include "BaseAttackApp.h"

namespace ddosimu5g {

Define_Module(BaseAttackApp);

BaseAttackApp::BaseAttackApp() {
}

BaseAttackApp::~BaseAttackApp() {
    cancelAndDelete(startAttackTimer);
    cancelAndDelete(stopAttackTimer);
    cancelAndDelete(rateUpdateTimer);
    cancelAndDelete(burstToggleTimer);
    cancelAndDelete(sendTimer);
    cancelAndDelete(behaviorModeTimer);
}

BaseAttackApp* BaseAttackApp::createFromProfile(const char* profileFile, 
                                                cModule* parent, 
                                                int appIndex, 
                                                int benignSlotEndIndex) {
    std::cout << "\n[BaseAttackApp::createFromProfile] =================" << std::endl;
    std::cout << "  Profile: " << profileFile << std::endl;
    std::cout << "  Parent: " << parent->getFullPath() << std::endl;
    std::cout << "  Slot: " << appIndex << std::endl;
    std::cout << "  Benign slot end index: " << benignSlotEndIndex << std::endl;
    
    // =====================================================
    // STEP 1: Parse JSON to get attackType
    // =====================================================
    std::ifstream file(profileFile);
    if (!file.is_open()) {
        throw cRuntimeError("Cannot open attack profile file: %s", profileFile);
    }
    
    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        throw cRuntimeError("JSON parsing error in %s: %s", profileFile, e.what());
    }
    
    if (!json.contains("attackProfile") || !json["attackProfile"].contains("attackType")) {
        throw cRuntimeError("Missing attackType in profile file: %s", profileFile);
    }
    
    std::string attackType = json["attackProfile"]["attackType"].get<std::string>();
    std::cout << "  Attack Type: " << attackType << std::endl;
    
    // =====================================================
    // STEP 2: Get module name from registry or fallback
    // =====================================================
    std::string moduleName;
    
    if (AttackRegistry::isRegistered(attackType)) {
        moduleName = AttackRegistry::getModuleName(attackType);
        std::cout << "  - Found in registry: " << moduleName << std::endl;
    } else {
        std::cerr << "  -> No registered attack found for type: " << attackType << std::endl;
    }
    
    // =====================================================
    // STEP 3: Create module dynamically
    // =====================================================
    cModuleType* moduleType = cModuleType::find(moduleName.c_str());
    if (!moduleType) {
        throw cRuntimeError("Attack module type '%s' not found in OMNeT++ registry!", moduleName.c_str());
    }
    
    cModule* module = moduleType->create("app", parent, appIndex);
    if (!module) {
        throw cRuntimeError("Failed to create attack module!");
    }
    
    std::cout << "  - Created module: " << module->getFullPath() << std::endl;
    
    // =====================================================
    // STEP 4: Set attackProfileFile parameter
    // =====================================================
    module->par("attackProfileFile") = profileFile;
    module->par("benignSlotEnd") = benignSlotEndIndex;

    std::cout << "  - Set attackProfileFile parameter" << std::endl;
    std::cout << "  - Set benignSlotEnd = " << benignSlotEndIndex << std::endl;
    std::cout << "[BaseAttackApp::createFromProfile] Complete! =======\n" << std::endl;
    
    // Return as BaseAttackApp*
    BaseAttackApp* attackApp = dynamic_cast<BaseAttackApp*>(module);
    if (!attackApp) {
        throw cRuntimeError("Created module is not a BaseAttackApp!");
    }
    
    return attackApp;
}

void BaseAttackApp::initialize(int stage) {

    ApplicationBase::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        // Get UE and app identifiers
        appId = getIndex();
        ueId = getParentModule()->getIndex();

        attackStartTime = par("startTime");
        attackStopTime = par("stopTime");

        // Check if this is a placeholder (attackStartTime < 0)
        if (attackStartTime < 0) {
            std::cout << "[BaseAttackApp] Placeholder mode - skipping initialization" << std::endl;
            return;
        }

        std::cout << "[BaseAttackApp] Active attack app - startTime=" << attackStartTime 
                  << ", stopTime=" << attackStopTime << std::endl;
        
        // =====================================================
        // TRY TO LOAD ATTACK PROFILE FROM JSON FIRST
        // =====================================================
        const char* profileFile = par("attackProfileFile").stringValue();
        
        if (profileFile && strlen(profileFile) > 0) {
            std::cout << "[BaseAttackApp] UE[" << ueId << "].app[" << appId 
                      << "] Loading attack profile: " << profileFile << std::endl;
            
            try {
                config = parseAttackProfile(profileFile);
                
                // Copy to member variables for backward compatibility
                attackType = config.attackType;
                //ratePattern = config.ratePattern;
                initialRate = config.initialRate;
                peakRate = config.peakRate;
                rampDuration = config.rampDuration;
                burstMode = (config.burstOnDuration > 0 && config.burstOffDuration > 0);
                burstOnDuration = config.burstOnDuration;
                burstOffDuration = config.burstOffDuration;
                destAddressStr = config.targets.empty() ? "" : config.targets[0];
                destPort = config.targetPort;
                
                std::cout << "[BaseAttackApp] - Profile loaded: " << attackType 
                          << "/" << config.attackStyle << "/" << config.behaviorMode << std::endl;
                
                // Schedule behavior mode application at attack start time
                // (Will be set by DataTrafficController via startTime parameter)
                
            } catch (const std::exception& e) {
                std::cerr << "[BaseAttackApp] ERROR loading profile: " << e.what() << std::endl;
                throw cRuntimeError("Failed to load attack profile: %s", e.what());
            }
        } else {
            // =====================================================
            // FALLBACK: Read from NED parameters (backward compatible)
            // =====================================================
            std::cout << "[BaseAttackApp] No profile file - using NED parameters" << std::endl;
            
            attackType = par("attackType").stdstringValue();
            destAddressStr = par("destAddress").stdstringValue();
            destPort = par("destPort");
            //ratePattern = par("ratePattern").stdstringValue();
            initialRate = par("initialRate");
            peakRate = par("peakRate");
            rampDuration = par("rampDuration");
            burstMode = par("burstMode");
            burstOnDuration = par("burstOnDuration");
            burstOffDuration = par("burstOffDuration");
        }
        
        if (!config.attackType.empty()) {
            simtime_t infectionTime = attackStartTime;
            attackStartTime = infectionTime + config.dormantDuration;
            attackStopTime = attackStartTime + config.attackDuration;
            
            std::cout << "[BaseAttackApp] Attack timing calculated from JSON:" << std::endl;
            std::cout << "  Infection: " << infectionTime << std::endl;
            std::cout << "  Dormant: " << config.dormantDuration << "s" << std::endl;
            std::cout << "  Attack Start: " << attackStartTime << std::endl;
            std::cout << "  Attack Duration: " << config.attackDuration << "s" << std::endl;
            std::cout << "  Attack Stop: " << attackStopTime << std::endl;
        }
        
        // Read benignSlotEnd from parameter (set by DataTrafficController)
        if (hasPar("benignSlotEnd")) {
            benignSlotEnd = par("benignSlotEnd").intValue();
            std::cout << "[BaseAttackApp] benignSlotEnd set to: " << benignSlotEnd << std::endl;
        }
        
        // Initialize state
        currentRate = initialRate;
        inBurstOn = true;
        
        std::cout << "[BaseAttackApp] Initialized - Type: " << attackType
                  << ", Style: " << config.attackStyle
                  << ", Rate: " << initialRate << "->" << peakRate << " pps" << std::endl;
        
        // Register signals
        packetSentSignal = registerSignal("packetSent");
        bytesSentSignal = registerSignal("bytesSent");
        attackStartSignal = registerSignal("attackStart");
        attackStopSignal = registerSignal("attackStop");
        
        EV << "[BaseAttackApp] UE[" << ueId << "].attackApp[" << appId 
           << "] initialized - Type: " << attackType << endl;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        if (attackStartTime < 0) {
            std::cout << "[BaseAttackApp] Skipping application layer init (placeholder)" << std::endl;
            return;
        }
        
        // Configure attack (resolve addresses, etc.)
        configureAttack();
                
        // Try to subscribe to queue length signals at cellular interface
        cModule* ue = getParentModule();
        cModule* cellularNic = ue->getSubmodule("cellularNic");
        if (cellularNic) {
            // Try to find queue or mac module
            cModule* queue = cellularNic->getSubmodule("queue");
            if (!queue) {
                queue = cellularNic->getSubmodule("mac");  // Some NICs have queue in MAC
            }
            
            if (queue) {
                simsignal_t queueLengthSignal = registerSignal("queueLength");
                queue->subscribe(queueLengthSignal, this);
                std::cout << "[BaseAttackApp] Subscribed to queue monitoring for adaptive rate control" << std::endl;
            }
        }

        // Initialize TrafficLabeler
        std::string basePath = par("labelFilePath").stdstringValue();
        
        if (!basePath.empty()) {
            std::cout << "[BaseAttackApp] Base path from INI: " << basePath << std::endl;
            
            std::ostringstream filepath;
            filepath << basePath << "/labels/ue" << ueId << "/app" << appId << ".csv";
            
            labelFilePath = filepath.str();
            
            std::cout << "[BaseAttackApp] Constructed label file path: " 
                      << labelFilePath << std::endl;
            
            labeler = TrafficLabeler::getInstance(labelFilePath);
            
            if (!labeler || !labeler->isOpen()) {
                std::cerr << "[BaseAttackApp] ERROR: Failed to initialize labeler for: " 
                          << labelFilePath << std::endl;
            } else {
                std::cout << "[BaseAttackApp] - Labeler ready: " << labelFilePath << std::endl;
            }
        } else {
            std::cout << "[BaseAttackApp] No label file path configured - labeling disabled" << std::endl;
        }
        
        // Schedule behavior mode application at attack start time
        if (!config.behaviorMode.empty() && config.behaviorMode != "coexistence") {
            behaviorModeTimer = new cMessage("applyBehaviorMode");
            scheduleAt(attackStartTime, behaviorModeTimer);
            std::cout << "[BaseAttackApp] Scheduled behavior mode '" << config.behaviorMode 
                      << "' at attack start time " << attackStartTime << std::endl;
        }

        std::cout << "\n[BaseAttackApp] === INITIALIZATION COMPLETE ===" << std::endl;
        std::cout << "  Module: " << getFullPath() << std::endl;
        std::cout << "  Current time: " << simTime() << std::endl;
        std::cout << "  Attack start scheduled for: " << attackStartTime << std::endl;
        std::cout << "  Returning control to simulation..." << std::endl;
    }
}

void BaseAttackApp::configureAttack() {
    // Resolve destination address
    if (!destAddressStr.empty()) {
        destAddress = resolveDestAddress(destAddressStr.c_str());
        if (destAddress.isUnspecified()) {
            EV_ERROR << "[BaseAttackApp] Failed to resolve destination: " 
                     << destAddressStr << endl;
            return;
        }
    }
    
    isConfigured = true;
    
    // Schedule attack lifecycle
    scheduleAttackLifecycle();
    
    EV << "[BaseAttackApp] Configured - Target: " << destAddress 
       << ":" << destPort << endl;
}

void BaseAttackApp::scheduleAttackLifecycle() {
    if (!isConfigured) {
        EV_WARN << "[BaseAttackApp] Cannot schedule - not configured" << endl;
        std::cerr << "[BaseAttackApp] ERROR: Cannot schedule attack lifecycle - app not configured" << std::endl;
        return;
    }
    
    if (attackStartTime < 0 || attackStopTime < 0) {
        EV_WARN << "[BaseAttackApp] Invalid timing - cannot schedule" << endl;
        std::cerr << "[BaseAttackApp] ERROR: Invalid timing - startTime="
                  << attackStartTime << ", stopTime=" << attackStopTime << std::endl;
        return;
    }
    
    std::cout << "[BaseAttackApp::scheduleAttackLifecycle] Scheduling attack lifecycle for " 
              << getFullPath() << std::endl;

    // Schedule attack start
    startAttackTimer = new cMessage("startAttack");
    scheduleAt(attackStartTime, startAttackTimer);
    
    // Schedule attack stop
    stopAttackTimer = new cMessage("stopAttack");
    scheduleAt(attackStopTime, stopAttackTimer);
    
    EV << "[BaseAttackApp] Scheduled attack: start=" << attackStartTime 
       << ", stop=" << attackStopTime << endl;
}

void BaseAttackApp::scheduleAttackStart(simtime_t startTime, simtime_t stopTime) {
    EV_INFO << "[BaseAttackApp] Scheduling attack: start=" << startTime << ", stop=" << stopTime << endl;
    std::cout << "[BaseAttackApp] Scheduling attack for " << getFullPath() 
              << ": start=" << startTime << ", stop=" << stopTime << std::endl;
    
    // Cancel any existing timers
    if (startAttackTimer && startAttackTimer->isScheduled()) {
        cancelEvent(startAttackTimer);
    }
    if (stopAttackTimer && stopAttackTimer->isScheduled()) {
        cancelEvent(stopAttackTimer);
    }
    
    // Create timers if needed
    if (!startAttackTimer) {
        startAttackTimer = new cMessage("attackStart");
    }
    if (!stopAttackTimer) {
        stopAttackTimer = new cMessage("attackStop");
    }
    
    // Schedule start and stop
    scheduleAt(startTime, startAttackTimer);
    scheduleAt(stopTime, stopAttackTimer);
    
    std::cout << "[BaseAttackApp] ✓ Attack timers scheduled successfully" << std::endl;
}

void BaseAttackApp::toggleBurst() {
    inBurstOn = !inBurstOn;
    
    if (inBurstOn) {
        std::cout << "[BaseAttackApp] Burst ON - sending at "
                  << currentRate << " pps" << std::endl;
        scheduleAt(simTime() + burstOnDuration, burstToggleTimer);
    } else {
        std::cout << "[BaseAttackApp] Burst OFF - paused for "
                  << burstOffDuration << "s" << std::endl;
        scheduleAt(simTime() + burstOffDuration, burstToggleTimer);
    }
}

void BaseAttackApp::handleMessageWhenUp(cMessage* msg) {
    if (msg->isSelfMessage()) {
        if (strcmp(msg->getName(), "attackStart") == 0 || msg == startAttackTimer) {
            EV_INFO << "[BaseAttackApp] Attack start timer fired" << endl;
            std::cout << "[BaseAttackApp] Attack start timer fired at " << simTime()
                      << " for " << getFullPath() << std::endl;
            
            isActive = true;
            emit(attackStartSignal, 1);
            startAttack();
            delete msg;
            startAttackTimer = nullptr;
        }
        else if (strcmp(msg->getName(), "attackStop") == 0 || msg == stopAttackTimer) {
            EV_INFO << "[BaseAttackApp] Attack stop timer fired" << endl;
            std::cout << "[BaseAttackApp] Attack stop timer fired at " << simTime()
                      << " for " << getFullPath() << std::endl;
            
            isActive = false;
            emit(attackStopSignal, 0);
            stopAttack();
            delete msg;
            stopAttackTimer = nullptr;
        }
        else if (msg == sendTimer) {
            executeAttack();
        }
        else if (msg == rateUpdateTimer) {
            updateAttackRate();
        }
        else if (msg == burstToggleTimer) {
            toggleBurst();
        }
        else if (msg == behaviorModeTimer) {
            std::cout << "[BaseAttackApp] Applying behavior mode: " << config.behaviorMode << std::endl;
            applyBehaviorMode(config.behaviorMode);
            delete msg;
            behaviorModeTimer = nullptr;
        }
        else {
            EV_WARN << "[BaseAttackApp] Unhandled self-message: " << msg->getName() << endl;
            std::cerr << "[BaseAttackApp] WARNING: Unhandled self-message: " << msg->getName()
                      << " (deleting)" << std::endl;
            delete msg;
        }
    }
    else {
        EV_WARN << "[BaseAttackApp] Unhandled external message: " << msg->getName() << endl;
        std::cerr << "[BaseAttackApp] WARNING: Unhandled external message" << std::endl;
        delete msg;
    }
}

void BaseAttackApp::logAttackPacket(const char* direction, 
                                   const char* protocol,
                                   const inet::L3Address& srcAddr,
                                   int srcPort,
                                   const inet::L3Address& destAddr,
                                   int destPort,
                                   int size,
                                   bool spoofingEnabled,
                                   const char* spoofedSrcIP,
                                   const char* label) {
    
    packetsSent++;
    bytesSent += size;
    
    emit(packetSentSignal, size);
    emit(bytesSentSignal, (long)size);
    
    // Check labeler is valid
    if (!labeler || !labeler->isOpen()) {
        static bool warningLogged = false;
        if (!warningLogged) {
            EV_WARN << "[BaseAttackApp] Labeler not initialized - labels will not be written" << endl;
            warningLogged = true;
        }
        return;
    }
    
    // Check source IP
    if (srcAddr.isUnspecified()) {
        std::cerr << "[BaseAttackApp] WARNING: Source IP is unspecified!" << std::endl;
    }
    
    // Create label entry
    LabelEntry entry;
    entry.timestamp = simTime();
    entry.packetNumber = packetsSent;
    entry.srcModule = getFullPath();
    entry.srcIP = srcAddr.str();
    entry.srcPort = srcPort;
    entry.direction = direction;
    entry.trafficType = attackType;
    entry.protocol = protocol;
    entry.destAddress = destAddr.str();
    entry.destPort = destPort;
    entry.packetSize = size;
    entry.label = label;  // Use provided label ("malicious" or "benign")
    entry.spoofingEnabled = spoofingEnabled;
    entry.spoofedSrcIP = spoofedSrcIP;
    
    EV_DEBUG << "[BaseAttackApp] Logging: " << entry.srcIP << ":" << entry.srcPort 
             << " -> " << entry.destAddress << ":" << entry.destPort << endl;
    
    // Write to CSV via TrafficLabeler
    if (!labeler->logPacket(entry)) {
        std::cerr << "[BaseAttackApp] ERROR: Failed to log packet" << std::endl;
    }
}


L3Address BaseAttackApp::resolveDestAddress(const char* addrStr) {
    L3AddressResolver resolver;
    return resolver.resolve(addrStr);
}

void BaseAttackApp::finish() {
    ApplicationBase::finish();
    // Ensure any active attack is stopped so subclasses can close sockets
    if (isActive) {
        EV << "[BaseAttackApp] finish(): attack still active, calling stopAttack() to cleanup." << endl;
        std::cout << "[BaseAttackApp] finish(): attack still active at finish(), attempting to stop..." << std::endl;
        try {
            stopAttack();
        } catch (const std::exception &e) {
            EV_WARN << "[BaseAttackApp] Exception while stopping attack in finish(): " << e.what() << endl;
            std::cerr << "[BaseAttackApp] ERROR: Exception while stopping attack in finish(): " << e.what() << std::endl;
        } catch (...) {
            EV_WARN << "[BaseAttackApp] Unknown exception while stopping attack in finish()" << endl;
            std::cerr << "[BaseAttackApp] ERROR: Unknown exception while stopping attack in finish()" << std::endl;
        }
    }

    // CRITICAL: Flush any remaining buffered labels before module destruction
    if (labeler) {
        labeler->close();
    }
    
    EV << "[BaseAttackApp] Attack summary:" << endl;
    EV << "  Type: " << attackType << endl;
    EV << "  Packets sent: " << packetsSent << endl;
    EV << "  Bytes sent: " << bytesSent << endl;
    EV << "  Duration: " << duration << "s" << endl;
    
    recordScalar("packetsSent", packetsSent);
    recordScalar("bytesSent", bytesSent);
    recordScalar("attackDuration", duration.dbl());
}

void BaseAttackApp::handleStartOperation(LifecycleOperation* operation) {
    EV_INFO << "BaseAttackApp starting" << endl;
}

void BaseAttackApp::handleStopOperation(LifecycleOperation* operation) {
    EV_INFO << "BaseAttackApp stopping" << endl;
    if (isActive) {
        stopAttack();
    }
}

void BaseAttackApp::handleCrashOperation(LifecycleOperation* operation) {
    EV_WARN << "BaseAttackApp crashed" << endl;
    if (isActive) {
        isActive = false;
    }
}

void BaseAttackApp::startAttack() {
    if (!isConfigured) {
        std::cerr << "[BaseAttackApp] ERROR: Cannot start - not configured!" << std::endl;
        return;
    }
    
    EV_INFO << "[BaseAttackApp] Starting attack with style: " << config.attackStyle << endl;
    std::cout << "[BaseAttackApp] Starting attack with style: " << config.attackStyle 
              << " (rate: " << initialRate << "->" << peakRate << ")" << std::endl;
    

    isActive = true;
    emit(attackStartSignal, 1);
    
    // Create send timer (if not already created by subclass)
    if (!sendTimer) {
        sendTimer = new cMessage("sendPacket");
        std::cout << "[BaseAttackApp] Created sendTimer" << std::endl;
    }
    
    // Ramping is enabled by parameters, 
    bool rampAttack = (rampDuration > 0 && peakRate > initialRate);

    // Setup ramping if needed
    if (rampAttack) {
        if (!rateUpdateTimer) {
            rateUpdateTimer = new cMessage("updateRate");
        }
        scheduleAt(simTime() + 0.1, rateUpdateTimer);
        
        std::cout << "[BaseAttackApp] Rate ramping enabled: "
                  << initialRate << " -> " << peakRate << " pps over "
                  << rampDuration << "s" << std::endl;
    }
    
    // Setup bursting if needed
    if (burstMode && burstOnDuration > 0 && burstOffDuration > 0) {
        if (!burstToggleTimer) {
            burstToggleTimer = new cMessage("toggleBurst");
        }
        scheduleAt(simTime() + burstOnDuration, burstToggleTimer);
        
        std::cout << "[BaseAttackApp] Burst mode enabled: ON="
                  << burstOnDuration << "s, OFF=" << burstOffDuration << "s" << std::endl;
    }
}

void BaseAttackApp::stopAttack() {
    EV_INFO << "[BaseAttackApp] Stopping attack" << endl;
    std::cout << "[BaseAttackApp] Stopping attack. Sent " << packetsSent 
              << " packets (" << bytesSent << " bytes)" << std::endl;
    
    isActive = false;
    emit(attackStopSignal, 0);
    
    // Cancel all timers
    if (sendTimer && sendTimer->isScheduled()) {
        cancelEvent(sendTimer);
    }
    if (rateUpdateTimer && rateUpdateTimer->isScheduled()) {
        cancelEvent(rateUpdateTimer);
    }
    if (burstToggleTimer && burstToggleTimer->isScheduled()) {
        cancelEvent(burstToggleTimer);
    }
}

void BaseAttackApp::executeAttack() {
    if (!isActive) {
        std::cout << "[BaseAttackApp] executeAttack called but not active" << std::endl;
        return;
    }
    
    if (simTime() >= attackStopTime) {
        std::cout << "[BaseAttackApp] Reached stop time - stopping attack" << std::endl;
        isActive = false;
        return;
    }
    
    // Check if we're in burst OFF period
    if (burstMode && !inBurstOn) {
        scheduleNextAttack();
        return;
    }
    
    try {
        // Send attack packet (subclass implements this)
        sendAttackPacket();
    } catch (const std::exception& e) {
        std::cerr << "[BaseAttackApp] Exception in sendAttackPacket: " << e.what() << std::endl;
    }
    
    // Schedule next packet
    scheduleNextAttack();
}

void BaseAttackApp::scheduleNextAttack() {
    if (!isActive || simTime() >= attackStopTime) {
        return;
    }
    
    if (!sendTimer) {
        std::cerr << "[BaseAttackApp] ERROR: sendTimer is nullptr in scheduleNextAttack()!" << std::endl;
        throw cRuntimeError("sendTimer is nullptr - was it deleted prematurely?");
    }
    
    if (sendTimer->isScheduled()) {
        std::cerr << "[BaseAttackApp] WARNING: sendTimer already scheduled - canceling first" << std::endl;
        cancelEvent(sendTimer);
    }
    
    // Calculate interval based on current rate
    double interval = calculateCurrentInterval();
    
    simtime_t nextSend = simTime() + interval;
    
    // Don't schedule beyond stop time
    if (nextSend >= attackStopTime) {
        std::cout << "[BaseAttackApp] Reached stop time - not scheduling next packet" << std::endl;
        return;
    }
    
    scheduleAt(nextSend, sendTimer);
}

void BaseAttackApp::updateAttackRate() {
    // Ramp rate from initialRate to peakRate over rampDuration
    simtime_t elapsed = simTime() - attackStartTime;
    
    if (elapsed >= rampDuration) {
        // Ramp complete - stay at peak
        currentRate = peakRate;
        
        // Cancel rate update timer
        if (rateUpdateTimer && rateUpdateTimer->isScheduled()) {
            cancelEvent(rateUpdateTimer);
        }
        
        std::cout << "[BaseAttackApp] Ramp complete - now at peak rate "
                  << peakRate << " pps" << std::endl;
        return;
    }
    
    // Linear interpolation
    double progress = elapsed.dbl() / rampDuration;
    currentRate = initialRate + (peakRate - initialRate) * progress;
    
    std::cout << "[BaseAttackApp] Ramping: rate=" << currentRate
              << " pps (progress=" << (progress*100) << "%)" << std::endl;
    
    // Schedule next update
    if (rateUpdateTimer) {
        scheduleAt(simTime() + 0.1, rateUpdateTimer);
    }
}

double BaseAttackApp::calculateCurrentInterval() {
    if (currentRate <= 0) {
        return 1.0;
    }
    return 1.0 / currentRate;
}

void BaseAttackApp::sendAttackPacket() {
    // Default implementation for placeholders
    if (attackStartTime >= 0) {
        throw cRuntimeError("sendAttackPacket() not implemented in subclass %s! "
                            "Attack subclasses must override this method.", 
                            getClassName());
    }
}

AttackConfig BaseAttackApp::parseAttackProfile(const char* jsonFilePath) {
    std::cout << "[BaseAttackApp] Parsing attack profile: " << jsonFilePath << std::endl;
    
    AttackConfig config;
    
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        throw cRuntimeError("Cannot open attack profile file: %s", jsonFilePath);
    }
    
    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        throw cRuntimeError("JSON parsing error in %s: %s", jsonFilePath, e.what());
    }
    
    // =====================================================
    // PARSE ATTACK PROFILE SECTION
    // =====================================================
    if (json.contains("attackProfile")) {
        auto& profile = json["attackProfile"];
        
        config.attackType = profile.value("attackType", "generic");
        config.attackStyle = profile.value("attackStyle", "intense");
        config.behaviorMode = profile.value("behaviorMode", "coexistence");
        
        std::cout << "  ├─ Attack Type: " << config.attackType << std::endl;
        std::cout << "  ├─ Attack Style: " << config.attackStyle << std::endl;
        std::cout << "  ├─ Behavior Mode: " << config.behaviorMode << std::endl;
    }
    
    // =====================================================
    // PARSE TIMING SECTION
    // =====================================================
    if (json.contains("timing")) {
        auto& timing = json["timing"];
        
        config.dormantDuration = timing.value("dormantDuration", 0.0);
        config.attackDuration = timing.value("attackDuration", 300.0);
        
        std::cout << "  ├─ Dormant Duration: " << config.dormantDuration << "s" << std::endl;
        std::cout << "  ├─ Attack Duration: " << config.attackDuration << "s" << std::endl;
    }
    
    // =====================================================
    // PARSE ATTACK PARAMETERS
    // =====================================================
    if (json.contains("attackParameters")) {
        auto& params = json["attackParameters"];
        
        //config.ratePattern = params.value("ratePattern", "constant");
        config.initialRate = params.value("initialRate", 10);
        config.peakRate = params.value("peakRate", 100);
        config.rampDuration = params.value("rampDuration", 10.0);
        config.burstOnDuration = params.value("burstOnDuration", 0.0);
        config.burstOffDuration = params.value("burstOffDuration", 0.0);
        config.packetSize = params.value("packetSize", 1400);
        config.enableSpoofing = params.value("enableSpoofing", false);

        std::cout << "  ├─ Initial Rate: " << config.initialRate << " pps" << std::endl;
        std::cout << "  ├─ Peak Rate: " << config.peakRate << " pps" << std::endl;
        std::cout << "  ├─ Enable Spoofing: " << (config.enableSpoofing ? "YES" : "NO") << std::endl;
        
        if (params.contains("queryDomain")) {
            config.queryDomain = params["queryDomain"].get<std::string>();
            std::cout << "  ├─ Query Domain: " << config.queryDomain << std::endl;
            
        }
        if (params.contains("amplificationFactor")) {
            config.amplificationFactor = params["amplificationFactor"].get<double>();
            std::cout << "  ├─ Amplification Factor: " << config.amplificationFactor << std::endl;
        }
        if (params.contains("openResolvers")) {
            config.openResolvers = params["openResolvers"].get<std::string>();
            std::cout << "  ├─ Open Resolvers: " << config.openResolvers << std::endl;
        }
    }
    
    // =====================================================
    // PARSE HTTP PARAMETERS (if present)
    // =====================================================
    if (json.contains("httpParameters")) {
        auto& httpParams = json["httpParameters"];
        
        config.numConnections = httpParams.value("numConnections", 10);
        config.httpMethod = httpParams.value("httpMethod", "GET");
        config.requestPaths = httpParams.value("requestPaths", "/");
        config.userAgent = httpParams.value("userAgent", "Mozilla/5.0");
        config.keepAlive = httpParams.value("keepAlive", true);
        config.contentLength = httpParams.value("contentLength", 0);
        
        std::cout << "  ├─ HTTP Connections: " << config.numConnections << std::endl;
        std::cout << "  ├─ HTTP Method: " << config.httpMethod << std::endl;
        std::cout << "  ├─ HTTP Keep-Alive: " << (config.keepAlive ? "YES" : "NO") << std::endl;
    }
    
    // =====================================================
    // PARSE TARGET CONFIGURATION
    // =====================================================
    if (json.contains("targetConfiguration")) {
        auto& target = json["targetConfiguration"];
        
        if (target.contains("targets") && target["targets"].is_array()) {
            for (const auto& t : target["targets"]) {
                config.targets.push_back(t.get<std::string>());
            }
        }
        
        config.targetPort = target.value("targetPort", 5000);
        
        std::cout << "  ├─ Targets: " << config.targets.size() << std::endl;
        std::cout << "  ├─ Target Port: " << config.targetPort << std::endl;
    }
    
    // Apply attack style to set rate pattern
    applyAttackStyle(config.attackStyle);
    
    // Validate configuration
    if (!validateConfig(config)) {
        throw cRuntimeError("Invalid attack configuration in file: %s", jsonFilePath);
    }
    
    std::cout << "  └─ Profile parsed and validated successfully" << std::endl;
    
    return config;
}

void BaseAttackApp::applyAttackStyle(const std::string& attackStyle) {
    std::cout << "[BaseAttackApp] Applying attack style: " << attackStyle << std::endl;
    
    if (attackStyle == "intense") {
        //ratePattern = "constant";
        burstMode = false;
        if (initialRate == 0) initialRate = 500;
        if (peakRate == 0) peakRate = 1000;
        if (rampDuration == 0) rampDuration = 10.0;
        std::cout << "  └─ Style: INTENSE - Constant peak rate" << std::endl;
    }
    else if (attackStyle == "stealthy") {
        //ratePattern = "ramp";
        if (initialRate == 0) initialRate = 5;
        if (peakRate == 0) peakRate = 50;
        if (rampDuration == 0) rampDuration = 60.0;
        std::cout << "  └─ Style: STEALTHY - Ramping rate" << std::endl;
    }
    else if (attackStyle == "pulsing") {
        //ratePattern = "burst";
        burstMode = true;
        if (burstOnDuration == 0) burstOnDuration = 2.0;
        if (burstOffDuration == 0) burstOffDuration = 5.0;
        if (initialRate == 0) initialRate = 100;
        std::cout << "  └─ Style: PULSING - Burst pattern" << std::endl;
    }
    else if (attackStyle == "ramping") {
        //ratePattern = "ramp";
        if (initialRate == 0) initialRate = 10;
        if (peakRate == 0) peakRate = 500;
        if (rampDuration == 0) rampDuration = 20.0;
        std::cout << "  └─ Style: RAMPING - Aggressive ramp" << std::endl;
    }
    else if (attackStyle == "slowrate") {
        //ratePattern = "constant";
        burstMode = false;
        if (initialRate == 0) initialRate = 5;
        if (peakRate == 0) peakRate = 20;
        if (rampDuration == 0) rampDuration = 120.0;
        std::cout << "  └─ Style: SLOWRATE - Constant low rate" << std::endl;
    }
    else {
        std::cout << "  └─ Unknown style, using default (constant)" << std::endl;
    }
}

bool BaseAttackApp::validateConfig(const AttackConfig& config) {
    // Validate attack type
    if (config.attackType.empty()) {
        std::cerr << "[BaseAttackApp] ERROR: Empty attack type" << std::endl;
        return false;
    }
    
    // Validate timing
    if (config.dormantDuration < 0 || config.attackDuration <= 0) {
        std::cerr << "[BaseAttackApp] ERROR: Invalid timing" << std::endl;
        return false;
    }
    
    // Validate rates
    if (config.peakRate <= 0 || config.initialRate < 0) {
        std::cerr << "[BaseAttackApp] ERROR: Invalid rates" << std::endl;
        return false;
    }
    
    // Validate targets
    if (config.targets.empty()) {
        std::cerr << "[BaseAttackApp] ERROR: No targets specified" << std::endl;
        return false;
    }
    
    return true;
}

/**
 * @brief Apply behavior mode to benign applications
 * @param behaviorMode Mode to apply (coexistence/replace/hybrid)
 */
void BaseAttackApp::applyBehaviorMode(const std::string& behaviorMode) {
    std::cout << "[BaseAttackApp] Applying behavior mode: " << behaviorMode << std::endl;
    
    cModule* ueModule = getParentModule();
    if (!ueModule) {
        std::cerr << "[BaseAttackApp] ERROR: Cannot get parent UE module!" << std::endl;
        return;
    }
    
    // =====================================================
    // COEXISTENCE: Benign traffic continues normally
    // =====================================================
    if (behaviorMode == "coexistence") {
        std::cout << "  └─ Mode: COEXISTENCE - Benign apps continue running alongside attack" << std::endl;
        // Do nothing
    }
    
    // =====================================================
    // REPLACE: Stop all benign traffic
    // =====================================================
    else if (behaviorMode == "replace") {
        std::cout << "  └─ Mode: REPLACE - Stopping all benign apps (slots 0-9)" << std::endl;
        
        for (int slot = 0; slot <= benignSlotEnd; slot++) {
            cModule* benignApp = ueModule->getSubmodule("app", slot);
            
            if (benignApp) {
                const char* typeName = benignApp->getComponentType()->getName();
                
                if (strcmp(typeName, "DynamicTrafficSender") == 0 || strcmp(typeName, "CbrSender") == 0) {
                    if (benignApp->hasPar("stopTime")) {
                        benignApp->par("stopTime") = simTime().dbl();
                        std::cout << "    ├─ Stopped " << benignApp->getFullPath() << std::endl;
                    }
                }
            }
        }
        
        std::cout << "    └─ All benign traffic stopped" << std::endl;
    }
    
    // =====================================================
    // HYBRID: Reduce benign traffic by 50%
    // =====================================================
    else if (behaviorMode == "hybrid") {
        std::cout << "  └─ Mode: HYBRID - Reducing benign app rates by 50%" << std::endl;
        
        for (int slot = 0; slot <= benignSlotEnd; slot++) {
            cModule* benignApp = ueModule->getSubmodule("app", slot);
            
            if (benignApp) {
                const char* typeName = benignApp->getComponentType()->getName();
                
                if (strcmp(typeName, "DynamicTrafficSender") == 0) {
                    if (benignApp->hasPar("sendInterval")) {
                        double currentInterval = benignApp->par("sendInterval").doubleValue();
                        double newInterval = currentInterval * 2.0;
                        benignApp->par("sendInterval") = newInterval;
                        std::cout << "    ├─ Reduced rate of " << benignApp->getFullPath() << std::endl;
                    }
                }
                else if (strcmp(typeName, "CbrSender") == 0) {
                    if (benignApp->hasPar("sampling_time")) {
                        double currentInterval = benignApp->par("sampling_time").doubleValue();
                        double newInterval = currentInterval * 2.0;
                        benignApp->par("sampling_time") = newInterval;
                        std::cout << "    ├─ Reduced rate of " << benignApp->getFullPath() << std::endl;
                    }
                }
            }
        }
        
        std::cout << "    └─ Benign traffic reduced by 50%" << std::endl;
    }
    
    else {
        std::cerr << "[BaseAttackApp] WARNING: Unknown behavior mode '" << behaviorMode 
                  << "', defaulting to COEXISTENCE" << std::endl;
    }
}

void BaseAttackApp::printProgress(const char* protocol, const char* extraInfo) {
    // Print rate changes (>10% difference)
    if (lastPrintedRate < 0 || abs(currentRate - lastPrintedRate) > currentRate * 0.1) {
        std::cout << "[" << attackType << "] Rate: " << currentRate << " pps" << std::endl;
        lastPrintedRate = currentRate;
    }
    
    // Adaptive interval based on rate
    int interval;
    if (currentRate >= 1000) {
        interval = 1000;
    } else if (currentRate >= 100) {
        interval = 100;
    } else if (currentRate >= 10) {
        interval = 10;
    } else {
        interval = 1;
    }
    
    // Print at adaptive intervals OR first/last packets
    bool shouldPrint = (packetsSent % interval == 0) || 
                       (packetsSent <= 5) || 
                       (attackStopTime > 0 && (attackStopTime - simTime()) < 0.1);
    
    if (shouldPrint) {
        std::cout << "[" << attackType << "] " << packetsSent << " " << protocol 
                  << " packets sent (rate=" << currentRate << " pps, total=" 
                  << (bytesSent / 1024) << " KB)";
        
        if (extraInfo && strlen(extraInfo) > 0) {
            std::cout << " " << extraInfo;
        }
        
        std::cout << std::endl;
    }
}

void BaseAttackApp::adjustAdaptiveRate() {
    // Only check periodically (every 1 second)
    if (simTime() - lastAdaptiveCheck < ADAPTIVE_CHECK_INTERVAL) {
        return;
    }
    lastAdaptiveCheck = simTime();
    
    if (packetsSent == 0) return;  
    
    // Calculate queue pressure (normalized to 0-1) - UE-SPECIFIC metric
    double queuePressure = (double)currentQueueLength / QUEUE_THROTTLE_THRESHOLD;
    
    double oldMultiplier = adaptiveRateMultiplier;
    
    // This ensures we're preventing UE bottleneck, allowing network congestion to be observed
    if (queuePressure > 0.8) {
        // UE queue filling up - reduce rate by 10% to prevent UE-side drops
        adaptiveRateMultiplier *= 0.9;
        if (adaptiveRateMultiplier < MIN_RATE_MULTIPLIER) {
            adaptiveRateMultiplier = MIN_RATE_MULTIPLIER;
        }
    } else if (queuePressure < 0.5 && adaptiveRateMultiplier < 1.0) {
        // UE queue healthy - slowly increase rate back up by 5%
        adaptiveRateMultiplier *= 1.05;
        if (adaptiveRateMultiplier > 1.0) {
            adaptiveRateMultiplier = 1.0;
        }
    }
    
    // Log significant changes (still track drops for visibility, but don't throttle based on them)
    if (fabs(oldMultiplier - adaptiveRateMultiplier) > 0.05) {
        std::cout << "[BaseAttackApp] UE queue-based throttle: " 
                  << (oldMultiplier * 100) << "% → " << (adaptiveRateMultiplier * 100) 
                  << "% (queue=" << currentQueueLength << "/" << QUEUE_THROTTLE_THRESHOLD
                  << ", network_drops=" << droppedPackets << ")" << std::endl;
    }
    
    // Apply multiplier to current rate (will affect next scheduled packet)
    int targetRate = (int)(peakRate * adaptiveRateMultiplier);
    if (currentRate != targetRate) {
        currentRate = targetRate;
    }
}

void BaseAttackApp::receiveSignal(cComponent* source, simsignal_t signalID, intval_t value, cObject* details) {
    // Handle queue length updates
    if (signalID == registerSignal("queueLength")) {
        currentQueueLength = value;
        
        // Check adaptive rate periodically
        adjustAdaptiveRate();
    }
}

} // namespace ddosimu5g
