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

#ifndef BASEATTACKAPP_H
#define BASEATTACKAPP_H

#include <inet/applications/base/ApplicationBase.h>
#include <inet/networklayer/common/L3Address.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/common/packet/Packet.h>
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/contract/IInterfaceTable.h>
#include <inet/networklayer/ipv4/Ipv4InterfaceData.h>

#include <omnetpp.h>
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "ddosimu5g/apps/dynamic/trafficlabel/TrafficLabeler.h"
#include "ddosimu5g/apps/dynamic/trafficlabel/LabelEntry.h"
#include "ddosimu5g/trafficcontroller/AttackRegistry.h"

using namespace omnetpp;
using namespace inet;

namespace ddosimu5g {

/**
 * @brief Configuration structure for attack parameters parsed from JSON profile
 * 
 * Contains all configuration parameters for DDoS attacks including timing,
 * rate patterns, target information, and behavior modes.
 */
struct AttackConfig {
    std::string attackType;          ///< Attack type: "udp_flood", "tcp_syn_flood", etc.
    std::string attackStyle;         ///< Style: "intense", "stealthy", "pulsing", "ramping", "slowrate"
    std::string behaviorMode;        ///< Behavior: "coexistence", "replace", "hybrid"
    
    // Timing parameters
    double dormantDuration;          ///< Time between infection and attack start (seconds)
    double attackDuration;           ///< Total attack duration (seconds)
    
    // Rate pattern parameters
    int initialRate;                 ///< Starting packet rate (pps)
    int peakRate;                    ///< Maximum packet rate (pps)
    double rampDuration;             ///< Time to reach peak rate (seconds)
    double burstOnDuration;          ///< Burst ON period (seconds)
    double burstOffDuration;         ///< Burst OFF period (seconds)
    
    // Target configuration
    std::vector<std::string> targets; ///< List of target addresses
    int targetPort;                   ///< Target port number
    
    // Attack-specific parameters
    int packetSize;                   ///< Packet size in bytes
    bool enableSpoofing;              ///< Enable source IP spoofing (UDP-based attacks)

    // DNS-specific parameters:
    std::string queryDomain;          ///< Domain name used for DNS queries
    double amplificationFactor;       ///< Expected amplification factor for DNS responses
    std::string openResolvers;;      ///< Comma-separated list of open DNS resolvers

    // HTTP-specific parameters:
    int numConnections;               ///< Number of concurrent TCP connections (HTTP)
    std::string httpMethod;           ///< HTTP method: "GET" or "POST"
    std::string requestPaths;         ///< Comma-separated HTTP request paths
    std::string userAgent;            ///< User-Agent header for HTTP requests
    bool keepAlive;                   ///< HTTP Keep-Alive connection mode
    int contentLength;                ///< Content length for POST requests (bytes)

    /**
     * @brief Constructor with default values
     */
    AttackConfig() : 
        dormantDuration(0.0), attackDuration(300.0),
        initialRate(100), peakRate(1000), 
        rampDuration(60.0), burstOnDuration(30.0), burstOffDuration(60.0),
        targetPort(80), packetSize(1400), enableSpoofing(false),
        queryDomain(""), amplificationFactor(0.0), openResolvers(""),
        numConnections(10), httpMethod("GET"), requestPaths("/"),
        userAgent("Mozilla/5.0"), keepAlive(true), contentLength(0) {}
};

/**
 * @brief Abstract base class for all DDoS attack implementations
 * 
 * Provides common infrastructure for attack applications including:
 * - Factory pattern for creating correct attack subclass from JSON profile
 * - Attack lifecycle management (start, stop, scheduling)
 * - Rate pattern support (constant, ramp, burst)
 * - Behavior mode application (coexistence, replace, hybrid)
 * - Traffic labeling for ML training
 * - Statistics collection
 * 
 * Subclasses must override sendAttackPacket() to implement attack-specific logic.
 */
class BaseAttackApp : public ApplicationBase, public cListener {
protected:
    // Attack configuration
    int benignSlotEnd = 9;           ///< Default, but will be set by DataTrafficController
    std::string attackType;          ///< Attack type identifier
    AttackConfig config;             ///< Full attack configuration from JSON
    simtime_t startDelay;           ///< Delay before starting attack
    simtime_t duration;             ///< Attack duration
    simtime_t attackStartTime = -1; ///< Scheduled start time
    simtime_t attackStopTime = -1;  ///< Scheduled stop time
    int initialRate;                ///< Starting rate (pps/rps/qps)
    int peakRate;                   ///< Maximum rate
    double rampDuration;            ///< Time to ramp from initial to peak
    bool burstMode;                 ///< Enable burst ON/OFF cycles
    double burstOnDuration;         ///< Burst ON period duration
    double burstOffDuration;        ///< Burst OFF period duration
    
    // Current state for style management
    int currentRate;                ///< Current packets/sec
    bool inBurstOn;                 ///< Currently in burst ON period
    
    // Target configuration
    L3Address destAddress;          ///< Resolved destination address
    std::string destAddressStr;     ///< Destination address string
    int destPort = -1;              ///< Destination port
    
    // State management
    bool isActive = false;          ///< Attack currently active
    bool isConfigured = false;      ///< Configuration completed
    cMessage* startAttackTimer = nullptr;  ///< Timer for attack start
    cMessage* stopAttackTimer = nullptr;   ///< Timer for attack stop
    cMessage* sendTimer = nullptr;          ///< Timer for packet sending
    cMessage* rateUpdateTimer = nullptr;    ///< Timer for ramping rate updates
    cMessage* burstToggleTimer = nullptr;   ///< Timer for burst ON/OFF cycles
    cMessage* behaviorModeTimer = nullptr;  ///< Timer for behavior mode application
    
    // Labeling - write directly to CSV
    std::shared_ptr<TrafficLabeler> labeler;  ///< Traffic labeler for ML training
    std::string labelFilePath;                ///< Path to label file
    int ueId = -1;                            ///< UE identifier
    int appId = -1;                           ///< Application identifier
    long packetCounter = 0;                   ///< Packet counter for labeling
    
    // Statistics
    simsignal_t packetSentSignal;    ///< Signal emitted when packet sent
    simsignal_t bytesSentSignal;     ///< Signal emitted for bytes sent
    simsignal_t attackStartSignal;   ///< Signal emitted when attack starts
    simsignal_t attackStopSignal;    ///< Signal emitted when attack stops
    simsignal_t currentRateSignal;   ///< Signal for tracking rate changes
    
    int packetsSent = 0;             ///< Total packets sent counter
    int64_t bytesSent = 0;           ///< Total bytes sent counter

    int lastPrintedRate = -1;        ///< Track rate changes and print only on change
    
    // Adaptive rate control - prevent UE bottleneck
    long droppedPackets = 0;         ///< Counter for dropped packets
    long currentQueueLength = 0;     ///< Current interface queue length
    double adaptiveRateMultiplier = 1.0;  ///< Rate reduction factor (0.2-1.0)
    simtime_t lastAdaptiveCheck = 0; ///< Last time adaptive control ran
    static constexpr long QUEUE_THROTTLE_THRESHOLD = 100;  ///< Queue threshold for throttling
    static constexpr double MIN_RATE_MULTIPLIER = 0.2;     ///< Minimum 20% of target rate
    static constexpr double ADAPTIVE_CHECK_INTERVAL = 1.0; ///< Check every 1 second

    
    /**
     * @brief Parse attack profile from JSON file
     * @param jsonFilePath Path to JSON profile file
     * @return Parsed AttackConfig structure
     */
    AttackConfig parseAttackProfile(const char* jsonFilePath);
    
    /**
     * @brief Adjust sending rate based on packet drops and queue congestion
     * Implements adaptive throttling to prevent UE bottleneck
     */
    virtual void adjustAdaptiveRate();
    
    /**
     * @brief Apply attack style to set rate pattern and burst mode
     * @param attackStyle Style from JSON (intense/stealthy/pulsing/ramping/slowrate)
     */
    void applyAttackStyle(const std::string& attackStyle);
    
    /**
     * @brief Validate attack configuration
     * @param config Configuration to validate
     * @return true if valid, false otherwise
     */
    bool validateConfig(const AttackConfig& config);
    
    /**
     * @brief Apply behavior mode to benign applications
     * @param behaviorMode Mode to apply (coexistence/replace/hybrid)
     * 
     * Modifies benign application behavior based on mode:
     * - coexistence: No changes (benign apps continue normally)
     * - replace: Stop all benign apps
     * - hybrid: Reduce benign app rates by 50%
     */
    void applyBehaviorMode(const std::string& behaviorMode);
        
    /**
     * @brief Smart console output - prevents spam during high-rate attacks
     * Automatically adjusts verbosity based on attack intensitybenignSlotEndIndex;
     * 
     * @param protocol Protocol name ("UDP", "TCP", "HTTP", etc.)
     * @param extraInfo Optional extra details (can be empty)
     */
    virtual void printProgress(const char* protocol, const char* extraInfo = "");

public:
    BaseAttackApp();
    virtual ~BaseAttackApp();
    
    /**
     * @brief Factory method to create attack app from JSON profile
     * @param profileFile Path to attack profile JSON file
     * @param parent Parent module (UE)
     * @param appIndex Application slot index (10-19 for attacks default)
     * @param benignSlotEndIndex Last index of benign apps (used for behavior modes)
     * @return Pointer to created attack application
     * 
     * This factory method:
     * 1. Parses JSON profile to get attackType
     * 2. Uses AttackRegistry to get module name (or fallback)
     * 3. Creates module dynamically
     * 4. Configures module with profile parameters
     * 5. Returns pointer to created module
     */
    static BaseAttackApp* createFromProfile(const char* profileFile,
                                            cModule* parent,
                                            int appIndex,
                                            int benignSlotEndIndex);
    
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage* msg) override;
    virtual void finish() override;
    virtual void handleStartOperation(LifecycleOperation* operation) override;
    virtual void handleStopOperation(LifecycleOperation* operation) override;
    virtual void handleCrashOperation(LifecycleOperation* operation) override;
    
    /**
     * @brief Start attack with style support
     */
    virtual void startAttack();
    
    /**
     * @brief Stop attack
     */
    virtual void stopAttack();
    
    /**
     * @brief Execute one attack iteration
     */
    virtual void executeAttack();
    
    /**
     * @brief Send one attack packet (must be overridden by subclasses)
     * 
     * Virtual with default implementation for placeholders.
     * Subclasses MUST override this to implement actual attack logic.
     * Default implementation does nothing (used for placeholder modules).
     */
    virtual void sendAttackPacket();
    
    /**
     * @brief Update attack rate for ramping pattern
     */
    void updateAttackRate();
    
    /**
     * @brief Toggle burst ON/OFF state
     */
    void toggleBurst();
    
    /**
     * @brief Calculate current send interval based on rate
     * @return Interval in seconds
     */
    double calculateCurrentInterval();
    
    /**
     * @brief Schedule next attack packet with style logic
     */
    void scheduleNextAttack();
    
    /**
     * @brief Configure attack (resolve addresses, etc.)
     */
    virtual void configureAttack();
    
    /**
     * @brief Schedule attack lifecycle (start and stop timers)
     */
    void scheduleAttackLifecycle();
    
    /**
     * @brief Schedule attack start (called by DataTrafficController)
     * @param startTime Time to start attack
     * @param stopTime Time to stop attack
     */
    virtual void scheduleAttackStart(simtime_t startTime, simtime_t stopTime);
    
    /**
     * @brief Log attack packet to CSV via TrafficLabeler
     * @param direction "TX" or "RX"
     * @param protocol Protocol name (UDP, TCP, DNS, etc.)
     * @param srcAddr Source IP address (real sender IP)
     * @param srcPort Source port
     * @param destAddr Destination IP address
     * @param destPort Destination port
     * @param size Packet size in bytes
     * @param spoofingEnabled Whether IP spoofing is enabled
     * @param spoofedSrcIP Spoofed source IP (victim address in amplification attacks)
     * @param label Traffic label ("malicious" or "benign"), defaults to "malicious"
     */
    virtual void logAttackPacket(const char* direction, 
                                const char* protocol,
                                const inet::L3Address& srcAddr, int srcPort,
                                const inet::L3Address& destAddr, int destPort,
                                int size,
                                bool spoofingEnabled = false,
                                const char* spoofedSrcIP = "",
                                const char* label = "malicious");
    
    /**
     * @brief Resolve destination address string to L3Address
     * @param addrStr Address string (hostname or IP)
     * @return Resolved L3Address
     */
    L3Address resolveDestAddress(const char* addrStr);
    
public:
    /**
     * @brief Handle signals for packet drops and queue monitoring
     */
    virtual void receiveSignal(cComponent* source, simsignal_t signalID,
                              intval_t value, cObject* details) override;
};

} // namespace ddosimu5g

#endif // BASEATTACKAPP_H
