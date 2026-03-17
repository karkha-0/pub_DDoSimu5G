//
// UdpFloodAttack - NED definition
//

package ddosimu5g.apps.adversarialApps.ddos;

import ddosimu5g.apps.adversarialApps.BaseAttackApp;

simple UdpFloodAttack extends BaseAttackApp
{
    parameters:
        @class(ddosimu5g::UdpFloodAttack);
        attackType = default("udp_flood");
        
        // UDP flood specific parameters
        int packetSize @unit(B) = default(1400B);
        double packetsPerSecond = default(1000);
        bool enableSpoofing = default(false);
        double jitter = default(0.0);  // Timing jitter (0-1, fraction of interval)
        //int localPort = default(5000 + ancestorIndex(2));  // Unique per UE
        int localPort = default(intuniform(10000, 65000)); // Random source port
        
        // Interface
        string interfaceTableModule = default("^.^.interfaceTable");
}
