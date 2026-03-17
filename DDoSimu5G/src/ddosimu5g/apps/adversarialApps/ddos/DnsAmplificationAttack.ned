//
// DnsAmplificationAttack - NED definition
//

package ddosimu5g.apps.adversarialApps.ddos;

import ddosimu5g.apps.adversarialApps.BaseAttackApp;

simple DnsAmplificationAttack extends BaseAttackApp
{
    parameters:
        @class(ddosimu5g::DnsAmplificationAttack);
        attackType = default("dns_amplification");
        
        // DNS amplification specific parameters
        bool enableSpoofing = default(false);  // Default FALSE for 5G
        string victimAddress = default("");  // Target to amplify attack against
        string openResolvers = default("8.8.8.8 1.1.1.1");  // Space-separated list
        double queriesPerSecond = default(500);
        int queryType = default(255);  // 255 = ANY (max amplification)
        //int localPort = default(6000 + ancestorIndex(2));
        int localPort = default(intuniform(10000, 65000)); // Random source port
        string queryDomain = default("isc.org");  // Domain for DNS query (affects amplification)
        double amplificationFactor = default(28.0);  // Expected amplification (for statistics)


        
        // Override destination - queries go to resolvers, not victim
        destAddress = default("");
        destPort = default(53);
        
        // Interface
        string interfaceTableModule = default("^.^.interfaceTable");
}
