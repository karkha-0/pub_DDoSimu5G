package routing.util;

import core.Coord;

public class ContactInfo {
    public Integer fromNode;
    public Integer toNode;

    public double connectTime = -1;
    public double disconnectTime = -1;  // Default to -1 until disconnected

    public boolean malwareTransmissionState = false;
    public boolean toNodeisInfected = false;
    public boolean fromNodeisInfected = false;

    public Coord toPosition;
    public Coord fromPosition;

    @Override
    public String toString() {
        return "ContactInfo{" +
            "fromNode=" + fromNode  +
            ", toNode=" + toNode  +
            ", connectTime=" + connectTime +
            ", disconnectTime=" + disconnectTime +
            ", malwareTransmissionState=" + malwareTransmissionState +
            ", toNodeisInfected=" + toNodeisInfected +
            '}';
    }
}