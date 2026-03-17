//
// DynamicTrafficSender - Dynamic multi-protocol traffic generator
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

#include "DynamicTrafficSender.h"

namespace ddosimu5g {

Define_Module(DynamicTrafficSender);

DynamicTrafficSender::~DynamicTrafficSender() {
}

void DynamicTrafficSender::initialize(int stage) {
    ApplicationBase::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        // Get module ID and app index
        id = getId();
        appId = getIndex();  // Get app[X] index (0-9)
        
        // Get parameters
        trafficScheduleFile = par("trafficScheduleFile").stdstringValue();
        
        // Get base path from parameter (e.g., "../run_results/pcaps/${configname}/${datetime}")
        std::string basePath = par("labelFilePath").stdstringValue();
        
        // Extract UE index from parent module
        int ueIndex = getParentModule()->getIndex();
        
        // Build hierarchical path: <basePath>/labels/ue<X>/app<Y>.csv
        std::ostringstream filepath;
        filepath << basePath << "/labels/ue" << ueIndex << "/app" << appId << ".csv";
        
        labelFilePath = filepath.str();
        
        labelFilePath = filepath.str();
        
        EV_INFO << "DynamicTrafficSender[" << appId << "] initializing..." << endl;
        std::cout << "[DynamicTrafficSender] App[" << appId << "] initializing with schedule: " 
                  << trafficScheduleFile << ", labels: " << labelFilePath << std::endl;
        
        // Initialize labeler (unique for this app)
        labeler = TrafficLabeler::getInstance(labelFilePath);
        if (labeler && labeler->isOpen()) {
            EV_INFO << "TrafficLabeler initialized: " << labelFilePath << endl;
            std::cout << "[DynamicTrafficSender] App[" << appId << "] labeler initialized: " 
                      << labelFilePath << std::endl;
        } else {
            EV_WARN << "Failed to initialize TrafficLabeler: " << labelFilePath << endl;
            std::cout << "[DynamicTrafficSender] Failed to initialize TrafficLabeler: " << labelFilePath << std::endl;
        }
        
        // Parse traffic schedule
        parseTrafficSchedule();
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Create and start protocol handler for this app_id
        // Wait until application layer is ready before starting traffic
        startTrafficFromSchedule();
    }
}

void DynamicTrafficSender::parseTrafficSchedule() {
    if (trafficScheduleFile.empty()) {
        EV_WARN << "No traffic schedule file specified" << endl;
        std::cout << "[DynamicTrafficSender] No traffic schedule file specified" << std::endl;
        return;
    }
    
    try {
        std::ifstream scheduleStream(trafficScheduleFile);
        if (!scheduleStream.is_open()) {
            throw std::runtime_error("Cannot open file: " + trafficScheduleFile);
        }
        
        trafficSchedule = nlohmann::json::parse(scheduleStream);
        scheduleStream.close();
        
        EV_INFO << "Loaded traffic schedule: " << trafficScheduleFile << endl;
        std::cout << "[DynamicTrafficSender] App[" << appId << "] loaded schedule with " 
                  << trafficSchedule["schedule"].size() << " entries" << std::endl;
        
    } catch (const std::exception& e) {
        EV_ERROR << "Error parsing traffic schedule: " << e.what() << endl;
        std::cerr << "[DynamicTrafficSender] ERROR parsing schedule: " << e.what() << std::endl;
    }
}

ProtocolHandler* DynamicTrafficSender::createProtocolHandler(const std::string& type) {
    if (type == "udp") {
        return new UdpProtocolHandler(this, labeler, appId);
    } else if (type == "dns") {
        return new DnsProtocolHandler(this, labeler, appId);
    } else if (type == "tcp") {
        return new TcpProtocolHandler(this, labeler, appId);
    } else if (type == "http") {
        return new HttpProtocolHandler(this, labeler, appId);
    }
    // Add more protocols here: mqtt, coap, https, etc.
    else {
        EV_WARN << "Unknown protocol type: " << type << endl;
        return nullptr;
    }
}

void DynamicTrafficSender::startTrafficFromSchedule() {
    if (!trafficSchedule.contains("schedule")) {
        EV_WARN << "No schedule array in JSON" << endl;
        return;
    }
    
    // Find traffic entry matching this app_id
    for (const auto& entry : trafficSchedule["schedule"]) {
        int entryAppId = entry.value("app_id", -1);
        
        if (entryAppId == appId) {
            std::string type = entry.value("type", "unknown");
            
            EV_INFO << "App[" << appId << "] matched schedule entry: type=" << type << endl;
            std::cout << "[DynamicTrafficSender] App[" << appId << "] matched schedule entry: " 
                      << type << std::endl;
            
            // Create protocol handler
            currentHandler = createProtocolHandler(type);
            
            if (currentHandler) {
                // Add traffic type to config
                nlohmann::json config = entry;
                config["trafficType"] = trafficSchedule.value("deviceType", "unknown");
                config["label"] = entry.value("label", "benign");
                
                // Start handler
                currentHandler->start(config);
                
                std::cout << "[DynamicTrafficSender] App[" << appId << "] started " 
                          << type << " handler" << std::endl;
            }
            
            return;  // Found matching entry
        }
    }
    
    EV_WARN << "No schedule entry found for app_id=" << appId << endl;
    std::cout << "[DynamicTrafficSender] App[" << appId << "] no matching schedule entry" << std::endl;
}

void DynamicTrafficSender::handleMessageWhenUp(cMessage* msg) {
    // Route message to appropriate protocol handler
    if (currentHandler) {
        // Self-messages (timers) have context pointer set
        if (msg->isSelfMessage()) {
            //std::cout << "[DynamicTrafficSender] App[" << appId << "] routing TIMER to handler" << std::endl;
            currentHandler->handleMessage(msg);
        }
        // Socket messages (packets from network) go to socket processor
        else {
            //std::cout << "[DynamicTrafficSender] App[" << appId << "] routing SOCKET MESSAGE '"
            //          << msg->getName() << "' to handler" << std::endl;
            currentHandler->processSocketMessage(msg);
        }
    } else {
        EV_WARN << "Received message but no active handler" << endl;
        std::cout << "[DynamicTrafficSender] App[" << appId << "] received message but NO HANDLER!" << std::endl;
        delete msg;
    }
}

void DynamicTrafficSender::finish() {
    EV_INFO << "DynamicTrafficSender[" << appId << "] finishing..." << endl;
    std::cout << "[DynamicTrafficSender] App[" << appId << "] FINISH called" << std::endl;
    
    // Stop and delete current handler FIRST, before labeler
    if (currentHandler) {
        currentHandler->stop();
        
        std::cout << "[DynamicTrafficSender] App[" << appId << "] finished. "
                  << "Sent=" << currentHandler->getPacketsSent() 
                  << ", Received=" << currentHandler->getPacketsReceived() << std::endl;
        
        // Delete handler NOW to avoid destructor trying to delete invalid pointer
        delete currentHandler;
        currentHandler = nullptr;
        std::cout << "[DynamicTrafficSender] App[" << appId << "] handler deleted" << std::endl;
    }
    
    // CRITICAL: Flush any remaining buffered labels AFTER handler cleanup
    if (labeler) {
        labeler->close();
    }
    
    std::cout << "[DynamicTrafficSender] App[" << appId << "] calling ApplicationBase::finish()" << std::endl;
    ApplicationBase::finish();
    std::cout << "[DynamicTrafficSender] App[" << appId << "] FINISH completed" << std::endl;
}

void DynamicTrafficSender::handleStartOperation(LifecycleOperation* operation) {
    EV_INFO << "Starting DynamicTrafficSender" << endl;
}

void DynamicTrafficSender::handleStopOperation(LifecycleOperation* operation) {
    EV_INFO << "Stopping DynamicTrafficSender" << endl;
    
    if (currentHandler) {
        currentHandler->stop();
    }
}

void DynamicTrafficSender::handleCrashOperation(LifecycleOperation* operation) {
    EV_WARN << "DynamicTrafficSender crashed" << endl;
}

} // namespace ddosimu5g
