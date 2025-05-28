//package simu5g.simulations.NR.Simple5Gsim.networks;

import inet.node.wireless.AccessPoint;
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

network NetworkMertics
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
            @display("p=585.16797,124.24799;is=n;i=device/server");
        }
        router: Router {
            @display("p=921.83997,120.24;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=1258.512,124.24799");
        }
        iUpf: Upf {
            @display("p=1046.088,629.256");
        }
        gnb: gNodeB {
            @display("p=1046.088,1030.056;is=vl");
        }
        ue[numUe]: NRUe {
            @display("p=628.09375,411.68124");
        }
        //accessPoint: AccessPoint {
        //    @display("p=1623.24,1286.568");
        //}
    connections:
        //# Data Network connections
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;

        //# 5G Core Network connections
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb.ppp;
}
