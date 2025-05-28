package ddosim5g.simcommands;

simple PostSimulationCommand {
    parameters:
        //string vecFilePath; // Declare a parameter to hold the .vec file path
        @class(PostSimulationCommand);  // Ensures the NED file references the correct C++ class
}