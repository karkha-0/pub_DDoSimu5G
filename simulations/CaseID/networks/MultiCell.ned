//package networks;

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

network MultiCell
{
    parameters:
        int numUe = default(1);
        //int numBgCells = default(0);
        @display("i=block/network2;bgb=2000,2000;bgi=background/lthmodified");
    submodules:
        channelControl: LteChannelControl {
            @display("p=50,25;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=50,75;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,125");
        }
        binder: Binder {
            @display("p=50,175;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=50.993748,258.7;is=s");
        }
        server: StandardHost {
            @display("p=212,118;is=n;i=device/server");
        }
        router: Router {
            @display("p=363.175,115.66875;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=527.35,116.9125");
        }
        iUpf: Upf {
            @display("p=725,118");
        }
        gnb1: gNodeB {
            @display("p=250,300;is=vl");
        }
        gnb2: gNodeB {
            @display("p=750,300.566;is=vl");
        }
        gnb3: gNodeB {
            @display("p=1250,301.132;is=vl");
        }
        gnb4: gNodeB {
            @display("p=1250,302.132;is=vl");
        }
        gnb5: gNodeB {
            @display("p=1250,304.132;is=vl");
        }
        gnb6: gNodeB {
            @display("p=1250,306.132;is=vl");
        }
        gnb7: gNodeB {
            @display("p=1250,307.132;is=vl");
        }
        gnb8: gNodeB {
            @display("p=1250,308.132;is=vl");
        }
        gnb9: gNodeB {
            @display("p=1250,310.132;is=vl");
        }
//        gnb10: gNodeB {
//            @display("p=1250,311.132;is=vl");
//        }
        ue[numUe]: NRUe {
            @display("p=628.09375,411.68124");
        }
    connections:
        //# Data Network connections
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;
        
        //# 5G Core Network connections
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb2.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb3.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb4.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb5.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb6.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb7.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb8.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb9.ppp;
//        iUpf.pppg++ <--> Eth10G <--> gnb10.ppp;        

        //# X2 connections
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb5.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb6.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb7.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb8.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb1.x2++ <--> Eth10G <--> gnb10.x2++;
        
        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb5.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb6.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb7.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb8.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb2.x2++ <--> Eth10G <--> gnb10.x2++;
        
        gnb3.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb5.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb6.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb7.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb8.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb3.x2++ <--> Eth10G <--> gnb10.x2++;
        
        gnb4.x2++ <--> Eth10G <--> gnb5.x2++;
        gnb4.x2++ <--> Eth10G <--> gnb6.x2++;
        gnb4.x2++ <--> Eth10G <--> gnb7.x2++;
        gnb4.x2++ <--> Eth10G <--> gnb8.x2++;
        gnb4.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb4.x2++ <--> Eth10G <--> gnb10.x2++;
        
        gnb5.x2++ <--> Eth10G <--> gnb6.x2++;
        gnb5.x2++ <--> Eth10G <--> gnb7.x2++;
        gnb5.x2++ <--> Eth10G <--> gnb8.x2++;
        gnb5.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb5.x2++ <--> Eth10G <--> gnb10.x2++;
        
        gnb6.x2++ <--> Eth10G <--> gnb7.x2++;
        gnb6.x2++ <--> Eth10G <--> gnb8.x2++;
        gnb6.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb6.x2++ <--> Eth10G <--> gnb10.x2++;
        
        gnb7.x2++ <--> Eth10G <--> gnb8.x2++;
        gnb7.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb7.x2++ <--> Eth10G <--> gnb10.x2++;
        
        gnb8.x2++ <--> Eth10G <--> gnb9.x2++;
//        gnb8.x2++ <--> Eth10G <--> gnb10.x2++;
        
//        gnb9.x2++ <--> Eth10G <--> gnb10.x2++;
}
