//
// DynamicTrafficSender - Generic Dynamic Traffic Application
//
// Generates different traffic patterns based on JSON schedule:
//  - UDP: Constant bit rate or bursty UDP traffic
//  - TCP: Connection-based data transfer  
//  - HTTP: Request-response patterns
//  - DNS: Query-response patterns
//
// Traffic parameters are configured via external JSON file,
// allowing flexible definition of any traffic profile (IoT, web, etc.)
//
// Example JSON format:
// {
//   "ueId": 0,
//   "schedule": [
//     {
//       "type": "udp",
//       "startTime": 0,
//       "duration": 60,
//       "destAddress": "iotServer",
//       "destPort": 5683,
//       "messageLength": 128,
//       "sendInterval": 60.0,
//       "burstSize": 1
//     }
//   ]
// }
//

package ddosimu5g.apps.dynamic;

import inet.applications.contract.IApp;

simple DynamicTrafficSender like IApp {
    parameters:
        @display("i=block/app");
        @class(ddosimu5g::DynamicTrafficSender);
        
        // Traffic schedule configuration
        string trafficScheduleFile = default("");  // Path to JSON file
        string labelFilePath = default("");  // Path to label CSV output file (optional)
        
        // Socket configuration
        int localPort = default(3000);  // Base local port
        
        // Statistics
        @signal[packetSent](type=inet::Packet);
        @signal[packetReceived](type=inet::Packet);
        @signal[trafficSwitch](type=long);
        
        @statistic[packetSent](title="packets sent"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[trafficSwitch](title="traffic type switches"; source=trafficSwitch; record=count,vector; interpolationmode=none);
    
    gates:
        output socketOut @labels(UdpControlInfo/down,TcpCommand/down);
        input socketIn @labels(UdpControlInfo/up,TcpCommand/up);
}