//
// SilentTcp.cc — silently drops segments to closed ports.
//

#include "ddosimu5g/common/SilentTcp.h"

using namespace inet;
using namespace inet::tcp;

namespace ddosimu5g {

Define_Module(SilentTcp);

void SilentTcp::segmentArrivalWhileClosed(Packet *tcpSegment,
                                          const Ptr<const TcpHeader>& tcpHeader,
                                          L3Address srcAddr,
                                          L3Address destAddr)
{
    // Instead of creating a temporary TcpConnection that sends RST,
    // just log and discard.
    EV_WARN << "[SilentTcp] Dropping segment to closed port "
            << tcpHeader->getDestPort()
            << " from " << srcAddr << ":" << tcpHeader->getSrcPort()
            << " (no RST sent)" << endl;

    delete tcpSegment;
}

}  // namespace ddosimu5g
