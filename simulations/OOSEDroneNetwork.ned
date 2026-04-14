package oosedrone.simulations;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.node.inet.WirelessHost;
import inet.visualizer.common.IntegratedVisualizer;

network OOSEDroneNetwork
{
    parameters:
        int numReceivers = default(5); //default num of R
        @display("bgb=1813,1049;bgi=background/my_city,s"); //GUI background size

    submodules:
        visualizer: IntegratedVisualizer {
            @display("p=50,50");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,150");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,250");
        }

        groundStation: WirelessHost {
            @display("p=200,800;i=device/receiverdish_l");
        }

        senderDrone: WirelessHost {
            @display("p=500,500;i=device/drone_l");
        }

        receiver[numReceivers]: WirelessHost {
            @display("i=device/drone_l"); //my_drone
        }
}
