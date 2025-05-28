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

#include "../simcommands/PostSimulationCommand.h"

Define_Module(PostSimulationCommand);

PostSimulationCommand::PostSimulationCommand() {
    getEnvir()->addLifecycleListener(this);

}

PostSimulationCommand::~PostSimulationCommand() {
    getEnvir()->removeLifecycleListener(this);
}

void PostSimulationCommand::initialize() {
    EV << "LTH - PostSimulationCommand module initialized.\n";  // Debug log
}

void PostSimulationCommand::lifecycleEvent(SimulationLifecycleEventType eventType, cObject *details) {
    if (eventType == SimulationLifecycleEventType::LF_ON_RUN_END) {
        EV << "LTH : Simulation end detected. Running post-simulation command.\n";
        try {
            // Retrieve the project root directory from the environment variable if set
            const char* projectRootEnv = std::getenv("PROJECT_ROOT_DIR");
            std::string projectRootDir = projectRootEnv ? projectRootEnv : ""; // Use empty string if not set

            // Construct file paths based on whether PROJECT_ROOT_DIR is set
            std::string resDir = getEnvir()->getConfig()->substituteVariables("${resultdir}/${configname}/");
            std::string baseName = getEnvir()->getConfig()->substituteVariables("${iterationvars}-repit-${repetition}");
            std::string vecFilePath;
            if (!projectRootDir.empty()) {
                vecFilePath = projectRootDir + "/" + resDir + baseName + ".vec";
            } else {
                vecFilePath = std::string("../../")  + resDir + baseName  + ".vec";
            }

            if (!vecFilePath.empty()) {
                EV << "Vector file path: " << vecFilePath << "\n";

                std::string outputFileName;
                if (!projectRootDir.empty()) {
                    outputFileName = projectRootDir + "/sim_dataset/" + baseName + "-vector-data.json";
                } else {
                    outputFileName = "../../sim_dataset/" + baseName + "-vector-data.json";
                }

                // Ensure sim_dataset directory exists
                std::string datasetDir = projectRootDir.empty() ? "../../sim_dataset" : projectRootDir + "/sim_dataset";
                if (mkdir(datasetDir.c_str(), 0777) != 0 && errno != EEXIST) {
                    EV << "LTH : Error creating directory: " << datasetDir << "\n";
                    return;
                }

                // Create the command string
                std::string command = "opp_scavetool x ";
                command += std::string("'");
                command += vecFilePath;
                command += std::string("'");
                command += " -F JSON -o ";
                command += std::string("'");
                command += outputFileName;
                command += std::string("'");

                EV << "LTH : Dataset store command  " << command << "\n";

                // Execute the command
                int result = std::system(command.c_str());
                if (result == 0) {
                    EV << "LTH : Post-simulation command executed successfully.\n";
                } else {
                    EV << "LTH : Error executing post-simulation command.\n";
                }
            }
        } catch (std::exception &e) {
            EV << "LTH : Exception caught: " << e.what() << "\n";
        }
    }
}
