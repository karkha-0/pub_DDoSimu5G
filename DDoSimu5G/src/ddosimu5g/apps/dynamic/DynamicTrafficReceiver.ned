//
// DynamicTrafficReceiver - Modular receiver with protocol handlers
// Routes traffic to protocol-specific handlers (UDP, DNS, TCP, HTTP)
//
package ddosimu5g.apps.dynamic;

import inet.applications.contract.IApp;

simple DynamicTrafficReceiver like IApp {
    parameters:
        @display("i=block/server");
        @class(ddosimu5g::DynamicTrafficReceiver);
        
        // Protocol-specific ports (optional, configure only needed ones)
        int udpPort = default(-1);   // UDP echo port
        int dnsPort = default(-1);   // DNS server port
        int tcpPort = default(-1);   // TCP server port
        int httpPort = default(-1);  // HTTP server port
        
        // Behavior
        bool echoMode = default(true);  // Send replies to requests
        
        // Statistics
        @signal[packetReceived](type=inet::Packet);
        @signal[packetSent](type=inet::Packet);
        
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[packetSent](title="packets sent"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
    
    gates:
        output socketOut @labels(UdpControlInfo/down,TcpCommand/down);
        input socketIn @labels(UdpControlInfo/up,TcpCommand/up);
}
