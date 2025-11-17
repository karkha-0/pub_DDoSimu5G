/* 
 * Copyright 2010 Aalto University, ComNet
 * Released under GPLv3. See LICENSE.txt for details. 
 * Updated 2024 byLund Univeristy, EIT, Security and Networks group
 * Update allow to log infection propagation into JSON
 */

package routing;

import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;

import org.json.JSONObject;

import com.fasterxml.jackson.core.JsonFactory;
import com.fasterxml.jackson.core.JsonGenerator;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

import core.DTNHost;
import core.Message;
import core.Settings;
import core.SimClock;
import routing.util.ConnectionInfo;

/**
 * Epidemic message router with drop-oldest buffer and only single transferring
 * connections at a time.
 */
public class EpidemicRouter extends ActiveRouter {
	
	private boolean isInfected = false; // Infection status
	private double malwareRecvTime = -1; // Time when the malware was received

	private static JsonGenerator infectionLogWriter = null;
	private String reportDir;
	private DTNHost srcMalwareNode; // The node that infected this node

	/**
	 * Constructor. Creates a new message router based on the settings in
	 * the given Settings object.
	 * @param s The settings object
	 */
	public EpidemicRouter(Settings s) {
		super(s);
		//TODO: read&use epidemic router specific settings (if any)

		// Lund added this section
		Settings reportSettings = new Settings("Report");
        this.reportDir = reportSettings.getSetting("reportDir");

		try {
            ObjectMapper mapper = new ObjectMapper();
			JsonFactory jsonFactory = new JsonFactory(mapper); // Attach ObjectMapper as codec
		
			infectionLogWriter = jsonFactory.createGenerator(new FileWriter(reportDir + "/infection_log.json"));
			infectionLogWriter.writeStartArray();
		} catch (IOException e) {
            e.printStackTrace();
        }
		// End
	}	
	/**
	 * Copy constructor.
	 * @param r The router prototype where setting values are copied from
	 */
	protected EpidemicRouter(EpidemicRouter r) {
		super(r);
		//TODO: copy epidemic settings here (if any)

		// Lund added this section
		this.isInfected = r.isInfected;
		this.malwareRecvTime = r.malwareRecvTime;
		this.srcMalwareNode = r.srcMalwareNode;
		//End
	}
			
	@Override
	public void update() {
		super.update();

		// Lund added this section
		if (!isInfected && malwareRecvTime > 0) {

			isInfected = true;

            JSONObject infectionData = new JSONObject(new LinkedHashMap<>());
            infectionData.put("node_id", getHost().getAddress());
            infectionData.put("infected_by", srcMalwareNode.getAddress());
            infectionData.put("malware_active_time", SimClock.getTime());
            infectionData.put("node_postion_at_active_infected", getHost().getLocation().toString());
            infectionData.put("infection_status", "Infected Active");
            //infectionDataArray.put(infectionData);

            // To fix memory issue
			ObjectMapper mapper = new ObjectMapper(); // Create a temporary ObjectMapper
            mapper.enable(SerializationFeature.INDENT_OUTPUT); // Enable pretty-printing
			try {
				mapper.writeValue(infectionLogWriter, infectionData.toMap()); // Serialize data to the writer
				infectionLogWriter.flush(); // Ensure data is written to disk
			} catch (IOException e) {
				e.printStackTrace();
			} finally {
				try {
					infectionLogWriter.flush(); // Double-check flushing
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}
		// End

		if (isTransferring() || !canStartTransfer()) {
			return; // transferring, don't try other connections yet
		}
		
		// Try first the messages that can be delivered to final recipient
		if (exchangeDeliverableMessages() != null) {
			return; // started a transfer, don't try others (yet)
		}
		
		// then try any/all message to any/all connection
		this.tryAllMessagesToAllConnections();
	}

	// Lund added the method override
	@Override
    public int receiveMessage(Message m, DTNHost from) {
		int result = super.receiveMessage(m, from);
		if (m.getTo().equals(this.getHost())) {
			if (!isInfected) {
				if (result == RCV_OK) {
					malwareRecvTime = SimClock.getTime();
					System.out.println("Node " + this.getHost().getAddress() +
						" received infection from node " + from.getAddress() +
						" at " + SimClock.getTime());
					srcMalwareNode = from;
				}
				else {
					System.err.println("-->Cant Rececive message " + m.getId() +  ", from " + m.getFrom() + 
										", to " + m.getTo() + ". Results is :"  + result);
					if (result == 1) {
						System.out.println("Print istransferring : " + isTransferring());

					}
				}
			}
		}
		return result;
	}
	//End

	@Override
	public EpidemicRouter replicate() {
		return new EpidemicRouter(this);
	}

	// Lund added this section. To finalize Json file at end of sim
    public static Map<String, JsonGenerator> getLogWriters() {
		if (infectionLogWriter != null)
		{
        	Map<String, JsonGenerator> writers = new HashMap<>();
        	writers.put("infectionLogWriter", infectionLogWriter);
			writers.put("malwareLogWriter", null);
			writers.put("contactLogWriter", null);
        	return writers;
		}
		return null;
    }
	//End

}