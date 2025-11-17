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

#include "../trafficcontroller/DataTrafficController.h"

Define_Module(DataTrafficController);

DataTrafficController::DataTrafficController() {
    EV << "DataTrafficController constructor ended at time: " << simTime() << "\n";
}

DataTrafficController::~DataTrafficController() {
    EV << "DataTrafficController Deconstructor ended at time: " << simTime() << "\n";
}

void DataTrafficController::initialize() {

    infectionTimeSignal_ = registerSignal("infectionEventTime");

    //Enable setting for traffic controller behaviour
    enableTrafficMod = par("enableTrafficMod").boolValue();
    trafficMod_pktSize = par("trafficMod_pktSize").intValue();
    trafficMod_dataRate = par("trafficMod_dataRate").doubleValue();

    // Load the JSON file
    const char *filePath = par("infectionFilePath").stringValue();
    parseInfectionData(filePath);

    // Schedule events based on infection data
    scheduleInfectionEvents();
}

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

void DataTrafficController::handleMessage(omnetpp::cMessage *msg) {

    if (msg->isSelfMessage()) {
        int nodeId = msg->getKind();

        /**
         * To Do. I need to set the update traffic from the config file
         */
        // Emit signal for infection
        emit(infectionTimeSignal_, nodeId);

        if (enableTrafficMod) {
            updateTraffic(nodeId);
            EV_INFO << "Update traffic is enabled. Enter ddosTraffic for nodeId: " << nodeId << endl;
                    std::cout << "Update traffic is enabled. Enter ddosTraffic for nodeId" << std::endl;

        } else {
            EV_INFO << "Update traffic is disabled. Skipping ddosTraffic for nodeId: " << nodeId << endl;
            std::cout << "Update DDoS traffic is disabled. Skipping ddosTraffic for nodeId" << std::endl;
        }

        //updateTraffic(nodeId);
        delete msg;
    }
}

void DataTrafficController::finish() {
    EV << "DataTrafficController Simulation ended at time: " << simTime() << "\n";
}

void DataTrafficController::updateTraffic(int nodeId) {

    // Construct the path to the CbrSender application
    //std::string modulePath = "Base_Config_5RAN.cbrUe[" + std::to_string(nodeId) + "].app[0]";
    std::string basePath = getParentModule()->getFullPath();
    std::string modulePath = basePath + ".cbrUe[" + std::to_string(nodeId) + "].app[0]";


    std::cout << "Module path: " << modulePath << std::endl;
    std::cout << "DataTrafficController - Processing self-message for nodeId: " << nodeId << std::endl;


    cModule *appModule = getModuleByPath(modulePath.c_str());
    if (!appModule)
        throw cRuntimeError("CbrSender module not found for nodeId: %d", nodeId);

    // Dynamically update parameters
    //appModule->par("PacketSize").setIntValue(2048); // For some reason right now i cant go beyond 2048 for packet size
    //appModule->par("sampling_time").setDoubleValue(0.05); // For some reason I cant go beyond 0.005 sampling_size
    appModule->par("PacketSize").setIntValue(trafficMod_pktSize); // For some reason right now i cant go beyond 2048 for packet size
    appModule->par("sampling_time").setDoubleValue(trafficMod_dataRate); // For some reason I cant go beyond 0.005 sampling_size

    EV_INFO << "Update traffic paket size:" << trafficMod_pktSize << endl;
    std::cout << "Update traffic paket size:" << trafficMod_pktSize << std::endl;

    EV_INFO << "Update traffic paket size:" << trafficMod_pktSize << endl;
    std::cout << "Update traffic paket size:" << trafficMod_pktSize << std::endl;

    // Notify the module to reinitialize these parameters
    cMessage *updateMsg = new cMessage("updateParams");
    sendDirect(updateMsg, appModule, "controlIn");
    std::cout << "Scheduled updateParams message for module: " << modulePath << std::endl;

    // Stop and delete the current application
    /*std::cout << "Stopping and deleting current application for UE[" << nodeId << std::endl;
    appModule->callFinish();  // Perform cleanup
    appModule->deleteModule();  // Remove the module from the simulation

    std::cout << "Checking UE module: " << (ueModule ? "Exists" : "Does not exist") << std::endl;

    ddosTraffic(nodeId);
    */

}

void DataTrafficController::ddosTraffic(int nodeId) {

    //std::cout << "Is sampling_time mutable? " << par("sampling_time").isMutable() << std::endl;

    // Create a new DDoS-like application dynamically
    std::cout << "Creating new DDoS-like application for UE[" << nodeId << "]" << std::endl;
    cModule *ueModule = getParentModule()->getSubmodule("cbrUe", nodeId);
    if (!ueModule)
        throw cRuntimeError("UE module not found for nodeId: %d", nodeId);

    // Create a new application with a unique name
    std::string newAppName = "ddosApp";
    if (ueModule->hasSubmodule(newAppName.c_str())) {
        throw cRuntimeError("Application module '%s' already exists for nodeId: %d", newAppName.c_str(), nodeId);
    }

    std::cout << "Creating new DDoS-like application for UE[" << nodeId << std::endl;
    cModuleType *ddosAppType = cModuleType::get("DDoSSender");
    cModule *newAppModule = ddosAppType->create(newAppName.c_str(), ueModule);
    newAppModule->finalizeParameters();


    newAppModule->par("sampling_time").setDoubleValue(0.001);  // Set parameters
    newAppModule->par("PacketSize").setIntValue(4096);

    // Set the destination address for the traffic
    std::string destAddress = "remoteServer";  // Replace with your actual destination
    newAppModule->par("destAddress").setStringValue(destAddress.c_str());

    std::cout << "Updated Parameters: "
              << "sampling_time=" << newAppModule->par("sampling_time").doubleValue()
              << ", PacketSize=" << newAppModule->par("PacketSize").intValue()
              << ", destAddress=" << newAppModule->par("destAddress").stringValue()
              << std::endl;
    try {
        newAppModule->buildInside();
        newAppModule->callInitialize();
        std::cout << "Successfully initialized DDoSSender for nodeId: " << nodeId << std::endl;
    } catch (const cRuntimeError &e) {
        std::cout << "Error during DDoSSender initialization: " << std::endl;
    }

    std::cout << "Added new DDoS application for UE[" << nodeId  << "]" << std::endl;
}
