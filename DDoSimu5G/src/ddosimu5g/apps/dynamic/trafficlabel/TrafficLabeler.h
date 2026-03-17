//
// TrafficLabeler.h - Traffic labeling system
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

#ifndef __DDOSIMU5G_TRAFFICLABELER_H
#define __DDOSIMU5G_TRAFFICLABELER_H

#include <omnetpp.h>
#include <string>
#include <fstream>
#include <mutex>
#include <map>
#include <memory>

#include "ddosimu5g/apps/dynamic/trafficlabel/LabelEntry.h"

namespace ddosimu5g {

/**
 * @brief Thread-safe singleton for out-of-band traffic labeling
 * 
 * Manages CSV label files. Multiple concurrent
 * protocol handlers can safely write to the same CSV file using this class.
 * One instance per CSV file path (typically one per UE).
 */
class TrafficLabeler {
public:

    static std::shared_ptr<TrafficLabeler> getInstance(const std::string& filepath);
    bool logPacket(const LabelEntry& entry);
     void close();
    ~TrafficLabeler();
    
    /**
     * @brief Check if CSV file is open and ready
     * @return true if file is open
     */
    bool isOpen() const { return fileInitialized; }
    
    /**
     * @brief Get filepath associated with this labeler
     * @return CSV file path
     */
    std::string getFilePath() const { return filepath; }


private:
    /**
     * @brief Private constructor for singleton pattern
     * @param filepath Path to CSV file
     */
    explicit TrafficLabeler(const std::string& filepath);
    
    // Prevent copying
    TrafficLabeler(const TrafficLabeler&) = delete;
    TrafficLabeler& operator=(const TrafficLabeler&) = delete;
    
    /**
     * @brief Flush buffered entries to disk
     */
    void flushBuffer();
    
    std::string filepath;               // CSV file path
    mutable std::mutex writeMutex;      // Protects concurrent writes
    bool fileInitialized;               // Track if file has been initialized
    std::ofstream csvFile;              // Keep file open persistently
    
    // Buffering for performance
    std::vector<std::string> buffer;    // Buffer for CSV lines
    size_t packetCount;                 // Counter for periodic flush
    static const size_t FLUSH_THRESHOLD = 1000;  // Flush every 1000 packets
    
    // Singleton instance management
    static std::map<std::string, std::shared_ptr<TrafficLabeler>> instances;
    static std::mutex instanceMutex;
};

} // namespace ddosimu5g

#endif
