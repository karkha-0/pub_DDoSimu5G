//
// TcpSynFloodAttack - NED definition
//

package ddosimu5g.apps.adversarialApps.ddos;

import ddosimu5g.apps.adversarialApps.BaseAttackApp;

simple TcpSynFloodAttack extends BaseAttackApp
{
    parameters:
        @class(ddosimu5g::TcpSynFloodAttack);
        attackType = default("tcp_syn_flood");
        
        // TCP SYN flood specific parameters
        bool enableSpoofing = default(true);
        string targetPorts = default("80 443 8080");  // Space-separated list
        int localPort = default(intuniform(10000, 65000)); // Random source port
        
        // Interface table module for finding cellular interface ID
        string interfaceTableModule = default("^.^.interfaceTable");

    gates:
        input rawIn @labels(Ipv4Header);   // Raw packet ingress from MessageDispatcher (replies)
        output rawOut @labels(Ipv4Header);  // Raw packet injection to MessageDispatcher
        
}
