//
// Copyright (C) 2005 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.applications.udpapp;

import inet.applications.contract.IApp;

//
// Video streaming client.
//
// This module sends one "video streaming request" to serverAddress:serverPort at time startTime
// and receives stream from ~UdpVideoStreamServer server.
//
// @see ~UdpVideoStreamServer
//
simple UdpVideoStreamClient like IApp
{
    parameters:
        int localPort = default(-1);  // local port (-1: use ephemeral port)
        string serverAddress;  // server address
        int serverPort;  // server port
        double startTime @unit(s) = default(1s);
        @display("i=block/app");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);    // extra time after lifecycle stop operation finished
        double stopOperationTimeout @unit(s) = default(2s);    // timeout value for lifecycle stop operation
        @signal[packetReceived](type=inet::Packet);
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[throughput](title="throughput"; unit=bps; source="throughput(packetReceived)"; record=vector);
        @statistic[endToEndDelay](title="end-to-end delay"; source="dataAge(packetReceived)"; unit=s; record=histogram,vector; interpolationmode=none);
        
        
        //LTH added this -> this is for Statistics on network performance. 
        
        @statistic[packets](title="packets"; source=packetReceived; record=count; unit=pk);
        // the statistical value is the length of the packet
        @statistic[packetLengths](title="packet lengths"; source=packetLength(packetReceived); record=sum,histogram,vector; unit=b; interpolationmode=none);
        // the statistical value is the data rate of the packets
        @statistic[dataRate](title="data rate"; source=throughput(packetReceived); record=vector; unit=bps; interpolationmode=linear);
        // the statistical value is the time difference of the current simulation time and the creation time of the bit
        @statistic[bitLifeTime](title="bit life time"; source=weightTimes(lengthWeightedValuePerRegion(lifeTimePerRegion(packetReceived))); record=histogram?; unit=s; interpolationmode=none);
        // the statistical value is the time difference of the current simulation time and the creation time of any bit in the region
        @statistic[meanBitLifeTimePerPacket](title="mean bit life time per packet"; source=weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived))); record=vector,histogram; unit=s; interpolationmode=none);
        // the statistical value is the difference of subsequent values of the mean bit elapsed time per packet
        @statistic[packetJitter](title="packet jitter"; source=jitter(weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived)))); record=vector,histogram; unit=s; interpolationmode=none);
        // the statistical value is the difference of the current and the mean bit elapsed time per packet
        @statistic[packetDelayDifferenceToMean](title="packet delay difference to mean"; source=differenceToMean(weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived)))); record=vector,histogram; unit=s; interpolationmode=none);
        // the statistical value is the variation of the mean bit elapsed time per packet
        @statistic[packetDelayVariation](title="packet delay variation"; source=stddev(weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived)))); record=vector,histogram; unit=s);
        
        /////////
        
        
    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
}

