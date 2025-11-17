// 
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
// 
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself, 
// and cannot be removed from it.
// 

package modifiedExternalFiles.Simu5G.src.common.binder; //need to be changed just commented to remove error
//package simu5g.common.binder;  //need to be changed just commented to remove error

// 
// Binder module
//
// It is used to store any information related to 
// the network that needs to be accessed via method calls, e.g. 
// for control-plane functionalities.
// There must be one (and one only) instance of the module in the network
//
simple Binder  // need to be changed just commented to remove error
{
    parameters:
        int blerShift = default(0);
        double maxDataRatePerRb @unit("Mbps") = default(1.16Mbps);
        bool printTrafficGeneratorConfig = default(false);
        @display("i=block/cogwheel");
        
        // LTH added this 
        @signal[testMsg]; 
        @statistic[testMsg](title="Test statistics from binder"; unit=""; source="testMsg"; record=vector);
        @signal[simNodeIds]; 
        @statistic[simNodeIds](title="Simulation UE Node IDs"; unit=""; source="simNodeIds"; record=vector);
}
