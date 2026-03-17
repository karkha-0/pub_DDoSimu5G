//
// BaseAttackApp - NED definition
//

package ddosimu5g.apps.adversarialApps;

import inet.applications.contract.IApp;

simple BaseAttackApp like IApp
{
    parameters:
        @display("i=block/app");
        @class(ddosimu5g::BaseAttackApp);
        
        // Timing parameters 
        double startTime @unit(s) @mutable = default(-1s);  // -1 means not started (placeholder)
        double stopTime @unit(s) @mutable = default(-1s);   // -1 means no stop time set
        
        // Attack configuration
        int benignSlotEnd @mutable = default(9);  // Last slot index for benign apps
        string attackProfileFile = default("");
        string attackType @mutable = default("generic");
        int messageLength @unit(B) @mutable = default(1024B);
        double sendInterval @unit(s) @mutable = default(0.001s);
        bool burstMode @mutable = default(false);
        int initialRate @mutable = default(100);
        int peakRate @mutable = default(1000);
        double rampDuration @unit(s) @mutable = default(0s);
        double burstOnDuration @unit(s) @mutable = default(0s);
        double burstOffDuration @unit(s) @mutable = default(0s);
        
        // Target configuration
        string destAddress @mutable = default("");
        int destPort @mutable = default(-1);
        
        // Labeling
        string labelFilePath = default("");
        
        // Statistics
        @signal[packetSent](type=long);
        @signal[bytesSent](type=long);
        @signal[attackStart](type=long);
        @signal[attackStop](type=long);
        
        @statistic[packetsSent](title="attack packets sent"; source=packetSent; record=count,sum,vector; interpolationmode=none);
        @statistic[bytesSent](title="attack bytes sent"; source=bytesSent; record=sum,vector; interpolationmode=none);
        @statistic[attackActive](title="attack activity"; source=attackStart,attackStop; record=vector; interpolationmode=sample-hold);
        
    gates:
        input socketIn @labels(UdpControlInfo/up,TcpCommand/up);
        output socketOut @labels(UdpControlInfo/down,TcpCommand/down);
}
