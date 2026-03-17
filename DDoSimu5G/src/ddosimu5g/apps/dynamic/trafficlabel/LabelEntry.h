//
// LabelEntry.h - Label entry data structure
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

#ifndef __DDOSIMU5G_LABELENTRY_H
#define __DDOSIMU5G_LABELENTRY_H

#include <omnetpp.h>
#include <string>

namespace ddosimu5g {

/**
 * @brief Packet metadata for out-of-band traffic labeling
 * 
 * This structure holds all information needed to create a CSV label traffic packet entry.
 *  * Each field corresponds to a CSV column.
 */
struct LabelEntry {
    omnetpp::simtime_t timestamp;       // Simulation time when packet was sent/received
    int packetNumber;                   // Sequential packet counter
    std::string srcModule;              // Source module name (e.g., "ue[0].app[2]")
    std::string srcIP;                  // Source IP address (for 5-tuple matching)
    int srcPort;                        // Source port number (for 5-tuple matching)
    std::string direction;              // "TX" for sent packets, "RX" for received packets
    std::string trafficType;            // Traffic pattern (e.g., "iot_smart_meter")
    std::string protocol;               // Protocol name (UDP, DNS, TCP, HTTP, MQTT, CoAP, etc.)
    std::string destAddress;            // Destination IP/hostname
    int destPort;                       // Destination port number
    int packetSize;                     // Packet size in bytes
    std::string label;                  // Traffic label (benign, malicious, attack_type, etc.)
    bool spoofingEnabled;               // Is IP spoofing enabled for this packet?
    std::string spoofedSrcIP;           // Spoofed source IP (victim IP)
    
    /**
     * @brief Converts entry to CSV format
     * @return CSV line (without newline)
     */
    std::string toCSV() const {
        std::ostringstream oss;
        oss << timestamp.dbl() << ","
            << packetNumber << ","
            << srcModule << ","
            << srcIP << ","
            << srcPort << ","
            << direction << ","
            << trafficType << ","
            << protocol << ","
            << destAddress << ","
            << destPort << ","
            << packetSize << ","
            << label << ","
            << (spoofingEnabled ? "1" : "0") << ","
            << (spoofingEnabled ? spoofedSrcIP : "");
        return oss.str();
    }
    
    /**
     * @brief Returns CSV header
     * @return CSV header line (without newline)
     */
    static std::string getCSVHeader() {
        return "timestamp,packet_num,src_module,src_ip,src_port,direction,traffic_type,protocol,dest_ip,dest_port,packet_size,label,spoofing_enabled,spoofed_src_ip";
    }
};

} // namespace ddosimu5g

#endif
