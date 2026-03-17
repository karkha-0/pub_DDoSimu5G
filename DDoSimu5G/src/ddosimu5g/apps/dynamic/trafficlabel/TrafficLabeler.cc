//
// TrafficLabeler.cc - Traffic labeling implementation
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

#include "ddosimu5g/apps/dynamic/trafficlabel/TrafficLabeler.h"

#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>

namespace ddosimu5g {

// Initialize static members
std::map<std::string, std::shared_ptr<TrafficLabeler>> TrafficLabeler::instances;
std::mutex TrafficLabeler::instanceMutex;

// Helper function to create directories recursively
static bool createDirectories(const std::string& path) {
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        std::string subdir = path.substr(0, pos);
        if (mkdir(subdir.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
    }
    // Create final directory
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

/**
 * @brief Destructor - closes file if still open
 */
TrafficLabeler::TrafficLabeler(const std::string& filepath) 
    : filepath(filepath), fileInitialized(false), packetCount(0) {
    
    // Extract directory path and create it
    size_t lastSlash = filepath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dirPath = filepath.substr(0, lastSlash);
        if (!createDirectories(dirPath)) {
            std::cerr << "[TrafficLabeler] WARNING: Could not create directory: " << dirPath << std::endl;
        }
    }
    
    // Check if file exists, if not create it with header
    std::ifstream checkFile(filepath);
    bool fileExists = checkFile.good();
    checkFile.close();
    
    if (!fileExists) {
        // Create new file with header and keep it open
        csvFile.open(filepath, std::ios::out | std::ios::trunc);
        if (csvFile.is_open()) {
            csvFile << LabelEntry::getCSVHeader() << std::endl;
            csvFile.flush();  // Ensure header is written
            std::cout << "[TrafficLabeler] Created new file with header: " << filepath << std::endl;
        } else {
            std::cerr << "[TrafficLabeler] ERROR: Failed to create file: " << filepath << std::endl;
            return;
        }
    } else {
        // File exists, open in append mode
        csvFile.open(filepath, std::ios::out | std::ios::app);
        if (!csvFile.is_open()) {
            std::cerr << "[TrafficLabeler] ERROR: Failed to open existing file: " << filepath << std::endl;
            return;
        }
    }
    
    fileInitialized = true;
    std::cout << "[TrafficLabeler] Initialized for file (kept open): " << filepath << std::endl;
}

TrafficLabeler::~TrafficLabeler() {
    // Flush any remaining buffered entries
    if (!buffer.empty()) {
        flushBuffer();
    }
    
    if (csvFile.is_open()) {
        csvFile.close();
    }
    std::cout << "[TrafficLabeler] Destructor called for: " << filepath 
              << " (wrote " << packetCount << " packets)" << std::endl;
}

/**
 * @brief Get or create labeler instance for given file path
 * @param filepath Absolute path to CSV label file
 * @return Shared pointer to labeler instance
 */
std::shared_ptr<TrafficLabeler> TrafficLabeler::getInstance(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(instanceMutex);
    
    std::cout << "[TrafficLabeler] getInstance called with filepath: '" << filepath << "'" << std::endl;
    
    // Check if instance already exists for this file
    auto it = instances.find(filepath);
    if (it != instances.end()) {
        std::cout << "[TrafficLabeler] Returning existing instance for: " << filepath << std::endl;
        return it->second;  // Return shared pointer directly
    }
    
    // Create new instance - kept alive by static map (survives module destruction)
    std::cout << "[TrafficLabeler] Creating NEW instance for: " << filepath << std::endl;
    auto labeler = std::shared_ptr<TrafficLabeler>(new TrafficLabeler(filepath));
    instances[filepath] = labeler;
    return labeler;  // Return shared pointer, map keeps instance alive
}

/**
 * @brief Log a packet to the CSV file (thread-safe, buffered)
 * @param entry LabelEntry containing all packet metadata
 * @return true if write succeeded, false otherwise
 */
bool TrafficLabeler::logPacket(const LabelEntry& entry) {
    std::lock_guard<std::mutex> lock(writeMutex);
    
    if (!fileInitialized || !csvFile.is_open()) {
        std::cerr << "[TrafficLabeler] ERROR: File not initialized or not open: " << filepath << std::endl;
        return false;
    }
    
    try {
        // Buffer the CSV line instead of immediate write
        buffer.push_back(entry.toCSV());
        packetCount++;
        
        // Flush periodically (every FLUSH_THRESHOLD packets)
        if (buffer.size() >= FLUSH_THRESHOLD) {
            flushBuffer();
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[TrafficLabeler] ERROR buffering CSV entry: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Flush buffered entries to disk (called internally)
 */
void TrafficLabeler::flushBuffer() {
    if (buffer.empty()) {
        return;
    }
    
    try {
        // Write all buffered lines with single newline (no flush per line)
        for (const auto& line : buffer) {
            csvFile << line << '\n';  // Just newline, no flush
        }
        
        // Single flush for entire batch
        csvFile.flush();
        
        // Clear buffer
        buffer.clear();
        
    } catch (const std::exception& e) {
        std::cerr << "[TrafficLabeler] ERROR flushing buffer: " << e.what() << std::endl;
    }
}

/**
 * @brief Close CSV file (called automatically on destruction)
 */
void TrafficLabeler::close() {
    std::lock_guard<std::mutex> lock(writeMutex);
    
    // Flush remaining buffer before closing
    if (!buffer.empty()) {
        flushBuffer();
    }
    
    if (csvFile.is_open()) {
        csvFile.close();
    }
    fileInitialized = false;
    std::cout << "[TrafficLabeler] Closed labeler for file: " << filepath << std::endl;
}

} // namespace ddosimu5g
