//
// SilentTcp.h — Extends INET Tcp to silently drop segments that
// arrive on closed ports (no RST back-scatter).
//

#ifndef DDOSIMU5G_COMMON_SILENTTCP_H
#define DDOSIMU5G_COMMON_SILENTTCP_H

#include "inet/transportlayer/tcp/Tcp.h"

namespace ddosimu5g {

/**
 * A Tcp subclass that overrides segmentArrivalWhileClosed() to
 * silently discard the segment instead of replying with a RST.
 *
 * In the standard INET Tcp, any SYN arriving at a port with no
 * listener triggers a temporary TcpConnection that sends an RST
 * back.  When a SYN-flood attack targets such a port, this creates
 * tens of thousands of DL RST packets that inflate PCAP counts and
 * create labelling artifacts.  SilentTcp avoids this.
 */
class SilentTcp : public inet::tcp::Tcp
{
  protected:
    void segmentArrivalWhileClosed(inet::Packet *tcpSegment,
                                   const inet::Ptr<const inet::tcp::TcpHeader>& tcpHeader,
                                   inet::L3Address srcAddr,
                                   inet::L3Address destAddr) override;
};

}  // namespace ddosimu5g

#endif
