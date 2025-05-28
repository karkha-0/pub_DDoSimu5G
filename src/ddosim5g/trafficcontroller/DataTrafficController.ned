package ddosim5g.trafficcontroller;

simple DataTrafficController {
    parameters:
        @class(DataTrafficController);  // Ensures the NED file references the correct C++ class
        string infectionFilePath;
        
        bool enableTrafficMod = default(false);
        int trafficMod_pktSize = default(512);
        double trafficMod_dataRate = default(1);
        
        @signal[infectionEventTime]; 
        @statistic[infectionEventTime](title="Imported infection data "; unit=""; source="infectionEventTime"; record=vector);
}