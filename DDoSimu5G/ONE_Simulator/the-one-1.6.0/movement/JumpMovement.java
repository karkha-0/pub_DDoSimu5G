/* 
 * Copyright 2024, Lund Univeristy, EIT, Security and Networks group
 * Released under GPLv3. See LICENSE.txt for details. 
 */

package movement;

import core.Coord;
import core.Settings;
import core.SimClock;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

/**
 * A custom movement model where nodes perform coordinated jumps to predefined locations.
 */
public class JumpMovement extends MovementModel {
    private Coord[] initialPositions;  // Grid positions of nodes
    private Coord startLocation;  // Starting point for the
    private List<Coord> jumpLocations;  
    private int currentJumpIndex;  // Current jump location index
    private double jumpInterval;  // Time between jumps
    private double stayDuration;  // Time to stay at each location
    private double nextJumpTime;  // Time of the next jump

    public JumpMovement(Settings settings) {
        super(settings);
    
        // Create a scoped Settings object for JumpMovement
        Settings jumpSettings = new Settings("JumpMovement");
        Settings groupSettings = new Settings("Group");
    
        // Retrieve jump interval and stay duration
        this.jumpInterval = jumpSettings.getDouble("jumpInterval", 300);  // Default 5 minutes
        this.stayDuration = jumpSettings.getDouble("stayDuration", 600);  // Default 10 minutes
        

        // Load the starting location for the grid
        int [] startLocationConfig = jumpSettings.getCsvInts("startLocation", 2);
        this.startLocation = new Coord(startLocationConfig[0], startLocationConfig[1]);

        // Load the number of hosts and grid layout
        int nrOfHosts = groupSettings.getInt("nrofHosts");

        int gridColumns = jumpSettings.getInt("gridColumns", 10);  // Default to 10 columns
        this.initialPositions = calculateGridPositions(startLocation, nrOfHosts, gridColumns);
            
        // Retrieve the file path for jump locations
        String jumpLocationsFile = jumpSettings.getSetting("locationsFile");
        if (jumpLocationsFile == null || jumpLocationsFile.trim().isEmpty()) {
            throw new IllegalArgumentException("JumpMovement.locationsFile is not specified or empty in the configuration.");
        }
    
        // Read jump locations from the file
        jumpLocations = new ArrayList<>();
        try (BufferedReader br = new BufferedReader(new FileReader(new File(jumpLocationsFile.trim())))) {
            String line;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (!line.isEmpty()) {
                    try {
                        String[] coords = line.split(",");
                        double x = Double.parseDouble(coords[0].trim());
                        double y = Double.parseDouble(coords[1].trim());
                        jumpLocations.add(new Coord(x, y));
                    } catch (Exception e) {
                        throw new IllegalArgumentException("Invalid format in jump locations file: " + line, e);
                    }
                }
            }
        } catch (Exception e) {
            throw new RuntimeException("Failed to read jump locations from file: " + jumpLocationsFile, e);
        }
    
        // Debug loaded jump locations
        //System.out.println("DEBUG: Jump locations loaded: " + jumpLocations);
    
        // Initialize variables
        this.currentJumpIndex = 0;
        this.nextJumpTime = SimClock.getTime() + jumpInterval;
    }


    /*@Override
    public Coord getInitialLocation() {
        // Start at the first jump location
        //return jumpLocations.get(0).clone();
        //return initialPositions[getHost().getAddress()].clone();
        return initialPositions[super.getHost().getAddress()].clone();
    }*/
    @Override
    public Coord getInitialLocation() {
        int address = super.getHost().getAddress();  // Get the host address
        Coord initialPosition = initialPositions[address].clone();
        return initialPosition;
    }


    /*@Override
    public Path getPath() {
        double currentTime = SimClock.getTime();
        Path path = new Path(generateSpeed());

        // Check if it's time for the next jump
        if (currentTime >= nextJumpTime) {
            // Move to the next location
            currentJumpIndex = (currentJumpIndex + 1) % jumpLocations.size();  // Loop through locations
            Coord nextLocation = jumpLocations.get(currentJumpIndex);
            path.addWaypoint(nextLocation);

            // Update next jump time
            nextJumpTime = currentTime + jumpInterval + stayDuration;
            //System.out.println("DEBUG: Node " + this.getHost().getAddress() + 
            //                   " jumped to " + nextLocation + 
            //                   " and will stay for " + stayDuration + " seconds.");
        } else {
            // Stay at the current location
            path.addWaypoint(jumpLocations.get(currentJumpIndex));
        }

        return path;
    }*/
    @Override
    public Path getPath() {
        double currentTime = SimClock.getTime();
        Path path = new Path(generateSpeed());

        // Check if it's time for the next jump
        if (currentTime >= nextJumpTime) {
            // Move to the next location (adjusted relative to the node's grid position)
            currentJumpIndex = (currentJumpIndex + 1) % jumpLocations.size();  // Loop through locations
            Coord baseJumpLocation = jumpLocations.get(currentJumpIndex);
            Coord originalPosition = initialPositions[super.getHost().getAddress()];
            Coord nextLocation = new Coord(
                baseJumpLocation.getX() + originalPosition.getX(),
                baseJumpLocation.getY() + originalPosition.getY()
            );
            path.addWaypoint(nextLocation);

            // Update next jump time
            nextJumpTime = currentTime + jumpInterval + stayDuration;
            /*System.out.println("DEBUG: Node " + this.getHost().getAddress() +
                               " jumped to " + nextLocation +
                               " and will stay for " + stayDuration + " seconds.");
            */
        } else {
            // Stay at the current location
            Coord originalPosition = initialPositions[super.getHost().getAddress()];
            Coord currentLocation = new Coord(
                jumpLocations.get(currentJumpIndex).getX() + originalPosition.getX(),
                jumpLocations.get(currentJumpIndex).getY() + originalPosition.getY()
            );
            path.addWaypoint(currentLocation);
        }

        return path;
    }

    @Override
    public MovementModel replicate() {
        return new JumpMovement(this);  // Call the prototype constructor
    }

 /**
     * Calculate grid positions relative to the starting location.
     */
    private Coord[] calculateGridPositions(Coord startLocation, int nrOfHosts, int gridColumns) {
        Coord[] positions = new Coord[nrOfHosts];
        double spacing = 20;  // Distance between grid points

        for (int i = 0; i < nrOfHosts; i++) {
            int row = i / gridColumns;
            int col = i % gridColumns;
            positions[i] = new Coord(
                startLocation.getX() + col * spacing,
                startLocation.getY() + row * spacing
            );
            // Debug log for each position
            //System.out.println("DEBUG: Host " + i + " initial position: " + positions[i]);
        }
        return positions;
    }

    // Prototype constructor
    public JumpMovement(JumpMovement proto) {
        super(proto);
        this.jumpLocations = new ArrayList<>(proto.jumpLocations);
        this.currentJumpIndex = proto.currentJumpIndex;
        this.jumpInterval = proto.jumpInterval;
        this.stayDuration = proto.stayDuration;
        this.nextJumpTime = proto.nextJumpTime;
        this.initialPositions = proto.initialPositions.clone();
        this.startLocation = proto.startLocation.clone();
    }
}
