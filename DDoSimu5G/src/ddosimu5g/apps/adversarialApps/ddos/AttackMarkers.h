//
// AttackMarkers.h - DSCP/TOS/IPID traffic identification markers for DDoS attacks
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
//  Marker summary:
//    Attack Type    | TOS  | DSCP | 
//    -------------------------------
//    UDP Flood      | 0xD0 |  52  | 
//    TCP SYN Flood  | 0xD2 |  52  | 
//    HTTP Flood     | 0xD4 |  53  | 
//    DNS Amplif.    | 0xD8 |  54  | 
//

#ifndef __DDOSIMU5G_ATTACKMARKERS_H_
#define __DDOSIMU5G_ATTACKMARKERS_H_

#include <cstdint>

namespace ddosimu5g {

/**
 * @brief Traffic identification markers embedded in raw IPv4 attack packets.
 *
 * Each attack sets two IPv4-layer fields that survive the 5G tunnel and are
 * visible in PCAP captures for offline traffic classification:
 *
 *  TOS byte  (Ipv4Header::setTypeOfService)
 *  — Visible in Wireshark as the DSCP field.
 *  — TOS = (DSCP << 2) | ECN

 */
struct AttackMarkers {
    // ── IPv4 TOS byte ────────────────────────────────────────────────────────
    static constexpr uint8_t TOS_UDP_FLOOD      = 0xD0;  // DSCP=52, ECN=0
    static constexpr uint8_t TOS_TCP_SYN_FLOOD  = 0xD2;  // DSCP=52, ECN=2
    static constexpr uint8_t TOS_HTTP_FLOOD     = 0xD4;  // DSCP=53, ECN=0
    static constexpr uint8_t TOS_DNS_AMP        = 0xD8;  // DSCP=54, ECN=0
};

} // namespace ddosimu5g

#endif // __DDOSIMU5G_ATTACKMARKERS_H_
