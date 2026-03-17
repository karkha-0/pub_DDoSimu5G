package ddosimu5g.trafficcontroller;

simple DataTrafficController {
    parameters:
        @class(DataTrafficController);  // Ensures the NED file references the correct C++ class
        string infectionFilePath = default("");  // Optional: only needed when enableAttackApps or enableTrafficMod is true
        
        bool enableTrafficMod = default(false);
        int trafficMod_pktSize = default(512);
        double trafficMod_dataRate = default(1);

        // NEW Make trafficController extends beyond one traffic behavior change
        //string attackProfileFile = default("");      // Path to JSON attack profile
        bool enableAttackApps = default(false);        // Enable attack app instantiation
        string ueModuleName = default("dynamicUe");    // Only used when enableAttackApps=true
        int attackSlotStart = default(10);             // Start slot of the attack apps on UE
        int attackSlotEnd = default(19);               // End slot of the attack apps on UE
 
        @signal[infectionEventTime]; 
        @statistic[infectionEventTime](title="Imported infection data "; unit=""; source="infectionEventTime"; record=vector);
}