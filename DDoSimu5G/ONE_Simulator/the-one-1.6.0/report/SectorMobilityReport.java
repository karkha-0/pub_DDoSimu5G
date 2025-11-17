/* 
 * Copyright 2024, Lund Univeristy, EIT, Security and Networks group
 * Released under GPLv3. See LICENSE.txt for details. 
 */

package report;

import com.fasterxml.jackson.core.JsonFactory;
import com.fasterxml.jackson.core.JsonGenerator;
import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.core.JsonToken;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import core.Coord;
import core.Settings;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * Logs sector-based mobility traces for nodes in JSON format with continuous writing.
 */
public class SectorMobilityReport extends Report {
    private final String reportDir;
    private final String scenarioName;
    private final double logInterval;
    private final int sectorSize;
    private final int worldWidth;
    private final int worldHeight;

    private final Map<Integer, Coord> sectorCoord;  // Sector ID -> Center Coord
    private final ConcurrentMap<Integer, Integer> nodeSectors;  // Node ID -> Current Sector
    private final ConcurrentMap<Integer, Double> lastLoggedTime;  // Node ID -> Last Log Time

    private final File reportFile;
    private final File tempFile;
    private JsonGenerator jsonGenerator;

    private static SectorMobilityReport INSTANCE = null;

    public SectorMobilityReport() {
        //init();

        INSTANCE = this;

        Settings reportSettings = new Settings("Report");
        this.reportDir = reportSettings.getSetting("reportDir", "reports");
        this.logInterval = reportSettings.getDouble("intervalMobilityLog", 60.0);

        Settings scenarioSettings = new Settings("Scenario");
        this.scenarioName = scenarioSettings.getSetting("name", "default_scenario");

        Settings movementSettings = new Settings("MovementModel");
        int [] worldSize = movementSettings.getCsvInts("worldSize", 2);
        this.worldWidth = worldSize[0];
		this.worldHeight = worldSize[1];

        this.sectorSize = reportSettings.getInt("sectorSize", 500);
        this.nodeSectors = new ConcurrentHashMap<>();
        this.lastLoggedTime = new ConcurrentHashMap<>();
        this.sectorCoord = calculateSectorCoord();

        // Prepare the report files
        this.reportFile = new File(reportDir, scenarioName + "_SectorMobility.json");
        this.tempFile = new File(reportDir, scenarioName + "_temp_SectorMobility.json");

        new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT);

        try {
            // Prepare JsonGenerator for continuous appending
            JsonFactory jsonFactory = new JsonFactory();
            FileWriter fileWriter = new FileWriter(tempFile);
            jsonGenerator = jsonFactory.createGenerator(fileWriter);
            jsonGenerator.useDefaultPrettyPrinter();
            jsonGenerator.writeStartArray();
        } catch (IOException e) {
            throw new RuntimeException("Failed to initialize JSON generator", e);
        }
    }
    
    public static SectorMobilityReport getInstance() {
        return INSTANCE;
    }

    /**
     * Calculate sector centers based on world dimensions and sector size.
     */
    private Map<Integer, Coord> calculateSectorCoord() {
        Map<Integer, Coord> coords = new HashMap<>();
        int sectorId = 0;
        for (int y = 0; y < worldHeight; y += sectorSize) {
            for (int x = 0; x < worldWidth; x += sectorSize) {
                double centerX = x + (sectorSize / 2.0);
                double centerY = y + (sectorSize / 2.0);
                coords.put(sectorId++, new Coord(centerX, centerY));
            }
        }
        return coords;
    }

    /**
     * Log sector mobility trace for a node.
     */
    public synchronized void logSectorMobility(int nodeId, Coord position, double time) {
        if (lastLoggedTime.containsKey(nodeId) && time - lastLoggedTime.get(nodeId) < logInterval) {
            return;  // Skip if log interval hasn't passed
        }

        /*double lastTime = lastLoggedTime.getOrDefault(nodeId, -1.0);
    
        // Ensure logging happens only at fixed intervals
        if (lastTime > 0 && (time - lastTime) < logInterval) {
            return;  // Skip logging if interval hasn't passed
        }*/

        int currentSector = calculateSectorId(position);
        Integer previousSector = nodeSectors.get(nodeId);  // Retrieve the previous sector without updating
        lastLoggedTime.put(nodeId, time);

        // Log only if the sector has changed
        if (previousSector != null && previousSector.equals(currentSector)) {
            /*System.out.println("DEBUG: Node " + nodeId + 
                   " moved from sector " + previousSector + 
                   " to sector " + currentSector + 
                   " at time " + time);*/
            return;
        }
        /*else {
            System.out.println(">>>>>>>>DEBUG: Node " + nodeId + 
                                " moved from sector " + previousSector + 
                                " to sector " + currentSector + 
                                " at time " + time);
        }*/

        try {
            jsonGenerator.writeStartObject();  // Start a movement object
            jsonGenerator.writeNumberField("nodeId", nodeId);
            jsonGenerator.writeNumberField("sector", currentSector);
            jsonGenerator.writeNumberField("timestamp", (double) Math.round((double)time / 60.0)); // Time in mins
            jsonGenerator.writeEndObject();  // End the movement object
            jsonGenerator.flush();  // Ensure data is written immediately
        } catch (IOException e) {
            System.err.println("Error writing mobility trace: " + e.getMessage());
        }

        // Update the sector map only after comparison
        nodeSectors.put(nodeId, currentSector);
        lastLoggedTime.put(nodeId, time);
    }

    /**
     * Calculate sector ID based on position.
     */
    private int calculateSectorId(Coord position) {
        int xSector = (int) (position.getX() / sectorSize);
        int ySector = (int) (position.getY() / sectorSize);
        int sectorsPerRow = worldWidth / sectorSize;

        /*System.out.println("DEBUG: Calculated sector ID for position " + position + 
                        " as " + (ySector * sectorsPerRow + xSector));*/

        return ySector * sectorsPerRow + xSector;
    }

    @Override
    public void done() {
        try {
            jsonGenerator.writeEndArray();  // Close the array in the temp file
            jsonGenerator.close();
            System.out.println("DEBUG: Closed temporary file.");
        } catch (IOException e) {
            System.err.println("Error finalizing temporary file: " + e.getMessage());
        }

        // Reorganize the temporary file into the final JSON structure
        reorganizeLog();
    }

    public synchronized void reorganizeLog() {
        //File finalFile = new File(reportFile.getParent(), "SectorMobility.json");
    
        try (JsonParser parser = new JsonFactory().createParser(tempFile);
             JsonGenerator writer = new JsonFactory().createGenerator(new FileWriter(this.reportFile))) {
    
            writer.useDefaultPrettyPrinter();
            writer.writeStartObject();  // Start the root object
    
            // Start "movements" object
            writer.writeObjectFieldStart("movements");
            Map<Integer, List<Map<String, Object>>> groupedLogs = new HashMap<>();
    
            parser.nextToken();  // Start array in temp file
            while (parser.nextToken() != JsonToken.END_ARRAY) {
                // Parse each movement object
                //Map<String, Object> log = new ObjectMapper().readValue(parser, Map.class);
                Map<String, Object> log = new ObjectMapper().readValue(parser, new TypeReference<Map<String, Object>>() {});
                int nodeId = (int) log.get("nodeId");
    
                // Add movement to the in-memory group for this node
                groupedLogs.computeIfAbsent(nodeId, k -> new ArrayList<>()).add(Map.of(
                    "sector", log.get("sector"),
                    "timestamp", log.get("timestamp")
                ));
    
                // If memory usage exceeds a certain threshold, flush groups to the final file
                if (groupedLogs.size() > 1000) {  // Adjust threshold as needed
                    flushGroupedLogs(writer, groupedLogs);
                    groupedLogs.clear();  // Clear the in-memory map
                }
            }
    
            // Flush any remaining grouped logs
            flushGroupedLogs(writer, groupedLogs);
            writer.writeEndObject();  // End "movements" object
    
            // Write "sectors" object
            writer.writeObjectFieldStart("sectors");
            for (Map.Entry<Integer, Coord> entry : sectorCoord.entrySet()) {
                writer.writeObjectFieldStart(String.valueOf(entry.getKey()));  // Start sector entry
                writer.writeFieldName("coord");
                writer.writeStartObject();
                writer.writeNumberField("x", entry.getValue().getX());
                writer.writeNumberField("y", entry.getValue().getY());
                writer.writeEndObject();  // End "coord" object
                writer.writeEndObject();  // End sector entry
            }
            writer.writeEndObject();  // End "sectors" object
    
            writer.writeEndObject();  // End root object
            writer.close();
            System.out.println("DEBUG: Finalized SectorMobilityReport.");
    
        } catch (IOException e) {
            System.err.println("Error reorganizing JSON log: " + e.getMessage());
        }

        // Delete the temporary file
        if (tempFile.exists() && !tempFile.delete()) {
            System.err.println("Failed to delete temporary file: " + tempFile.getAbsolutePath());
        } else {
            System.out.println("DEBUG: Temporary file deleted: ");
        }
    }
    private void flushGroupedLogs(JsonGenerator writer, Map<Integer, List<Map<String, Object>>> groupedLogs) throws IOException {
        for (Map.Entry<Integer, List<Map<String, Object>>> entry : groupedLogs.entrySet()) {
            writer.writeFieldName(String.valueOf(entry.getKey()));  // Node ID
            writer.writeStartArray();  // Start array for node's movements
            for (Map<String, Object> trace : entry.getValue()) {
                writer.writeStartObject();
                writer.writeNumberField("sector", (int) trace.get("sector"));
                writer.writeNumberField("timestamp", (double) trace.get("timestamp"));
                writer.writeEndObject();
            }
            writer.writeEndArray();  // End array for node's movements
        }
    }
    
    
}


