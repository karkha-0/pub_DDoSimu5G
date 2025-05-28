#!/bin/bash

## Note:
# To run this script you need to change the mobility and the infections paths in the ini and the ned file

# Ask user for DDoS flag (0 = disabled, 1 = enabled)
read -p "Enable DDoS traffic? Enter 0 (false) or 1 (true): " DDOS_FLAG

# Validate input
if [[ "$DDOS_FLAG" != "0" && "$DDOS_FLAG" != "1" ]]; then
    echo "Invalid input. Please enter 0 or 1."
    exit 1
fi

echo "Select a mobility configuration:"
echo "1) Stationary"
echo "2) SlowMoving"
echo "3) MixedMoving"
read -p "Enter choice [1-3]: " CONFIG_CHOICE

case $CONFIG_CHOICE in
  1) CONFIG_NAME="TC-frag-DDoS-infec-5RAN-002-stationary" ;;
  2) CONFIG_NAME="TC-frag-DDoS-infec-5RAN-002-slowMoving" ;;
  3) CONFIG_NAME="TC-frag-DDoS-infec-5RAN-002-MixedMoving" ;;
  *) echo "Invalid choice."; exit 1 ;;
esac

# Create the simRun_logs directory if it doesn't exist
LOG_DIR="simRun_logs"
if [ ! -d "$LOG_DIR" ]; then
    mkdir "$LOG_DIR"
fi

# Get the current date and time to append to the log file name
DATE_TIME=$(date +"%Y%m%d_%H%M%S")

# Set the log file name with date and time suffix
#LOG_FILE="$LOG_DIR/TC-Base-DDoS-infec-5RAN-002-simulation_DDOS_FLAG_${DDOS_FLAG}_${SIMTIME}_${DATE_TIME}.log"
LOG_FILE="$LOG_DIR/${CONFIG_NAME}_simulation_DDOS_FLAG_${DDOS_FLAG}_${SIMTIME}_${DATE_TIME}.log"


export PROJECT_ROOT_DIR="../../.."
samples_dir="../../../../"


# Start the timer
START_TIME=$(date +%s)

#opp_run -r 0 -c TC-Base-DDoS-infec-5RAN-002 -n $PROJECT_ROOT_DIR/simulations/CaseID/networks/:$PROJECT_ROOT_DIR/src/:$samples_dir/inet4.5/src:$samples_dir/Simu5G-1.2.2/src -l $samples_dir/inet4.5/src/libINET.so -l $samples_dir/Simu5G-1.2.2/src/libsimu5g.so -l $PROJECT_ROOT_DIR/src/libDDoSim5G.so -u Cmdenv -f  ../Test-cases-001/TC-Base-DDoS-infec-5RAN-002.ini -s --cmdenv-redirect-output=true > "$LOG_FILE" 2>&1

# Run simulation with selected repetition and optional parameter override
  #-c TC-Base-DDoS-infec-5RAN-002 \
  
opp_run -r $DDOS_FLAG \
  -c $CONFIG_NAME \
  -n $PROJECT_ROOT_DIR/simulations/CaseID/networks/:$PROJECT_ROOT_DIR/src/:$samples_dir/inet4.5/src:$samples_dir/Simu5G-1.2.2/src \
  -l $samples_dir/inet4.5/src/libINET.so \
  -l $samples_dir/Simu5G-1.2.2/src/libsimu5g.so \
  -l $PROJECT_ROOT_DIR/src/libDDoSim5G.so \
  -u Cmdenv \
  -f ../Test-cases-001/TC-frag-DDoS-infec-5RAN-002.ini \
  -s \
  --cmdenv-redirect-output=true \
  > "$LOG_FILE" 2>&1


# End the timer
END_TIME=$(date +%s)

# Calculate and print the elapsed time
ELAPSED_TIME=$((END_TIME - START_TIME))
#echo "Simulation completed in $ELAPSED_TIME seconds."
echo "Simulation completed in $ELAPSED_TIME seconds. Log file saved to $LOG_FILE."
