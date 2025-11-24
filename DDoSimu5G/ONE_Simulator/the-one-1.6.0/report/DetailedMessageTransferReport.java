/* 
 * Copyright 2024, Lund Univeristy, EIT, Security and Networks group
 * Released under GPLv3. See LICENSE.txt for details. 
 */
package report;

import core.Connection;
import core.ConnectionListener;
import core.DTNHost;
import core.Message;
import core.MessageListener;
import core.SimClock;

import java.util.HashMap;
import java.util.Map;

/**
 * Report for tracking detailed message transfers, including sender, receiver,
 * and timestamp of successful message delivery.
 */
public class DetailedMessageTransferReport extends Report implements MessageListener, ConnectionListener {
    public static String HEADER = "# time  sender  receiver  messageID  delivered";

    // Store connection details for each pair of nodes
    private Map<String, ConnectionInfo> connectionDetails = new HashMap<>();

    /**
     * Constructor.
     */
    public DetailedMessageTransferReport() {
        init();
    }

    @Override
    public void init() {
        super.init();
        write(HEADER);  // Write the header for the report
    }

    // Class to store connection info between two nodes
    private class ConnectionInfo {
        double connectTime;
        double disconnectTime = -1;  // Default to -1 until disconnected
        double sendTime = -1;
        boolean successfulTransfer = false;  // Tracks if a successful transfer occurred
    }

    // Helper method to create a key for identifying a connection between two nodes
    private String createConnectionKey(DTNHost host1, DTNHost host2) {
        return Math.min(host1.getAddress(), host2.getAddress()) + "-" + Math.max(host1.getAddress(), host2.getAddress());
    }

    @Override
    public void hostsConnected(DTNHost host1, DTNHost host2) {
        //System.out.println("-->Connection started between " + host1.getAddress() + "and" + host2.getAddress());
        String connectionKey = createConnectionKey(host1, host2);
        
        if (!connectionDetails.containsKey(connectionKey)) {
            ConnectionInfo info = new ConnectionInfo();
            info.connectTime = getSimTime();  // Store the connection time
            info.successfulTransfer = false;  // Initialize successful transfer flag
            connectionDetails.put(connectionKey, info);
        }
    }

    @Override
    public void hostsDisconnected(DTNHost host1, DTNHost host2) {
        //System.out.println("-->Connection ended between ");
        String connectionKey = createConnectionKey(host1, host2);
        ConnectionInfo info = connectionDetails.get(connectionKey);

        if (info != null) {
            info.disconnectTime = getSimTime();  // Store disconnection time
            //System.out.println("--> Connection ended between " + host1.getAddress() + " and " + host2.getAddress() + " at time " + info.disconnectTime);


            /*
            String log = String.format("Connection between %d and %d:\n" +
                                        "Connection Start Time: %.2f\n" +
                                        "Package Send Time: %.2f\n" +
                                        "Disconnection Time: %.2f\n" + 
                                        "Contact Time: %.2f\n",
                                        host1.getAddress(), host2.getAddress(),
                                        info.connectTime, info.sendTime, info.disconnectTime,
                                        (info.disconnectTime - info.connectTime));
            write(log);  // Log connection details
            */

            // Log the details only if there was a successful message transfer
            if (info.successfulTransfer) {
                //System.out.println("DEBUG: Connection started between " + host1.getAddress() + " and " + host2.getAddress() + " at time " + info.connectTime);
                //System.out.println("DEBUG: Connection ended between " + host1.getAddress() + " and " + host2.getAddress() + " at time " + info.disconnectTime);
                
                
                String log = String.format("Connection between %d and %d:\n" +
                                        "Connection Start Time: %.2f\n" +
                                        "Package Send Time: %.2f\n" +
                                        "Disconnection Time: %.2f\n" + 
                                        "Contact Time: %.2f\n",
                                        host1.getAddress(), host2.getAddress(),
                                        info.connectTime, info.sendTime, info.disconnectTime,
                                        (info.disconnectTime - info.connectTime));
                write(log);  // Log connection details
                info.successfulTransfer = false;
                
            }
        }
    }

    @Override
    public void messageTransferred(Message m, DTNHost from, DTNHost to, boolean success) {
        // Only track successful transfers
        if (success && !isWarmup() && !isWarmupID(m.getId())) {
            String timestamp = format(getSimTime());
            String log = timestamp + " " + from.getAddress() + " " + to.getAddress() + " " + m.getId() + " SUCCESS";
            write(log);  // Log successful transfer
              
            String connectionKey = createConnectionKey(from, to);
            ConnectionInfo info = connectionDetails.get(connectionKey);
            info.sendTime = getSimTime();
            //System.out.println("--> Message transfer ended between " + from.getAddress() + " and " + to.getAddress() + " at time " + timestamp);

            // Only log if connection information is available
            if (info != null) {
                //System.out.println("-->Logging connection details for successful message transfer");
                info.successfulTransfer = true;  // Mark that a successful transfer occurred
                
            } 
        }
    }

    @Override
    public void newMessage(Message m) {
        // Handle new message event, if needed in the future (not relevant for now)
    }

    @Override
    public void messageDeleted(Message m, DTNHost where, boolean dropped) {
        // Log dropped or deleted messages if needed
        System.out.println("DEBUG: Message: " + m.getId() + "detleted to  " + where.getAddress()  + " at time " + format(getSimTime()));
    }

    @Override
    public void messageTransferAborted(Message m, DTNHost from, DTNHost to) {
        // Optionally log aborted transfers (not implemented for simplicity)
        System.out.println("DEBUG: Message: " + m.getId() + "transfer Aborted between " + from.getAddress() + " and " + to.getAddress() + " at time " + format(getSimTime()));
    }

    @Override
    public void messageTransferStarted(Message m, DTNHost from, DTNHost to) {
        //System.out.println("--> Message transfer started between " + from.getAddress() + " and " + to.getAddress() + " at time " + format(getSimTime()));
        // Optionally log when message transfer starts (not implemented for simplicity)
    }

    @Override
    public void done() {
        super.done();
   }
}
