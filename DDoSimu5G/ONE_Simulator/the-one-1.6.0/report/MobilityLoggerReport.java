/* 
 * Copyright 2024, Lund Univeristy, EIT, Security and Networks group
 * Released under GPLv3. See LICENSE.txt for details. 
 */

package report;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.core.JsonFactory;
import com.fasterxml.jackson.core.JsonGenerator;
import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.core.JsonToken;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

import core.Coord;
import core.Settings;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * Logs mobility traces for nodes in JSON format with continuous writing.
 */
public class MobilityLoggerReport extends Report {

    private String reportDir;
    private String scenarioName;
    private double logInterval;

    private final ConcurrentMap<Integer, Double> lastLoggedTime = new ConcurrentHashMap<>();
    private final File reportFile;

    private final ObjectMapper objectMapper;
    private JsonGenerator jsonGenerator;

    private static MobilityLoggerReport INSTANCE = null;

    public MobilityLoggerReport() {
        init();

        INSTANCE = this;

        Settings reportSettings = new Settings("Report");
        this.reportDir = reportSettings.getSetting("reportDir", "reports");
        this.logInterval = reportSettings.getDouble("intervalMobilityLog");

        Settings scenarioSettings = new Settings("Scenario");
        this.scenarioName = scenarioSettings.getSetting("name", "default_scenario");

        // Prepare the report file
        reportFile = new File(reportDir, scenarioName + "_MobilityTraces.json");

        // Initialize ObjectMapper
            objectMapper = new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT);

        try {
            // Prepare JsonGenerator for continuous appending
            JsonFactory jsonFactory = new JsonFactory();
            FileWriter fileWriter = new FileWriter(reportFile);
            jsonGenerator = jsonFactory.createGenerator(fileWriter);
            jsonGenerator.useDefaultPrettyPrinter();

            // Start the root object
            jsonGenerator.writeStartObject();
        } catch (IOException e) {
            throw new RuntimeException("Failed to initialize JSON generator", e);
        }
    }

    @Override
    public void init() {
        lastLoggedTime.clear();
    }

    public static MobilityLoggerReport getInstance() {
        return INSTANCE;
    }

    public static void logMobility(int nodeId, Coord position, double time) {
        if (getInstance() != null) {
            getInstance().logMobilityInstance(nodeId, position, time);
        }
        if (SectorMobilityReport.getInstance() != null) {
            // Pass the same data to SectorMobilityReport for sector-based logging
            SectorMobilityReport.getInstance().logSectorMobility(nodeId, position, time);       
        }
    }

    public synchronized void logMobilityInstance(int nodeId, Coord position, double time) {
        if (lastLoggedTime.containsKey(nodeId) && time - lastLoggedTime.get(nodeId) < logInterval) {
            return; // Skip if log interval hasn't passed
        }
    
        lastLoggedTime.put(nodeId, time);
    
        try {
            // Write log entry directly to the JSON file
            jsonGenerator.writeFieldName("nodeId: " + nodeId);
            jsonGenerator.writeStartObject();
            jsonGenerator.writeFieldName("position");
    
            // Use ObjectMapper to write Position object
            objectMapper.writeValue(jsonGenerator, new Position(position.getX(), position.getY()));
    
            jsonGenerator.writeFieldName("timestamp");
            jsonGenerator.writeNumber(time);
            jsonGenerator.writeEndObject();  // Ensure proper closure of this object
            jsonGenerator.flush(); // Ensure data is written immediately
        } catch (IOException e) {
            System.err.println("Error writing log for node " + nodeId + ": " + e.getMessage());
        }

        //System.out.println("DEBUG: Delegated mobility log for node " + nodeId + " to SectorMobilityReport.");
    }
    

    @Override
    public void done() {
        finalizeLog(); // Ensure proper closure of the JSON structure
    }

    public static void finalizeLog() {
        if (getInstance() != null)
            getInstance().finalizeLogInstance();
    }

    public synchronized void finalizeLogInstance() {
        try {
            // Finalize JSON structure if the generator is open
            if (jsonGenerator != null) {
                // Properly close the root object
                if (!jsonGenerator.isClosed()) {
                    jsonGenerator.writeEndObject(); // Close the root object
                    jsonGenerator.close();         // Close the generator
                }
            }

            // Reorganize the JSON file
            reorganizeLog();

        } catch (IOException e) {
            System.err.println("Error finalizing JSON log: " + e.getMessage());
        }
    }

    public synchronized void reorganizeLog() {
        File tempFile = new File(reportFile.getParent(), "temp_" + reportFile.getName());

        try (JsonParser parser = new JsonFactory().createParser(reportFile);
            JsonGenerator writer = new JsonFactory().createGenerator(new FileWriter(tempFile))) {

            writer.useDefaultPrettyPrinter();
            writer.writeStartObject(); // Start the new JSON object

            Map<String, GroupedLog> groupedLogs = new TreeMap<>(
                Comparator.comparingInt(Integer::parseInt) // Numeric sorting
            );

            parser.nextToken(); // Start object
            while (parser.nextToken() != JsonToken.END_OBJECT) {
                String fieldName = parser.getText(); // Get the field name
                parser.nextToken(); // Move to the value of the field

                String nodeId = fieldName.replace("nodeId: ", "");

                // Determine if the entry is a single log or a grouped log
                JsonNode node = objectMapper.readTree(parser);

                if (node.has("position") && node.has("timestamp")) {
                    SingleLog log = objectMapper.treeToValue(node, SingleLog.class);
                    groupedLogs.computeIfAbsent(nodeId, k -> new GroupedLog()).addLog(log);
                } else if (node.has("positions") && node.has("timestamps")) {
                    GroupedLog log = objectMapper.treeToValue(node, GroupedLog.class);
                    groupedLogs.put(nodeId, log); // Overwrite or merge as necessary
                } else {
                    System.err.println("Unrecognized JSON structure for nodeId: " + nodeId);
                }
            }

            // Write grouped logs incrementally to the new file
            for (Map.Entry<String, GroupedLog> entry : groupedLogs.entrySet()) {
                writer.writeFieldName(entry.getKey());
                objectMapper.writeValue(writer, entry.getValue());
            }

            // Remove
            groupedLogs.clear();

            writer.writeEndObject(); // End the JSON object
        } catch (IOException e) {
            System.err.println("Error reorganizing JSON log: " + e.getMessage());
        }

        // Replace the original file with the reorganized file
        if (!tempFile.renameTo(reportFile)) {
            System.err.println("Failed to replace the original JSON file with the reorganized file.");
        }
    }

    // Represents a single log entry
    private static class SingleLog {
        @JsonProperty("position")
        private Position position;

        @JsonProperty("timestamp")
        private double timestamp;

        public Position getPosition() {
            return position;
        }

        public double getTimestamp() {
            return timestamp;
        }
    }

    // Represents grouped logs for a node
    private static class GroupedLog {
        private final List<Position> positions = new ArrayList<>();
        private final List<Double> timestamps = new ArrayList<>();

        public void addLog(SingleLog log) {
            positions.add(log.getPosition());
            timestamps.add(log.getTimestamp());
        }

        @JsonProperty("positions")
        public List<Position> getPositions() {
            return positions;
        }

        @JsonProperty("timestamps")
        public List<Double> getTimestamps() {
            return timestamps;
        }
    }

    // Represents a position with x and y coordinates
    private static class Position {
        @JsonProperty("x")
        private final double x;

        @JsonProperty("y")
        private final double y;

        // Add a constructor for deserialization
        @JsonCreator
        public Position(@JsonProperty("x") double x, @JsonProperty("y") double y) {
            this.x = x;
            this.y = y;
        }
        
    }
}
