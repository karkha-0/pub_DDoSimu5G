package routing;

import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONObject;

import com.fasterxml.jackson.core.JsonFactory;
import com.fasterxml.jackson.core.JsonGenerator;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

import core.Connection;
import core.Coord;
import core.DTNHost;
import core.Message;
import core.Settings;
import core.SimClock;
import routing.util.ConnectionInfo;
import routing.util.ContactInfo;

/**
 * InfectionRouter behaves like EpidemicRouter but logs infection events.
 */
public class InfectionRouter extends ActiveRouter {

    private boolean isInfected = false; // Infection status
    private double malwareRecvTime = -1; // Time when the malware was received
    private DTNHost srcMalwareNode; // The node that infected this node

    private int installationTime;         // Time required for malware to be installed on the node
    private int malwareSizeKB;        // Size of the malware message in kilobytes

    // JSON logging
    private static Set<Integer> infectedNodesSet = new HashSet<>();
    private static JSONArray infectionDataArray = new JSONArray();
    private static JSONArray malwarePropagationDataArray = new JSONArray();

    // To fix memory issue
    private static JsonGenerator infectionLogWriter = null;
    private static JsonGenerator malwareLogWriter = null;
    private static JsonGenerator contactLogWriter = null;
    private String reportDir;

    protected Map<String, Message> messagesInProgress = new HashMap<>();          
    protected static Map<String, ContactInfo> contactDetails = new ConcurrentHashMap<>(); // hashmap to access all kind of info abut a connection
    protected static Map<String, ConnectionInfo> connectionDetails = new ConcurrentHashMap<>(); // hashmap to access all kind of info abut a connection

    /**
     * Constructor: Initializes InfectionRouter with simulation settings.
     */
    public InfectionRouter(Settings s) {
        super(s);

        // To fix memory issue
        Settings reportSettings = new Settings("Report");
        this.reportDir = reportSettings.getSetting("reportDir");

        String infectedNodesRange = s.getSetting("infectedNodesRange", "0,10");
        if (!infectedNodesRange.isEmpty()) {
            markInitialInfectedNodes(infectedNodesRange);
        }

        this.installationTime = s.getInt("installationTime",10); // Default to 10 second
        this.malwareSizeKB = s.getInt("malwareSize", 500); // Default 500kB

        System.out.println(this.reportDir);
        //Tryint to fix memory issue
        initLogWriters(this.reportDir); // Initialize log writers here
    }

    /**
     * Copy constructor to replicate the router.
     */
    protected InfectionRouter(InfectionRouter r) {
        super(r);
        this.isInfected = r.isInfected;
        this.malwareRecvTime = r.malwareRecvTime;
        this.srcMalwareNode = r.srcMalwareNode;

        this.installationTime = r.installationTime;
        this.malwareSizeKB = r.malwareSizeKB;
    }

    /**
     * Main update function - Follows Epidemic-style spreading while logging infections.
     */
    @Override
    public void update() {
        super.update();

        if (!isInfected && malwareRecvTime > 0 && 
            SimClock.getTime() - malwareRecvTime >= installationTime) {

            isInfected = true;
            infectedNodesSet.add(getHost().getAddress());

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
        }

        if (isTransferring() || !canStartTransfer()) {
            return;
        }

        // Try delivering messages first
        if (exchangeDeliverableMessages() != null) {
            return;
        }
               
        // Then try spreading infection opportunistically (like EpidemicRouter)
        this.tryAllMessagesToAllConnections();
    }

    /**
     * Handles new connections and spreads infection immediately.
     */
    @Override
    public void changedConnection(Connection con) {
        if (con.isUp() && isInfected) {
            DTNHost otherNode = con.getOtherNode(getHost());

            //System.out.println("Infected node " + getHost().getAddress() +
            //    " encountered " + otherNode.getAddress() + " at " + SimClock.getTime());

            // Create and send malware immediately
            Message malwareMessage = new Message(getHost(), otherNode,
                "Malware" + getHost().getAddress(), malwareSizeKB);

            //String connectionKey = createConnectionKey(getHost(),otherNode);
            //ConnectionInfo connInfo = getOrCreateConnectionInfo(connectionKey);
            //connInfo.transferStartTime = SimClock.getTime();
            //connectionDetails.put(connectionKey, connInfo);

            startTransfer(malwareMessage, con);

        }
    }

    /**
     * Starts message transfer without delay.
     */
    @Override
    protected int startTransfer(Message m, Connection con) {

        DTNHost targetNode = con.getOtherNode(getHost());
        String connectionKey = createConnectionKey(getHost(),targetNode);
        ConnectionInfo connInfo = getOrCreateConnectionInfo(connectionKey);
        connInfo.transferStartTime = SimClock.getTime();
        connectionDetails.put(connectionKey, connInfo);

        return super.startTransfer(m, con);
    }

    /**
     * Receives messages and logs infection.
     */
    @Override
    public int receiveMessage(Message m, DTNHost from) {
        if (m.getTo().equals(this.getHost())) {
            if (!isInfected) {
                int result = super.receiveMessage(m, from);
                if (result == RCV_OK) {
                    System.out.println("Node " + this.getHost().getAddress() +
                        " received infection from node " + from.getAddress() +
                        " at " + SimClock.getTime());

                    srcMalwareNode = from;
                    malwareRecvTime = SimClock.getTime();

                    String connectionKey = createConnectionKey(this.getHost(),from);
                    ConnectionInfo connInfo = getOrCreateConnectionInfo(connectionKey);
                    connInfo.transferEndTime = SimClock.getTime();
                    connInfo.messageId = m.getId();
                    connInfo.fromNode = from;
                    connInfo.toNode = this.getHost();
                    connInfo.messageSize = m.getSize();
                    connInfo.messageCreationTime = m.getCreationTime();
                    connectionDetails.put(connectionKey, connInfo);

                    //logMalwarePropagation(from, this.getHost(), malwareRecvTime);
                    // Log message propgation data for sender and receiver
                    logMalwarePropData(from , m.getTo());       
                }
                return result;
            }
            return DENIED_OLD;
        }
        return DENIED_POLICY;
    }

    /**
     * Replicates the router.
     */
    @Override
    public InfectionRouter replicate() {
        return new InfectionRouter(this);
    }

    /**
     * Retrieves the set of infected nodes.
     * 
     * @return A set of node IDs representing infected nodes
     */
    public static Set<Integer> getInfectedNodesSet() {
        return infectedNodesSet;
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
     * Retrieves the class object of connection info if existing
     * Else it create one
     * @return Class obhect of ConnectionInfo
     */
    public static ConnectionInfo getOrCreateConnectionInfo(String connectionKey) {
        ConnectionInfo connInfo = connectionDetails.getOrDefault(connectionKey, new ConnectionInfo());
        return connInfo;
    }

    /**
     * Logs malware transmission.
     */
    /*private void logMalwarePropagation(DTNHost from, DTNHost to, double time) {
        JSONObject malwarePropagationData = new JSONObject(new LinkedHashMap<>());
        malwarePropagationData.put("from_node", from.getAddress());
        malwarePropagationData.put("to_node", to.getAddress());
        malwarePropagationData.put("malware_recv_time", time);
        malwarePropagationData.put("infection_status", "Infected");

        malwarePropagationDataArray.put(malwarePropagationData);
    }*/

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

    // To fix memory issue
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
