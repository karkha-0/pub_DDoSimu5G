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
import ddosim5g.simcommands.PostSimulationCommand;

network Base_Config_1RAN
{
    parameters:
        int numUe = default(1);
        int numBgCells = default(0);
        @display("i=block/network2;bgb=2000,2000;bgi=background/lthmodified");

    types:
        channel C extends DatarateChannel
        {
            datarate = 100Mbps;
            delay = 0.1us;
        }
    submodules:
        postSimCommand: PostSimulationCommand {//{
            //parameters:
                //vecFilePath = default("defaultpath.vec"); // Optional: Provide a default value or leave it unset
        //}

            @display("p=52.104,436.87198;is=vs");
        }
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
        internetCloud: InternetCloud {
            @display("p=1010.016,172.344");
        }
        localRouter: Router {
            @display("p=701.39996,172.344");
        }
        remoteServer: StandardHost {
            @display("p=416.832,172.344;is=n;i=device/server");
        }
        upf: Upf {
            @display("p=1430.856,172.344");
        }
        iUpf: Upf {
            @display("p=1823.6399,172.344");
        }
        gnb1: gNodeB {
            @display("p=2078.152,172.87195;is=l");

        }
        ue[numUe]: NRUe {
            @display("p=628.09375,411.68124");
        }
    connections:
        //# Data network
        //UPF to internetCloud
        internetCloud.pppg++ <--> C <--> upf.filterGate;

        // Local Router to InternetCloud
        localRouter.ethg++ <--> C <--> internetCloud.pppg++;

        // Local Router to Remote Server
        localRouter.pppg++ <--> Eth10G <--> remoteServer.pppg++;

        //# 5G Core Network connections
        // iUPF to UPF
        iUpf.ethg++ <--> Eth10G <--> upf.ethg++;
        //iUPF tp gnb
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
}
