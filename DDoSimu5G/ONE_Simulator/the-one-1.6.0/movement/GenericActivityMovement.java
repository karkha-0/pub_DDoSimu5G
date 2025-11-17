/* 
 * Copyright 2024, Lund Univeristy, EIT, Security and Networks group
 * Released under GPLv3. See LICENSE.txt for details. 
 */

package movement;

import movement.map.DijkstraPathFinder;
import movement.map.MapNode;
import movement.map.SimMap;
import core.Coord;
import core.Settings;
import core.SimClock;

import java.io.File;
import java.util.*;
import input.WKTReader;


/**
 * This class represents the mobility model where nodes select a random destination from
 * a set of stops and return to their home location after the trip.
 * Trip could be one or multiple destinations
 */
public class GenericActivityMovement extends MapBasedMovement {
    private int numStops; // Number of random stops available
    private boolean roundTrip; // Indicates whether nodes return to home after the trip
    private double minMoveDelay, maxMoveDelay, minWaitTime, maxWaitTime;
    private Coord startLocation; // Initial location of the node
    private List<Coord> stopLocations; // Available stop locations
    private DijkstraPathFinder pathFinder;
    private double moveDelay;
    private Coord lastWaypoint;
    private Trip trip;

    /**
     * Constructor initializes movement settings and assigns a random stop to each node.
     * @param settings Mobility settings for the simulation.
     */
    public GenericActivityMovement(Settings settings) {
        super(settings);
        this.pathFinder = new DijkstraPathFinder(null);

        Settings mobilitySettings = new Settings("GenericActivityMovement");
        this.numStops = mobilitySettings.getInt("numStops");
        this.roundTrip = mobilitySettings.getBoolean("roundTrip");
        this.minMoveDelay = mobilitySettings.getDouble("minMoveDelay");
        this.maxMoveDelay = mobilitySettings.getDouble("maxMoveDelay");
        this.minWaitTime = mobilitySettings.getDouble("minWaitTime");
        this.maxWaitTime = mobilitySettings.getDouble("maxWaitTime");
        //this.started = false;
        this.moveDelay = minMoveDelay + (maxMoveDelay - minMoveDelay) * rng.nextDouble();

        SimMap map = getMap();
        startLocation = intiateStartLocation(map);
        this.lastWaypoint = startLocation.clone();
        stopLocations = getStopLocations(mobilitySettings, map);
        this.trip = new Trip(startLocation, stopLocations, minWaitTime, maxWaitTime, roundTrip);
    }

    /**
     * Selects a random starting location from the map nodes.
     * @param map The simulation map.
     * @return A random coordinate on the map.
     */
    private Coord intiateStartLocation(SimMap map) {
        MapNode[] nodes = map.getNodes().toArray(new MapNode[0]);
        return nodes[rng.nextInt(nodes.length)].getLocation().clone();
    }

    /**
     * Retrieves a list of stop locations from configuration or generates random stops.
     * @param settings Mobility settings.
     * @param map The simulation map.
     * @return List of stop locations.
     */
    private List<Coord> getStopLocations(Settings settings, SimMap map) {
        List<Coord> stops = new ArrayList<>();
        String stopsFile = settings.getSetting("stopsFile", "");
        if (!stopsFile.isEmpty()) {
            try {
                stops = new WKTReader().readPoints(new File(stopsFile));
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        if (stops.isEmpty()) {
            MapNode[] nodes = map.getNodes().toArray(new MapNode[0]);
            while (stops.size() < numStops) {
                Coord loc = nodes[rng.nextInt(nodes.length)].getLocation().clone();
                if (!stops.contains(loc)) stops.add(loc);
            }
        }
        return stops;
    }

    @Override
    public Coord getInitialLocation() {
        return startLocation.clone();
    }

    @Override
    public Path getPath() {
        if (SimClock.getTime() < this.moveDelay) {
            return null;
        }

        if (!trip.isStopCompleted()) {
            return null;
        }
        
        SimMap map = getMap();
        Path path = new Path(generateSpeed());
        
        if (!trip.hasMoreStops()) {
            return null;
        }

        Coord destination;
        if (roundTrip && !trip.isReturningToStart()) {
            trip.setReturningToStart(true);
            destination = trip.getNextStop();
        } else if (roundTrip && trip.isReturningToStart()) {
            trip.setReturningToStart(false);
            destination = startLocation;
        } else {
            destination = trip.getNextStop();
        }      

        MapNode startNode = map.getNodeByCoord(getLastLocation());
        MapNode endNode = map.getNodeByCoord(destination);
        List<MapNode> shortestPath = pathFinder.getShortestPath(startNode, endNode);
        for (MapNode node : shortestPath) {
            path.addWaypoint(node.getLocation());
        }
        
        // Now update lastWaypoint AFTER movement calculation
        this.lastWaypoint = destination.clone();

        trip.setWaitTime(SimClock.getTime());

        return path;
    }

    @Override
    public boolean isReady() {
        return trip.isStopCompleted();
    }

    @Override
    public MapBasedMovement replicate() {
        return new GenericActivityMovement(this);
    }

    @Override
    public void setLocation(Coord lastWaypoint) {
        this.lastWaypoint = lastWaypoint.clone();
    }

    @Override
    public Coord getLastLocation() {
        return lastWaypoint.clone();
    }

    private GenericActivityMovement(GenericActivityMovement proto) {
        super(proto);
        this.numStops = proto.numStops;
        this.roundTrip = proto.roundTrip;
        this.minMoveDelay = proto.minMoveDelay;
        this.maxMoveDelay = proto.maxMoveDelay;
        this.minWaitTime = proto.minWaitTime;
        this.maxWaitTime = proto.maxWaitTime;
        this.startLocation = proto.intiateStartLocation(proto.getMap());
        this.stopLocations = new ArrayList<>(proto.stopLocations);
        this.pathFinder = proto.pathFinder;
        this.moveDelay = proto.minMoveDelay + (proto.maxMoveDelay - proto.minMoveDelay) * rng.nextDouble();

        this.lastWaypoint = this.startLocation.clone();
        this.trip = new Trip(proto.startLocation, proto.stopLocations, proto.minWaitTime, proto.maxWaitTime, proto.roundTrip);
    }
}

/**
 * Represents the trip for each node, including selecting a random stop and returning home.
 */
class Trip {
    private boolean roundTrip;
    private double waitTime;
    private double minWaitTime;
    private double maxWaitTime;
    private boolean returningToStart;
    private Coord startLocation;
    private boolean tripsCompleted;
    private Coord chosenStop; 

    public Trip(Coord startLocation, List<Coord> stopLocations, double minWaitTime, double maxWaitTime, boolean roundTrip) {
        this.startLocation = startLocation;
        this.minWaitTime = minWaitTime;
        this.maxWaitTime = maxWaitTime;
        this.roundTrip = roundTrip;
        this.returningToStart = false;
        this.tripsCompleted = false;

        if (!stopLocations.isEmpty()) {
            Random rand = new Random();
            this.chosenStop = stopLocations.get(rand.nextInt(stopLocations.size())); // Pick one random stop
        }        
    }
    

    public boolean isStopCompleted() {
        return SimClock.getTime() >= waitTime;
    }

    public boolean hasMoreStops() {
        
        return !this.tripsCompleted; // Only allows one trip before returning home
    }
    
    public Coord getNextStop() {
        return chosenStop;
    }

    public void setWaitTime(double currentTime) {
        this.waitTime = currentTime + minWaitTime + (maxWaitTime - minWaitTime) * Math.random();
    }

    public boolean isReturningToStart() {
        //this.tripsCompleted = true;
        return returningToStart;
    }
    
    public void setReturningToStart(boolean returning) {
        this.returningToStart = returning;
        this.tripsCompleted = !returning;
    }
}
