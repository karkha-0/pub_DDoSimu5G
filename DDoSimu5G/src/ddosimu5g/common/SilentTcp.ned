//
// SilentTcp — drops incoming segments to closed ports instead of
// sending TCP RST.  This prevents back-scatter when SYN-flood
// attacks target a port with no listener.
//
// Usage:  *.generalServer.tcp.typename = "ddosimu5g.common.SilentTcp"
//

package ddosimu5g.common;

import inet.transportlayer.tcp.Tcp;

simple SilentTcp extends Tcp
{
    @class(ddosimu5g::SilentTcp);
}
