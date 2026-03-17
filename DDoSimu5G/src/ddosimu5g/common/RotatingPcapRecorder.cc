//
// RotatingPcapRecorder.cc - Implementation of rotating PCAP recorder
//

#include "ddosimu5g/common/RotatingPcapRecorder.h"
#include <omnetpp.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>

using namespace omnetpp;
using namespace inet;

namespace ddosimu5g {

Define_Module(RotatingPcapRecorder);

void RotatingPcapRecorder::initialize()
{
    // Call parent initialize first
    PcapRecorder::initialize();
    
    // Get rotation parameters
    enableFileRotation = par("enableFileRotation");
    maxPacketsPerFile = par("maxPacketsPerFile");
    maxMegabytesPerFile = par("maxMegabytesPerFile");
    rotationDateTimeFormat = par("rotationDateTimeFormat").stdstringValue();
    
    // Store base filename for rotation
    baseFileName = par("pcapFile").stdstringValue();
    
    if (enableFileRotation && baseFileName.length() > 0) {
        std::cout << "[RotatingPcapRecorder] File rotation enabled: ";
        if (maxPacketsPerFile > 0)
            std::cout << "max " << maxPacketsPerFile << " packets/file";
        if (maxMegabytesPerFile > 0) {
            if (maxPacketsPerFile > 0) std::cout << " OR ";
            std::cout << "max " << maxMegabytesPerFile << " MB/file";
        }
        std::cout << std::endl;
        
        currentRotationIndex = 0;
        packetsInCurrentFile = 0;
        bytesInCurrentFile = 0;
    }
}

void RotatingPcapRecorder::recordPacket(const cPacket *cpacket, Direction direction, cComponent *source)
{
    // Call parent recordPacket (which writes to file)
    PcapRecorder::recordPacket(cpacket, direction, source);
    
    // Track packet for rotation (only if parent actually wrote the packet)
    if (auto packet = dynamic_cast<const Packet *>(cpacket)) {
        packetsInCurrentFile++;
        bytesInCurrentFile += packet->getByteLength();
        
        // Check rotation threshold
        if (enableFileRotation) {
            checkAndRotateIfNeeded();
        }
    }
}

void RotatingPcapRecorder::checkAndRotateIfNeeded()
{
    bool shouldRotate = false;
    std::string reason;
    
    if (maxPacketsPerFile > 0 && packetsInCurrentFile >= maxPacketsPerFile) {
        shouldRotate = true;
        reason = std::string("reached ") + std::to_string(packetsInCurrentFile) + 
                 " packets (max " + std::to_string(maxPacketsPerFile) + ")";
    }
    
    if (maxMegabytesPerFile > 0) {
        uint64_t maxBytes = (uint64_t)maxMegabytesPerFile * 1024 * 1024;
        if (bytesInCurrentFile >= maxBytes) {
            shouldRotate = true;
            if (!reason.empty()) reason += " AND ";
            reason = std::string("reached ") + std::to_string(bytesInCurrentFile / (1024*1024)) + 
                     " MB (max " + std::to_string(maxMegabytesPerFile) + " MB)";
        }
    }
    
    if (shouldRotate) {
        std::cout << "[RotatingPcapRecorder] Rotating file: " << reason << std::endl;
        rotateFile();
    }
}

void RotatingPcapRecorder::rotateFile()
{
    if (!pcapWriter || !pcapWriter->isOpen()) {
        std::cout << "[RotatingPcapRecorder] WARNING: Cannot rotate - no active writer or file not open" << std::endl;
        return;
    }
    
    // Close current file
    std::cout << "[RotatingPcapRecorder] Closing current file (index " << currentRotationIndex 
              << ", packets=" << packetsInCurrentFile << ", size=" << (bytesInCurrentFile / (1024*1024)) 
              << " MB)" << std::endl;
    pcapWriter->close();
    
    // Move to next index
    currentRotationIndex++;
    packetsInCurrentFile = 0;
    bytesInCurrentFile = 0;
    
    // Generate rotated filename
    std::string newFileName = generateRotatedFileName();
    
    // Open new file
    std::cout << "[RotatingPcapRecorder] Opening rotated file: " << newFileName << std::endl;
    pcapWriter->open(newFileName.c_str(), par("snaplen"));
    pcapWriter->setFlush(par("alwaysFlush"));
}

std::string RotatingPcapRecorder::generateRotatedFileName()
{
    std::ostringstream oss;
    
    // Find last dot to insert rotation index before extension
    size_t lastDot = baseFileName.rfind('.');
    
    if (lastDot != std::string::npos) {
        // Has extension (e.g., "upf.pcapng" -> "upf_001.pcapng")
        std::string nameWithoutExt = baseFileName.substr(0, lastDot);
        std::string ext = baseFileName.substr(lastDot);  // includes the dot
        
        oss << nameWithoutExt << "_"
            << std::setfill('0') << std::setw(3) << currentRotationIndex
            << ext;
    } else {
        // No extension (e.g., "upf" -> "upf_001")
        oss << baseFileName << "_"
            << std::setfill('0') << std::setw(3) << currentRotationIndex;
    }
    
    return oss.str();
}

} // namespace ddosimu5g
