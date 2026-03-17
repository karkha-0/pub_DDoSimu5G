//
// TC-002 Network Configuration: Dynamic Traffic with Multiple Backend Servers
// Tests DynamicTrafficApp with separate servers for UDP, TCP, HTTP, and DNS traffic
// Based on Base_Config_5RAN with multiple application servers
//

package scenarios.ned;

import inet.networklayer.internetcloud.MatrixCloudDelayer;
import inet.node.internetcloud.InternetCloud;
import simu5g.nodes.NR.gNodeB;
import simu5g.nodes.NR.NRUe;
import simu5g.common.binder.Binder;
import simu5g.world.radio.LteChannelControl;
import simu5g.common.carrierAggregation.CarrierAggregation;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.nodes.Upf;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import ned.DatarateChannel;
import ddosimu5g.trafficcontroller.DataTrafficController;

network TC002_Network_Dynamic
{
    parameters:
        int numDynamicUe = default(10);  // UEs with DynamicTrafficApp
        @display("i=block/network2;bgb=2200,2000");        

    types:
        channel CustomChannel extends DatarateChannel
        {
            datarate = 1Gbps;
            delay = 0.1us;
        }

    submodules:
        // Control components
        channelControl: LteChannelControl {
            @display("p=1931.856,1739.4719;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=909.816,1747.4879;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=220.43999,1779.552");
        }
        binder: Binder {
            @display("p=496.99197,1779.552;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=1410.8159,1747.4879;is=s");
        }
        
        // Traffic Controller for DDoS attack orchestration
        dataTrafficController: DataTrafficController {
            @display("p=1650.0,1747.4879;is=s");
        }

        // Network infrastructure
        internetCloud: InternetCloud {
            @display("p=1498.992,529.05597");
        }
        appRouter: Router {
            @display("p=941.87994,396.792;i=abstract/router");
        }

        // Application servers - different server for each traffic type
        udpServer: StandardHost {
            @display("p=308.616,392.784;is=n;i=device/server");
        }
        tcpServer: StandardHost {
            @display("p=700,200;is=n;i=device/server");
        }
        httpServer: StandardHost {
            @display("p=997.99194,124.24799;is=n;i=device/server");
        }
        dnsServer: StandardHost {
            @display("p=1298.5919,124.24799;is=n;i=device/server");
        }
        generalServer: StandardHost {
            @display("p=1691.376,244.48799;is=n;i=device/server");
        }

        // 5G core network
        upf: Upf {
            @display("p=1895.7839,561.12");
        }
        iUpf: Upf {
            @display("p=1867.7279,1022.04");
        }

        // Radio Access Network - 5 gNodeBs for coverage
        gnb1: gNodeB {
            @display("p=500,1300;is=vl");
        }
        gnb2: gNodeB {
            @display("p=500,900;is=vl");
        }
        gnb3: gNodeB {
            @display("p=1000,900;is=vl");
        }
        gnb4: gNodeB {
            @display("p=1500,900;is=vl");
        }
        gnb5: gNodeB {
            @display("p=1500,1300;is=vl");
        }

        // User Equipment with DynamicTrafficApp
        dynamicUe[numDynamicUe]: NRUe {
            @display("p=997.99194,1418.8319");
        }

    connections allowunconnected:
        // Connect application servers to router
        udpServer.pppg++ <--> Eth10G <--> appRouter.pppg++;
        tcpServer.pppg++ <--> Eth10G <--> appRouter.pppg++;
        httpServer.pppg++ <--> Eth10G <--> appRouter.pppg++;
        dnsServer.pppg++ <--> Eth10G <--> appRouter.pppg++;
        generalServer.pppg++ <--> Eth10G <--> appRouter.pppg++;

        // Connect router to internet cloud
        appRouter.pppg++ <--> Eth10G <--> internetCloud.pppg++;

        // Connect internet cloud to UPF
        internetCloud.pppg++ <--> Eth10G <--> upf.filterGate;

        // Connect UPF to intermediate UPF
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;

        // Connect gNodeBs to intermediate UPF
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb2.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb3.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb4.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb5.ppp;

        // X2 connections between gNodeBs (for handover)
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb4.x2++ <--> Eth10G <--> gnb5.x2++;
        gnb5.x2++ <--> Eth10G <--> gnb1.x2++;
}
