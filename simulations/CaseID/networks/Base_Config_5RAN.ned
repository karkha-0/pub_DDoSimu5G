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
import simu5g.apps.cbr.CbrSender;
import ddosim5g.trafficcontroller.DataTrafficController;

//import inet.visualizer.common.IntegratedVisualizer;

simple DDoSSender extends CbrSender
{
    parameters:
        @mutable;
}

network Base_Config_5RAN
{
    parameters:
        int numCbrUe = default(1);
        int numVidUe = default(0);
        int numBgCells = default(0);
        @display("i=block/network2;bgb=2000,2000;bgi=background/lthmodified");

    types:
        channel CustomChannel extends DatarateChannel
        {
            datarate = 250kbps;
            delay = 0.1us;
        }
    submodules:
        trafficController: DataTrafficController {
            parameters:
                infectionFilePath = default("");
                //infectionFilePath = "../../../ONE_Simulator/the-one-1.6.0/reports/D2D_MalwarePropagation/10MinsMalwarePropagationReport.json"; // To run from Qtenv
                //infectionFilePath = "../../../../ONE_Simulator/the-one-1.6.0/reports/D2D_MalwarePropagation/10MinsMalwarePropagationReport.json"; // To run from script
                //infectionFilePath = "ONE_Simulator/the-one-1.6.0/reports/D2D_MalwarePropagation/10MinsMalwarePropagationReport.json"; // To run from cmdenv
     
                @display("p=236.472,1827.648;is=s");
        }
        channelControl: LteChannelControl {
            @display("p=1162.32,1583.1599;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=701.39996,1583.1599;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=236.472,1583.1599");
        }
        binder: Binder {
            @display("p=701.39996,1767.528;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=1010.016,1827.648;is=s");
        }
        internetCloud: InternetCloud {
            // add this line to ini file
            //**.internet.ipv4Delayer.typename = "MatrixCloudDelayer" ->  I am not sure about this one, maybe look again at internetcloud on doc and in inet folder 
            @display("p=1010.016,172.344");
        }
        localRouter: Router {
            @display("p=621.24,172.344");
        }
        remoteServer: StandardHost {
            @display("p=288.576,172.344;is=n;i=device/server");
        }
        upf: Upf {
            @display("p=1430.856,172.344");
        }
        iUpf: Upf {
            @display("p=1823.6399,172.344");
        }
        gnb1: gNodeB {
            @display("p=541.07996,1346.688;is=l");
        }
        gnb2: gNodeB {
            @display("p=557.112,589.17596;is=l");
        }
        gnb3: gNodeB {
            @display("p=1482.96,589.17596;is=l");
        }
        gnb4: gNodeB {
            @display("p=1042.08,977.95197;is=l");
        }
        gnb5: gNodeB {
            @display("p=1482.96,1346.688;is=l");
        }
        //Creating two sets of UEs, one suspetical for attack, and 
        // one not but with higher datarate demands (VideoStreamingApp in ini provides more
        //statistical metrics)
        cbrUe[numCbrUe]: NRUe {
            @display("p=981.95996,661.32");
        }
        VidUe[numVidUe]: NRUe {
            @display("p=701.39996,893.784");
        }
        //visualizer: IntegratedVisualizer {
        //    parameters:
        //        @display("p=1523.0399,1827.648");
        //}
        videoRouter: Router {
            @display("p=236.472,432.86398");
        }
        videoServer: StandardHost {
            @display("p=236.472,741.48;is=n;i=device/server");
        }
    connections:
        //# Data network
        // Local Router to Remote Server
        remoteServer.pppg++ <--> Eth10G <--> localRouter.pppg++;
        // Local Router to InternetCloud
        localRouter.pppg++ <--> Eth10G <--> internetCloud.pppg++;

        // Video Router to Video Server
        videoServer.pppg++ <--> Eth10G <--> videoRouter.pppg++;
        // Video Router to InternetCloud
        videoRouter.pppg++ <--> Eth10G <--> internetCloud.pppg++;

        //UPF to internetCloud
        internetCloud.pppg++ <--> Eth10G <--> upf.filterGate;

        //# 5G Core Network connections
        // UPF to iUPF
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;  
        //iUpf.ethg++ <--> Eth10G <--> upf.ethg++;
        //iUpf.pppg++ <--> Eth10G <--> upf.pppg++;
        
        //iUPF tp gnb
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb2.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb3.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb4.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb5.ppp;
        
        //# X2 connections. Note all gnbs need to be conncted to all if we going to place them randomly around the map
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb5.x2++;

        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb5.x2++;

        gnb3.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb5.x2++;

        gnb4.x2++ <--> Eth10G <--> gnb5.x2++;
        
}
