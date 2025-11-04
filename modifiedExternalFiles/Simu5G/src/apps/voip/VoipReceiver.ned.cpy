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

package simu5g.apps.voip;

import inet.applications.contract.IApp;


simple VoIPReceiver like IApp
{
    parameters:
        int localPort = default(3000);
        int emodel_Ie_ = default(5);
        int emodel_Bpl_ = default(10);
        int emodel_A_ = default(5);
        double emodel_Ro_ = default(93.2);
        double playout_delay @unit("s") = default(0s);
        int dim_buffer = default(20);
        double sampling_time @unit("s") = default(0.02s);

		// LTH commented these
//		@signal[voIPFrameLoss];
//        @statistic[voIPFrameLoss](title="VoIP Frame Loss"; unit="ratio"; source="voIPFrameLoss"; record=mean);
//        @signal[voIPFrameDelay];
//        @statistic[voIPFrameDelay](title="VoIP Frame Delay"; unit="s"; source="voIPFrameDelay"; record=mean,vector);
//        @signal[voIPPlayoutDelay];
//        @statistic[voIPPlayoutDelay](title="VoIP Playout Delay"; unit="s"; source="voIPPlayoutDelay"; record=mean,);
//        @signal[voIPPlayoutLoss];
//        @statistic[voIPPlayoutLoss](title="VoIP Playout Loss"; unit="ratio"; source="voIPPlayoutLoss"; record=mean);
//        @signal[voIPJitter];
//        @statistic[voIPJitter](title="VoIP Playout Loss"; unit="s"; source="voIPJitter"; record=mean);
//        @signal[voIPMos];
//        @statistic[voIPMos](title="VoIP Mos Signal"; unit="MOS"; source="voIPMos"; record=mean);
//        @signal[voIPTaildropLoss];
//        @statistic[voIPTaildropLoss](title="VoIP Tail Drop Loss"; unit="ratio"; source="voIPTaildropLoss"; record=mean);
//        @signal[voIPReceivedThroughput];
//        @statistic[voIPReceivedThroughput](title="Throughput received at the application level"; unit="Bps"; source="voIPReceivedThroughput"; record=mean,vector);
//        @display("i=block/source");
        
        // LTH addes this -> changed some of the statistics into histogram 
        //@statistic[voIPReceivedThroughput](title="voIPReceivedThroughput"; source="voIPReceivedThroughput"; unit=s; record=histogram,vector; interpolationmode=none);
        //@statistic[voIPFrameDelay](title="voIPFrameDelay"; source="voIPFrameDelay"; unit=s; record=histogram,vector; interpolationmode=none);
        //@statistic[voIPFrameLoss](title="voIPFrameLoss"; source="voIPFrameLoss"; unit=s; record=histogram,vector; interpolationmode=none);
        
        @signal[voIPFrameLoss];
        @statistic[voIPFrameLoss](title="VoIP Frame Loss"; unit="ratio"; source="voIPFrameLoss"; record=mean,vector, histogram);
        @signal[voIPFrameDelay];
        @statistic[voIPFrameDelay](title="VoIP Frame Delay"; unit="s"; source="voIPFrameDelay"; record=mean, vector, histogram);
        @signal[voIPPlayoutDelay];
        @statistic[voIPPlayoutDelay](title="VoIP Playout Delay"; unit="s"; source="voIPPlayoutDelay"; record=mean,vector, histogram);
        @signal[voIPPlayoutLoss];
        @statistic[voIPPlayoutLoss](title="VoIP Playout Loss"; unit="ratio"; source="voIPPlayoutLoss"; record=mean,vector, histogram);
        @signal[voIPJitter];
        @statistic[voIPJitter](title="VoIP Jitter"; unit="s"; source="voIPJitter"; record=mean,vector, histogram);
        @signal[voIPMos];
        @statistic[voIPMos](title="VoIP MOS Signal"; unit="MOS"; source="voIPMos"; record=mean,vector, histogram);
        @signal[voIPTaildropLoss];
        @statistic[voIPTaildropLoss](title="VoIP Tail Drop Loss"; unit="ratio"; source="voIPTaildropLoss"; record=mean,vector, histogram);
        @signal[voIPReceivedThroughput];
        @statistic[voIPReceivedThroughput](title="Throughput received at the application level"; unit="Bps"; source="voIPReceivedThroughput"; record=mean, vector, histogram);
        @display("i=block/source");
        
        
        //@signal[packetReceived](type=inet::Packet);
        //@statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        //@statistic[endToEndDelay](title="end-to-end delay"; source="dataAge(packetReceived)"; unit=s; record=histogram,vector; interpolationmode=none);
        //@statistic[Something](title="Karim"; source="dataAge(packetReceived)"; unit=s; record=histogram,vector; interpolationmode=none);
        // LTH added this end
        
    gates:
        output socketOut;
        input socketIn;
}

