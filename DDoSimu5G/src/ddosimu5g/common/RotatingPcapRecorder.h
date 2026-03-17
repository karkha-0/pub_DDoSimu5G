//
// RotatingPcapRecorder.h - PCAP recorder with automatic file rotation
//
// Extended from inet::PcapRecorder to support rotation by packet count or file size.
// Automatically closes current file and opens a new one when thresholds are reached.
//

#ifndef DDOSIMU5G_COMMON_ROTATINGPCAPRECORDER_H
#define DDOSIMU5G_COMMON_ROTATINGPCAPRECORDER_H

#include "inet/common/packet/recorder/PcapRecorder.h"
#include "inet/common/packet/recorder/PcapWriter.h"
#include "inet/common/packet/recorder/PcapngWriter.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>
#include <string>
#include <cstdint>

namespace ddosimu5g {

using omnetpp::cPacket;
using omnetpp::cComponent;
using inet::Direction;

/**
 * @brief Extended PcapRecorder with automatic file rotation
 * 
 * Extends inet::PcapRecorder to rotate output PCAP files when:
 * - Packet count reaches maxPacketsPerFile, OR
 * - File size reaches maxMegabytesPerFile
 * 
 * Rotated files are named: originalname_001.pcapng, originalname_002.pcapng, etc.
 */
class RotatingPcapRecorder : public inet::PcapRecorder {
protected:
    // Rotation configuration
    bool enableFileRotation = false;
    int maxPacketsPerFile = 0;
    int maxMegabytesPerFile = 0;
    std::string rotationDateTimeFormat;
    
    // Rotation tracking
    int currentRotationIndex = 0;
    int packetsInCurrentFile = 0;
    uint64_t bytesInCurrentFile = 0;
    std::string baseFileName;
    
    virtual void initialize() override;
    
    /**
     * @brief Override to track packet count and file size for rotation
     */
    virtual void recordPacket(const cPacket *cpacket, Direction direction, cComponent *source) override;
    
    /**
     * @brief Check if rotation threshold is reached and rotate if needed
     */
    virtual void checkAndRotateIfNeeded();
    
    /**
     * @brief Rotate to a new PCAP file
     */
    virtual void rotateFile();
    
    /**
     * @brief Generate next rotated filename
     */
    virtual std::string generateRotatedFileName();
};

} // namespace ddosimu5g

#endif
