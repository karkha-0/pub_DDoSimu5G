//
// ProtocolHandler.h - Base protocol handler interface
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

#ifndef __DDOSIMU5G_PROTOCOLHANDLER_H
#define __DDOSIMU5G_PROTOCOLHANDLER_H

#include <omnetpp.h>
#include "nlohmann/json.hpp"
#include "../trafficlabel/LabelEntry.h"
#include <memory>
#include "ddosimu5g/apps/dynamic/trafficlabel/TrafficLabeler.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/common/TimeTag_m.h"

namespace ddosimu5g {

// Forward declaration
class DynamicTrafficSender;

/**
 * @brief Base interface for all protocol handlers
 * 
 * Each protocol (UDP, DNS, TCP, HTTP, MQTT, CoAP, etc.) implements this
 * interface to handle its specific traffic generation and logging.
 * This enables easy addition of new protocols without modifying core code.
 */
class ProtocolHandler {
public:
    /**
     * @brief Constructor
     * @param parent Pointer to parent DynamicTrafficSender module
     * @param labeler Shared labeler for CSV logging
     * @param appId App index (0-9) for this handler
     */
    ProtocolHandler(DynamicTrafficSender* parent, 
                    std::shared_ptr<TrafficLabeler> labeler,
                    int appId)
        : parent(parent), labeler(labeler), appId(appId), 
          currentTrafficType("unknown"), currentLabel("benign"),
          packetsSent(0), packetsReceived(0), isActive(false) {}
    
    virtual ~ProtocolHandler() = default;
    
    /**
     * @brief Initialize protocol handler from JSON configuration
     * @param config JSON object with protocol-specific parameters
     */
    virtual void start(const nlohmann::json& config) = 0;
    
    /**
     * @brief Stop current traffic generation
     */
    virtual void stop() = 0;
    
    /**
     * @brief Handle incoming self-messages (timers)
     * @param msg Message to handle
     */
    virtual void handleMessage(omnetpp::cMessage* msg) = 0;
    
    /**
     * @brief Process incoming socket messages (data from network)
     * @param msg Socket message to process (packet from peer)
     * 
     * This method should call the appropriate socket's processMessage()
     * to trigger callbacks like socketDataArrived().
     */
    virtual void processSocketMessage(omnetpp::cMessage* msg) = 0;
    
    /**
     * @brief Get protocol name (UDP, DNS, TCP, HTTP, etc.)
     * @return Protocol identifier string
     */
    virtual std::string getProtocolName() const = 0;
    
    /**
     * @brief Check if handler is currently active
     * @return true if traffic is being generated
     */
    bool getIsActive() const { return isActive; }
    
    /**
     * @brief Get current traffic type (e.g., "iot_smart_meter")
     * @return Traffic type string
     */
    std::string getCurrentTrafficType() const { return currentTrafficType; }
    
    /**
     * @brief Get app ID
     * @return App index (0-9)
     */
    int getAppId() const { return appId; }
    
    /**
     * @brief Get packets sent counter
     * @return Number of packets sent
     */
    int getPacketsSent() const { return packetsSent; }
    
    /**
     * @brief Get packets received counter
     * @return Number of packets received
     */
    int getPacketsReceived() const { return packetsReceived; }

protected:
    DynamicTrafficSender* parent;                  // Parent module
    std::shared_ptr<TrafficLabeler> labeler;       // CSV logger
    int appId;                                      // App index (0-9)
    
    std::string currentTrafficType;                // Current traffic pattern name
    std::string currentLabel;                      // Current label (benign/malicious)
    
    int packetsSent;                               // Packets sent counter
    int packetsReceived;                           // Packets received counter
    bool isActive;                                 // Is currently generating traffic
    
    /**
     * @brief Helper to log packet to CSV (auto-resolves source IP)
     * @param protocol Protocol name
     * @param destAddr Destination address
     * @param destPort Destination port
     * @param packetSize Packet size in bytes
     * @param srcPort Source port number
     * @param timestamp Packet timestamp
     */
    // void logPacketToCSV(const std::string& protocol,
    //                    const std::string& destAddr,
    //                    int destPort,
    //                    int packetSize,
    //                    int srcPort,
    //                    simtime_t timestamp);
    
    /**
     * @brief Helper to log packet to CSV with explicit source info
     * @param protocol Protocol name
     * @param destAddr Destination address
     * @param destPort Destination port
     * @param packetSize Packet size in bytes
     * @param srcAddr Source IP address
     * @param srcPort Source port number
     * @param timestamp Packet timestamp
     */
    void logPacketToCSV(const std::string& protocol,
                       const std::string& destAddr,
                       int destPort,
                       int packetSize,
                       //const std::string& srcAddr,
                       int srcPort,
                       omnetpp::simtime_t timestamp);


    // void logPacketToCSV(const std::string& protocol, 
    //                     inet::Packet* packet,
    //                     bool isTX);

};


} // namespace ddosimu5g

#endif
