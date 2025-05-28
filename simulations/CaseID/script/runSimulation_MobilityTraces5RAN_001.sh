#!/bin/bash

## Note:
# To run this script you need to change the mobility and the infections paths in the ini and the ned file

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

opp_run -r 40 -c TC-Base-MobTraces-Base5RAN-001 -n ../../../networks/:../../../src/:../../../../inet4.5/src:../../../../Simu5G-1.2.2/src -l ../../../../inet4.5/src/libINET.so -l ../../../../Simu5G-1.2.2/src/libsimu5g.so -l ../../../src/libHelloWorldSim.so -u Cmdenv -f  ../TC-MobilityTraces-Base5RAN-001.ini -s --cmdenv-redirect-output=true > "$LOG_FILE" 2>&1


# End the timer
END_TIME=$(date +%s)

# Calculate and print the elapsed time
ELAPSED_TIME=$((END_TIME - START_TIME))
#echo "Simulation completed in $ELAPSED_TIME seconds."
echo "Simulation completed in $ELAPSED_TIME seconds. Log file saved to $LOG_FILE."
