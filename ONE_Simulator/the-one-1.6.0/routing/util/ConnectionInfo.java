package routing.util;

import core.DTNHost;

// Class to store connection info between two nodes
public class ConnectionInfo {
    public double initSendTime = -1;
    public double connectTime = -1;
    public double disconnectTime = -1;  // Default to -1 until disconnected
    public double handshakeTime = -1;
    public double prepTime = -1;
    public double transferStartTime = -1;
    public double transferEndTime = -1;
    public double messageCreationTime = -1;
    public double messageSize = -1;
    public double lastSendAttempt = -1;

    public boolean successfulTransfer = false;  // Tracks if a successful transfer occurred
    public boolean isInfected = false;

    public String messageId;
    public DTNHost fromNode;
    public DTNHost toNode;
}