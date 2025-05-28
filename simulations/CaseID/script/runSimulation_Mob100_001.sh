#!/bin/bash

# Function to print the usage of the script
print_usage() {
    echo "Please select a simulation time:"
    echo "1) 60 seconds (1 minute)"
    echo "2) 300 seconds (5 minutes)"
    echo "3) 600 seconds (10 minutes)"
    echo "4) 3600 seconds (1 hour)"
    echo "5) 7200 seconds (2 hours)"
}

# Prompt the user for input
print_usage
read -p "Enter the number corresponding to the desired simulation time: " selection

# Set the simulation time based on user input
case $selection in
    1)
        SIMTIME="60s"
        ;;
    2)
        SIMTIME="300s"
        ;;
    3)
        SIMTIME="600s"
        ;;
    4)
        SIMTIME="3600s"
        ;;
    5)
        SIMTIME="7200s"
        ;;
    *)
        echo "Invalid selection."
        exit 1
        ;;
esac

echo "Running simulation with simtime = $SIMTIME"

# Create the simRun_logs directory if it doesn't exist
LOG_DIR="simRun_logs"
if [ ! -d "$LOG_DIR" ]; then
    mkdir "$LOG_DIR"
fi

# Get the current date and time to append to the log file name
DATE_TIME=$(date +"%Y%m%d_%H%M%S")

# Set the log file name with date and time suffix
LOG_FILE="$LOG_DIR/simulation_${SIMTIME}_${DATE_TIME}.log"

export PROJECT_ROOT_DIR="../../../"

# Start the timer
START_TIME=$(date +%s)

# Run the simulation command
#opp_run -r 0 -r "\$simtime=$SIMTIME" -c TC-Base-Mob100-001 -n ../../../networks/:../../../src/:../../../../inet4.5/src:../../../../Simu5G-1.2.2/src -l ../../../../inet4.5/src/libINET.so -l ../../../../Simu5G-1.2.2/src/libsimu5g.so -l ../../../src/libHelloWorldSim.so -u Cmdenv -f ../TC-Base-Mob100-001.ini 

opp_run -r 0 -r "\$simtime=$SIMTIME" -c TC-Base-Mob100-001 -n ../../../networks/:../../../src/:../../../../inet4.5/src:../../../../Simu5G-1.2.2/src -l ../../../../inet4.5/src/libINET.so -l ../../../../Simu5G-1.2.2/src/libsimu5g.so -l ../../../src/libHelloWorldSim.so -u Cmdenv -f ../TC-Base-Mob100-001.ini --cmdenv-express-mode=true --cmdenv-log-level=debug > "$LOG_FILE" 2>&1


# End the timer
END_TIME=$(date +%s)

# Calculate and print the elapsed time
ELAPSED_TIME=$((END_TIME - START_TIME))
#echo "Simulation completed in $ELAPSED_TIME seconds."
echo "Simulation completed in $ELAPSED_TIME seconds. Log file saved to $LOG_FILE."

