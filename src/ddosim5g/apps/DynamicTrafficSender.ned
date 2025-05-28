package ddosim5g.apps;

import inet.applications.contract.IApp;

simple DynamicTrafficSender like IApp {
    
    gates:
        output socketOut;
        input socketIn;
    
}