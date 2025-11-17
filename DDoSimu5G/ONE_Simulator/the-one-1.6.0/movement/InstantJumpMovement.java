/* 
 * Copyright 2024, Lund Univeristy, EIT, Security and Networks group
 * Released under GPLv3. See LICENSE.txt for details. 
 */

package movement;

import core.Coord;
import core.Settings;
import core.SimClock;

/**
 * A movement model where nodes instantly teleport between a starting position and a fixed random destination.
 */
public class InstantJumpMovement extends MovementModel {
    private Coord startPosition;  // Initial random position
    private Coord destination;    // Randomly assigned destination
    private double stayDuration;  // Time to stay at each position
    private double nextJumpTime;  // Time when the next jump happens
    private double teleportDelay; // Optional delay before appearing at the new location
    private double maxX, maxY;    // World size boundaries

    public InstantJumpMovement(Settings settings) {
        super(settings);
        
        // Set movement model namespace
        settings.setNameSpace(MOVEMENT_MODEL_NS);

        // Retrieve world size from settings
        int[] worldSize = settings.getCsvInts(WORLD_SIZE, 2);
        this.maxX = worldSize[0];
        this.maxY = worldSize[1];

        Settings instantJumpSettings = new Settings("InstantJumpMovement");

        // Retrieve configuration values
        this.stayDuration = instantJumpSettings.getDouble("stayDuration", 60); // Default 10 minutes
        this.teleportDelay = instantJumpSettings.getDouble("teleportDelay", 0); // Default instant

        // Assign unique random positions for each node
        this.startPosition = randomCoord();
        this.destination = randomCoordDifferentFrom(startPosition);

        // Set the first jump time
        this.nextJumpTime = SimClock.getTime() + stayDuration;
    }

    @Override
    public Coord getInitialLocation() {
        return startPosition.clone();
    }

    @Override
    public Path getPath() {
        double currentTime = SimClock.getTime();
        Path path = new Path(generateSpeed());

        if (super.getHost() == null) {
            return path; // Avoid NullPointerException if host is not assigned
        }

        // If it's time to jump, switch positions instantly
        if (currentTime >= nextJumpTime) {
            Coord nextLocation = (startPosition.equals(super.getHost().getLocation())) 
                                 ? destination 
                                 : startPosition;
            
            // Log the movement speed   
            /*System.out.println("DEBUG: Node " + super.getHost().getAddress()
            + " jumped from " + super.getHost().getLocation()
            + " to " + nextLocation 
            + " at time " + currentTime
            + " with speed " + generateSpeed());*/

            // Introduce optional teleport delay before appearing at the destination
            if (teleportDelay > 0) {
                path.addWaypoint(super.getHost().getLocation());  // Stay at current location briefly
                path.addWaypoint(nextLocation, teleportDelay);    // Appear at the next location after delay
            } else {
                path.addWaypoint(nextLocation);  // Instant jump
            }

            // Schedule the next jump
            nextJumpTime = currentTime + stayDuration;
        } else {
            // Stay in place
            path.addWaypoint(super.getHost().getLocation());
        }

        return path;
    }

    @Override
    public MovementModel replicate() {
        return new InstantJumpMovement(this); // Call the prototype constructor
    }

    // Prototype constructor
    public InstantJumpMovement(InstantJumpMovement proto) {
        super(proto);
        this.maxX = proto.maxX;
        this.maxY = proto.maxY;
        this.startPosition = proto.startPosition.clone();
        this.destination = proto.destination.clone();
        this.stayDuration = proto.stayDuration;
        this.nextJumpTime = proto.nextJumpTime;
        this.teleportDelay = proto.teleportDelay;

        // Generate a new unique position for each node instance
        this.startPosition = randomCoord();
        this.destination = randomCoordDifferentFrom(startPosition);
    }

    /**
     * Returns a random coordinate within world boundaries using MovementModel.rng.
     */
    private Coord randomCoord() {
        return new Coord(MovementModel.rng.nextDouble() * maxX,
                         MovementModel.rng.nextDouble() * maxY);
    }

    /**
     * Returns a random coordinate different from the given one.
     */
    private Coord randomCoordDifferentFrom(Coord reference) {
        Coord newCoord;
        do {
            newCoord = randomCoord();
        } while (newCoord.equals(reference));
        return newCoord;
    }
}
