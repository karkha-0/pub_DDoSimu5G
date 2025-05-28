/* 
 * Copyright 2024, Lund Univeristy, EIT, Security and Networks group
 * Released under GPLv3. See LICENSE.txt for details. 
 */

package routing;

import core.Connection;
import core.Coord;
import core.DTNHost;
import core.Message;
import core.Settings;
import core.SimClock;
import routing.util.ConnectionInfo;
import routing.util.ContactInfo;
import core.MessageListener;

import java.io.FileWriter;
import java.io.IOException;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

import org.json.JSONArray;
import org.json.JSONObject;

import com.fasterxml.jackson.core.JsonFactory;
import com.fasterxml.jackson.core.JsonGenerator;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

/**
 * D2DRouter for malware propagation via Device-to-Device (D2D) wireless communication .
 */
public class D2DRouter extends ActiveRouter {

    private int malwareSizeKB;           // Size of the malware in KBtransmitSpeedKbps
    private int installationTime;        // Time taken to install malware
    private int messageTTL;              // Message TTL

    private double malwareRecvTime = -1; // Timestamp when malware was received
    private double appHandshakeDelay;    // Delay for handshake between nodes
    private double malwarePrepDelay;     // Delay for preparing malware for trnsfer
    private double lastSendAttempt = -1; // Timestamp of the last infection attempt
    private double sendInterval;         // Minimum time between consecutive infections
    
    private boolean isInfected = false;  // Indicates whether the current node is infected

    private DTNHost srcMalwareNode;      // Stores the node that infected this node

    private static Set<Integer> infectedNodesSet = new HashSet<>();                             // Store the list of infected nodes
    protected List<Connection> currentConnections = new ArrayList<>();                          // List of active connections

    //protected static Map<String, Double> connectionStartTimes = new ConcurrentHashMap<>();      // Tracks connection start times
    
    protected Map<String, Boolean> isTransmissionInProgress = new HashMap<>();                  // Flags for transmission progress
    protected Map<String, Message> messagesInProgress = new HashMap<>();                        // Stores ongoing message transfers
    protected static Map<String, ConnectionInfo> connectionDetails = new ConcurrentHashMap<>(); // hashmap to access all kind of info abut a connection
    protected static Map<String, ContactInfo> contactDetails = new ConcurrentHashMap<>(); // hashmap to access all kind of info abut a connection

    // JSON arrays for logging 
    private static JSONArray infectionDataArray = new JSONArray(); 
    private static JSONArray malwarePropagationDataArray = new JSONArray(); 
    private static JSONArray nodeContactDataArray = new JSONArray();

    // To fix memory issue
    private static JsonGenerator infectionLogWriter = null;
    private static JsonGenerator malwareLogWriter = null;
    private static JsonGenerator contactLogWriter = null;
    private String reportDir;
    
    // Class Constructor
    public D2DRouter(Settings s) {
        super(s);
        initializeSettings(s); // Load settings from configuration
        
        System.out.println(this.reportDir);
        //Tryint to fix memory issue
        initLogWriters(this.reportDir); // Initialize log writers here
    }

    @Override
    public D2DRouter replicate() {
        return new D2DRouter(this);
    }
    /**
     * Copy constructor for creating a new D2DRouter instance by copying the state of an existing one.
     * This ensures that the new router has the same configuration and infection state as the original,
     * while keeping independent copies of the infected nodes, connection start times, and connections.
     *
     * @param r The original D2DRouter object to copy.
     */
    protected D2DRouter(D2DRouter r) {
        super(r);       

        this.isInfected = r.isInfected;
        this.malwareRecvTime = r.malwareRecvTime;
        this.srcMalwareNode = r.srcMalwareNode;

        this.installationTime = r.installationTime;
        this.appHandshakeDelay = r.appHandshakeDelay;
        this.malwarePrepDelay = r.malwarePrepDelay;
        this.malwareSizeKB = r.malwareSizeKB;
        this.sendInterval = r.sendInterval;
        this.messageTTL = r.messageTTL;

        this.currentConnections = new ArrayList<>(r.currentConnections);
    }

    // Class Initialization
    @Override
    public void init(DTNHost host, List<MessageListener> mListeners) {
        super.init(host, mListeners);       

        // If this node is in the infected nodes list, mark it as infected
        if (infectedNodesSet.contains(getHost().getAddress())) {
            isInfected = true;
        }
    }
    
    /**
     * Load infection parameters from configuration file
     * Mark initial range of infected nodes
     * 
     * @param s Grap configuration settings 
     */
    private void initializeSettings(Settings s) {
        this.installationTime = s.getInt("installationTime", 10);
        this.appHandshakeDelay = s.getDouble("AppHandshakeTime", 1);
        this.malwarePrepDelay = s.getDouble("MalwarePreparationTime", 2);
        this.malwareSizeKB = s.getInt("malwareSize", 500);
        this.sendInterval = s.getDouble("transmitInterval", 1); // Taking the first value of the interval as a default
        this.messageTTL = s.getInt("messageTTL", 100);

        // To fix memory issue
        Settings reportSettings = new Settings("Report");
        this.reportDir = reportSettings.getSetting("reportDir");
        

        String infectedNodesRange = s.getSetting("infectedNodesRange", "0,10");
        if (!infectedNodesRange.isEmpty()) {
            markInitialInfectedNodes(infectedNodesRange);
        }
    }

    /**
     * Updates the state of the node, manages infection status, and handles sequential message propagation.
     * Check if installation time has passed to activate infection.
     * Attempt infections periodically based on sendInterval
     */
    @Override
    public void update() {
        super.update();

        double currentTime = SimClock.getTime();
        
        // Check for installation completion if the node is not infected but have already recived an malware
        if (!isInfected && malwareRecvTime > 0 && currentTime - malwareRecvTime >= installationTime) {
            isInfected = true;
            //System.out.println("DEBUG: Node " + getHost().getAddress() + " is now actively infected at " + currentTime);

            // Log infection event
            JSONObject infectionData = new JSONObject(new LinkedHashMap<>());
            infectionData.put("node_id", getHost().getAddress());
            infectionData.put("infected_by", srcMalwareNode.getAddress());
            infectionData.put("malware_active_time", SimClock.getTime());
            infectionData.put("node_postion_at_active_infected", getHost().getLocation().toString());
            infectionData.put("infection_status", "Infected Active");
            //infectionDataArray.put(infectionData);

            // To fix memory issue
            try {
                writeLogToDisk(infectionLogWriter, infectionData.toMap());
            } catch (Exception e) {
                e.printStackTrace();
            }
            
            infectedNodesSet.add(getHost().getAddress());
        }
         
        
        // If node is not Active infected node, exit early
        if (!isInfected) return;

        // Only attempt an infection every sendInterval. If node is send interval hasn't passed, exit early
        if (currentTime - lastSendAttempt < sendInterval) return;
        
        //update last send attempt time for the node
        lastSendAttempt = currentTime;       

        /**
         * D2D Discovery 
         * This is considered as Direct discovery
         * The direct discovery procedure can work for both in-coverage and out-of coverage scenarios.
         * Retrieve the list of currently active connections
         * 
         */ 
        List<Connection> activeConnections = getConnections();

        if (activeConnections.isEmpty() || activeConnections == null) return;

        /*
        // Randomly select a connection to infect from the active connections
        Random random = new Random();
        Connection con = activeConnections.get(random.nextInt(activeConnections.size()));
        DTNHost targetNode = con.getOtherNode(getHost());
         */

        for (Connection con : activeConnections) {
            DTNHost targetNode = con.getOtherNode(getHost());
            
            // If the node in the active connections is not infected, we dont procced
            if (!infectedNodesSet.contains(getHost().getAddress())) continue;
               
            // If the target node is already infected we dont procced
            if (infectedNodesSet.contains(targetNode.getAddress())) continue;                        
            
            String connectionKey = createConnectionKey(getHost(),targetNode);
            ConnectionInfo connInfo = getOrCreateConnectionInfo(connectionKey);

            // Possibly it is needed to ensure that nodes are within range with isWithinRange form network interface

            if (!messagesInProgress.containsKey(connectionKey)) {
                Message malwareMessage = createMalwareMessage(targetNode);
                malwareMessage.setTtl(messageTTL);  // Set message TTL as specified in the group
                getHost().getRouter().createNewMessage(malwareMessage); // Add message to node message queue
                messagesInProgress.put(connectionKey, malwareMessage);
            }

            connInfo.messageId = messagesInProgress.get(connectionKey).getId();
            connInfo.fromNode = getHost();
            connInfo.toNode = targetNode;
            connInfo.messageSize = messagesInProgress.get(connectionKey).getSize();
            connInfo.messageCreationTime = messagesInProgress.get(connectionKey).getCreationTime();
            connectionDetails.put(connectionKey, connInfo);

            // Skip if already transferring a message or no active connections, no messages in queue, or can't transfer
            if (isTransferring() || !canStartTransfer()) continue;
            
            // Start message tranfer with preset delays
            initiateTransferWithDelays(messagesInProgress.get(connectionKey), con, connectionKey);
        }
    }

    /**
     * Create a unique malware message for the target node
     *
     * @param targetNode   Target node to recieve message
     * @return A unique message ID for the target node
     */
    private Message createMalwareMessage(DTNHost targetNode) {
        String messageId = "Malware-" + getHost().getAddress() + "-" + targetNode.getAddress();
        //System.out.println("DEBUG: Message ID " + messageId + " is created." + ", at timestamp: " + SimClock.getTime());
        return new Message(getHost(), targetNode, messageId, malwareSizeKB);
    }

    /**
     * Handles the infection logic with delays for handshake, preparation, and transmission.
     * Complete the transfer once all delays are satisfied
     * 
     * @param m   The malware message
     * @param con The active connection
     * @param connectionKey The connection key
     */
    private void initiateTransferWithDelays(Message m, Connection con, String connectionKey) {
        
        double currentSimTime = SimClock.getTime();
        ConnectionInfo connInfo = getOrCreateConnectionInfo(connectionKey);
    
        // Check if the connectionKey is already in the map
        //if (!connectionStartTimes.containsKey(connectionKey)) {
        if (connInfo.initSendTime < 0) {
            //connectionStartTimes.put(connectionKey, currentSimTime); // Track the start time for new connections
            connInfo.initSendTime = currentSimTime; // Track the start time for new connections
            connectionDetails.put(connectionKey, connInfo);
        }
             
        // Flags to check all phases are marked as completed
        boolean handshakeComplete = isTransmissionInProgress.getOrDefault(connectionKey + "_handshake", false);
        boolean prepComplete = isTransmissionInProgress.getOrDefault(connectionKey + "_prep", false);
                    
        // Handshake phase while handling handshake delay
        if ((currentSimTime - connInfo.initSendTime >= appHandshakeDelay) && (!handshakeComplete))
        {
            connInfo.handshakeTime = SimClock.getTime();
            connectionDetails.put(connectionKey, connInfo);   
            isTransmissionInProgress.put(connectionKey + "_handshake", true);            
            //System.out.println("DEBUG: Handshake Phase");
            return; // Handshake phase started, so wait until next update cycle to continue with next phase
        }
        
        // Preparation phase, while handling message preparation delay
        if (((currentSimTime - connInfo.initSendTime) >= (appHandshakeDelay + malwarePrepDelay)) &&
            (!prepComplete))
        {
            isTransmissionInProgress.put(connectionKey + "_prep", true);
            connInfo.prepTime = SimClock.getTime();
            connectionDetails.put(connectionKey, connInfo);
            //System.out.println("DEBUG: Malware Package Preparation Phase");
            return; // Malware preparation phase started, so wait until next update cycle to continue with next phase
        }

        /** 
         * Complete the transfer. Once all delays are complete, start the transfer
         * Check all flags before starting the transfer
         * Handshake here is like synchronization signals (PSSS/SSSS) and PSBCH from other UEs
         */
        if (handshakeComplete && prepComplete)
        {
            //System.out.println("DEBUG: All phases complete, starting transfer for message " + m.getId());
            connInfo.transferStartTime = SimClock.getTime();
            connectionDetails.put(connectionKey, connInfo);
            super.startTransfer(m, con);  // Call the base class transfer method
        
            // Mark the receiving node as infected, node is not active until the installation time is complete and isInfected flag is true (in update method)
            infectedNodesSet.add(con.getOtherNode(getHost()).getAddress());

            //Track message tranmissions success for node contacts
            if (contactDetails.containsKey(connectionKey)) {
                ContactInfo contactInfo = getOrCreateContactInfo(connectionKey);
                contactInfo.malwareTransmissionState = true;
                contactDetails.put(connectionKey, contactInfo);
            }

            // Reset messageInProgress with current message ID
            messagesInProgress.remove(connectionKey);
            
            // Reset transmission flags after transfer is completed
            isTransmissionInProgress.remove(connectionKey + "_handshake");
            isTransmissionInProgress.remove(connectionKey + "_prep");
            isTransmissionInProgress.remove(connectionKey + "_transfer");
            isTransmissionInProgress.remove(connectionKey);
        }
    }

    /**
     * Mark nodes selected in the simulation run configuration as infected
     * 
     * @param range Range of infected nodes from the simulation run config file 
     */
    private void markInitialInfectedNodes(String range) {
        String[] rangeParts = range.split(",");
        int startNode = Integer.parseInt(rangeParts[0].trim());
        int endNode = Integer.parseInt(rangeParts[1].trim());

        for (int i = startNode; i <= endNode; i++) {
            infectedNodesSet.add(i);
        }
    }

    /**
     * Helper method to create consitent connectionKey for identifying a connection between two nodes
     * The connectionKey start with the smaller nodeId and then the larger
     * @param host1: Node ID of sender of type DTNHost
     * @param host2: Node ID of receiver of type DTNHost
     * @return: connectionKey of type String
     */ 
    private String createConnectionKey(DTNHost host1, DTNHost host2) {
        return Math.min(host1.getAddress(), host2.getAddress()) + "-" + Math.max(host1.getAddress(), host2.getAddress());
    }

    /**
     * Receives a message and determines whether the current node is the final recipient.
     * Logs the message details and infection status if the message is received by a non-infected node.
     * 
     * @param m The message being received
     * @param from The node from which the message was sent
     * @return The result of the message reception process
     */
    @Override
    public int receiveMessage(Message m, DTNHost from) {
        // Check if this node is the intended recipient (final destination) for the message
        if (m.getTo().equals(this.getHost())) {
    
            // Now, check if the message is delivered to non infected Nodes only
            if (!isInfected) {
                int result = super.receiveMessage(m, from);
        
                // Log the successful reception of the message
                if (result == RCV_OK){
                    malwareRecvTime = SimClock.getTime(); // Log infection start time    
                    
                    /*System.out.println("Node " + this.getHost().getAddress() +
                    " received infection from node " + from.getAddress() +
                    " at " + SimClock.getTime());*/
                }
                return result;
            }
            else {
                // Node already infected, will not recieve new malware
                return DENIED_OLD;
            }
        }
        // If this node is not the intended recipient, discard the message
        return DENIED_POLICY;
    }

    /**
     * Handles the event when a message is fully transferred and delivered to this node.
     * Logs the message transfer and initiates the infection process if the node is not yet infected.
     * 
     * @param id The ID of the message that was transferred
     * @param from The node that transferred the message
     * @return The message after being processed
     */
    @Override
    public Message messageTransferred(String id, DTNHost from) {
        Message m = super.messageTransferred(id, from); 

        // If the message is fully delivered to this host as final recipient
        if (isDeliveredMessage(m)) {
            // If the receiving node is not infected, start the infection process
            if (!isInfected) {
                srcMalwareNode = from;                
                
                String connectionKey = createConnectionKey(this.getHost(),from);
                ConnectionInfo connInfo = getOrCreateConnectionInfo(connectionKey);
                connInfo.transferEndTime = SimClock.getTime();
                connectionDetails.put(connectionKey, connInfo);

                // Log message propgation data for sender and receiver
                logMalwarePropData(from , m.getTo());
            }
        }
        return m; // Return the message after processing
    }

    @Override
    public void changedConnection(Connection con) {
        DTNHost otherNode = con.getOtherNode(getHost());
        String connectionKey = createConnectionKey(getHost(),otherNode);
        ConnectionInfo connInfo = getOrCreateConnectionInfo(connectionKey);
        
        // See if connections is up, and log connection start time for the connectionKey
        if (con.isUp()) { 
            connInfo.connectTime = SimClock.getTime();
            connectionDetails.put(connectionKey, connInfo);

            if (!contactDetails.containsKey(connectionKey)) {  
                ContactInfo contactInfo = getOrCreateContactInfo(connectionKey);
                contactInfo.fromNode = getHost().getAddress();
                contactInfo.fromPosition = getHost().getLocation();
                contactInfo.toNode = otherNode.getAddress();
                contactInfo.toPosition = otherNode.getLocation();
                contactInfo.connectTime = SimClock.getTime();
                if (infectedNodesSet.contains(getHost().getAddress())) { 
                    contactInfo.fromNodeisInfected = true;
                }
                if (infectedNodesSet.contains(otherNode.getAddress())) {
                    contactInfo.toNodeisInfected =true;
                }
                contactDetails.put(connectionKey, contactInfo);
            }
        }
        else {
            // We log all connections made, even those not resulting in infection
            if (contactDetails.containsKey(connectionKey)) {
                ContactInfo contactInfo = getOrCreateContactInfo(connectionKey);
                contactInfo.disconnectTime = SimClock.getTime();
                contactDetails.put(connectionKey, contactInfo);
                // Log nodes contact details
                logNodesConctactData(connectionKey);
            }
        }
    }

    /**
     * Retrieves the class object of connection info if existing
     * Else it create one
     * @return Class obhect of ConnectionInfo
     */
    public static ConnectionInfo getOrCreateConnectionInfo(String connectionKey) {
        ConnectionInfo connInfo = connectionDetails.getOrDefault(connectionKey, new ConnectionInfo());
        return connInfo;
    }

    /**
     * Retrieves the class object of contact info if existing
     * Else it create one
     * @return Class obhect of ContactInfo
     */
    public static ContactInfo getOrCreateContactInfo(String connectionKey) {
        ContactInfo contactInfo = contactDetails.getOrDefault(connectionKey, new ContactInfo());
        contactDetails.put(connectionKey, contactInfo);
        return contactInfo;
    }
    /**
     * Retrieves the set of infected nodes.
     * @return A set of node IDs representing infected nodes
     */
    public static Set<Integer> getInfectedNodesSet() {
        return infectedNodesSet;
    }

    /**
     * Logs node contact detailsn data
     * Logs spatial data (x, y coordinates) for both the sender and receiver during the connection up and down.
     * @param connectionKey String contacting the connection key 
     */
    public void logNodesConctactData(String connectionKey) {

        JSONObject contactData = new JSONObject(new LinkedHashMap<>());
        ContactInfo contactInfo = contactDetails.get(connectionKey);

        if (contactInfo != null) {
            // Log connections event  
            contactData.put("connection_key", connectionKey);
            contactData.put("node_id", contactInfo.fromNode);
            contactData.put("connected_to", contactInfo.toNode);
            contactData.put("connection_time", contactInfo.connectTime);
            contactData.put("disconnection_time", contactInfo.disconnectTime);
            contactData.put("malware_tranmissed", contactInfo.malwareTransmissionState);
            contactData.put("to_node_infected_at_connection", contactInfo.toNodeisInfected); 
            contactData.put("from_node_infected_at_connection", contactInfo.fromNodeisInfected); 
            contactData.put("from_node_position_at_conn_start", contactInfo.fromPosition.toString());
            contactData.put("to_node_position_at_conn_start", contactInfo.toPosition.toString());
            contactData.put("distance_between_nodes", contactInfo.fromPosition.distance(contactInfo.toPosition));  // Distance between nodes
            //nodeContactDataArray.put(contactData);

            // Trying to fix memory issue
            try {
                writeLogToDisk(contactLogWriter, contactData.toMap());
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        // Cleanup of Contact details
        contactDetails.remove(connectionKey);
    }

    /**
     * Logs Malware propgation data
     * Logs spatial data (x, y coordinates) for both the sender and receiver during the message exchange.
     * @param from The sending node
     * @param to The receiving node
     */
    public void logMalwarePropData(DTNHost from, DTNHost to) {
        Coord fromPos = from.getLocation();
        Coord toPos = to.getLocation();

        double distance = fromPos.distance(toPos);
        
        String connectionKey = createConnectionKey(from, to);
        
        JSONObject malwarePropagationData = new JSONObject(new LinkedHashMap<>());
        malwarePropagationData.put("from_node_position_at_transfer_end", fromPos.toString());
        malwarePropagationData.put("to_node_position_at_transfer_end", toPos.toString());
        malwarePropagationData.put("distance_between_nodes", distance);  // Distance between nodes
        malwarePropagationData.put("infection_status", "Infected InActive");

        ConnectionInfo connInfo = connectionDetails.get(connectionKey);
        if (connInfo != null) {
            malwarePropagationData.put("from_node", connInfo.fromNode.getAddress());
            malwarePropagationData.put("to_node", connInfo.toNode.getAddress());
            malwarePropagationData.put("connetion_key", connectionKey);
            malwarePropagationData.put("message_id", connInfo.messageId);
            malwarePropagationData.put("message_size", connInfo.messageSize);
            malwarePropagationData.put("message_creation_time", connInfo.messageCreationTime);
            malwarePropagationData.put("conn_start_time", connInfo.connectTime);
            malwarePropagationData.put("transfer_start_request", connInfo.initSendTime);
            malwarePropagationData.put("transfer_handshake_time", connInfo.handshakeTime);
            malwarePropagationData.put("transfer_prep_time", connInfo.prepTime);
            malwarePropagationData.put("transfer_start_time", connInfo.transferStartTime);
            malwarePropagationData.put("transfer_end_time", connInfo.transferEndTime);
        }       
        // Add to infection report
        //malwarePropagationDataArray.put(malwarePropagationData);

        // Trying to fix memory issue
        try {
            writeLogToDisk(malwareLogWriter, malwarePropagationData.toMap());
        } catch (Exception e) {
            e.printStackTrace();
        }

        // Cleanup of Connection details
        connectionDetails.remove(connectionKey);
    }

    /**
     * Generates a JSON report with infection data and malware propagation data.
     * 
     * @return A JSONObject containing the infection and propagation reports
     */
    public static JSONObject getInfectionReportArray() {
        JSONObject reportData = new JSONObject();
        reportData.put("infectionData", infectionDataArray);
        reportData.put("malwarePropagationData", malwarePropagationDataArray);
        reportData.put("contactDetailsData", nodeContactDataArray);
        return reportData;
    }

    // Class to store connection info between two nodes
    /*private static class ConnectionInfo {
        double initSendTime = -1;
        double connectTime = -1;
        double disconnectTime = -1;  // Default to -1 until disconnected
        double handshakeTime = -1;
        double prepTime = -1;
        double transferStartTime = -1;
        double transferEndTime = -1;
        double messageCreationTime = -1;
        double messageSize = -1;
        double lastSendAttempt = -1;

        boolean successfulTransfer = false;  // Tracks if a successful transfer occurred
        boolean isInfected = false;

        String messageId;
        DTNHost fromNode;
        DTNHost toNode;
    }*/

    /*private static class ContactInfo {
        Integer fromNode;
        Integer toNode;

        double connectTime = -1;
        double disconnectTime = -1;  // Default to -1 until disconnected

        boolean malwareTransmissionState = false;
        boolean toNodeisInfected = false;
        boolean fromNodeisInfected = false;

        Coord toPosition;
        Coord fromPosition;

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
    }*/


    // Trying to fix memory issue
    public void initLogWriters(String reportDir) {
        try {
            ObjectMapper mapper = new ObjectMapper();
            JsonFactory jsonFactory = new JsonFactory(mapper); // Attach ObjectMapper as codec
    
            infectionLogWriter = jsonFactory.createGenerator(new FileWriter(reportDir + "/infection_log.json"));
            malwareLogWriter = jsonFactory.createGenerator(new FileWriter(reportDir + "/malware_log.json"));
            contactLogWriter = jsonFactory.createGenerator(new FileWriter(reportDir + "/contact_log.json"));
    
            // Start JSON arrays
            infectionLogWriter.writeStartArray();
            malwareLogWriter.writeStartArray();
            contactLogWriter.writeStartArray();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    // To fix memory issue
    public static Map<String, JsonGenerator> getLogWriters() {
        if (infectionLogWriter != null && malwareLogWriter != null && contactLogWriter != null)
        {
            Map<String, JsonGenerator> writers = new HashMap<>();
            writers.put("infectionLogWriter", infectionLogWriter);
            writers.put("malwareLogWriter", malwareLogWriter);
            writers.put("contactLogWriter", contactLogWriter);
            return writers;
        }
        return null;
    }

    // To fix memory issue
    public void writeLogToDisk(JsonGenerator writer, Map<String, Object> data) {
        ObjectMapper mapper = new ObjectMapper(); // Create a temporary ObjectMapper
        mapper.enable(SerializationFeature.INDENT_OUTPUT); // Enable pretty-printing
        try {
            mapper.writeValue(writer, data); // Serialize data to the writer
            writer.flush(); // Ensure data is written to disk
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                writer.flush(); // Double-check flushing
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
    
}
