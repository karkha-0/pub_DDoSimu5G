package ddosim5g.apps;

import inet.applications.contract.IApp;

simple DynamicTrafficReceiver like IApp {
    
    gates:
        output socketOut;
        input socketIn;
}