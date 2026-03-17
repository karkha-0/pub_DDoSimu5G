//
// RotatingPcapRecorder - Extended PcapRecorder with automatic file rotation
//
// This module extends inet.common.packet.recorder.PcapRecorder to support
// automatic PCAP file rotation after reaching a threshold (by packet count or file size).
// This prevents oversized PCAP files during long-running simulations.
//
// Usage in INI:
//   *.upf.pppIf.pcapRecorder[0].typename = "ddosimu5g::RotatingPcapRecorder"
//   *.upf.pppIf.pcapRecorder[0].enableFileRotation = true
//   *.upf.pppIf.pcapRecorder[0].maxPacketsPerFile = 1000000
//   OR
//   *.upf.pppIf.pcapRecorder[0].maxMegabytesPerFile = 500
//

package ddosimu5g.common;

import inet.common.packet.recorder.PcapRecorder;

simple RotatingPcapRecorder extends PcapRecorder
{
    parameters:
        bool enableFileRotation = default(false);          // Enable automatic PCAP file rotation
        int maxPacketsPerFile = default(1000000);          // Rotate after this many packets (0 = disabled)
        int maxMegabytesPerFile = default(0);              // Rotate after this many MB (0 = disabled)
        string rotationDateTimeFormat = default("%Y%m%d_%H%M%S");  // Date/time suffix format in rotated filenames
}
