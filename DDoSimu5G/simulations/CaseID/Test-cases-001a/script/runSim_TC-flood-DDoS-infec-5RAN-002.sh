#!/bin/bash

## Note:
# To run this script you need to change the mobility and the infections paths in the ini and the ned file

# Source OMNeT++ environment - dynamically find OMNeT++ installation
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Method 1: Check if already in environment (OMNET_ROOT set)
if [ -n "$OMNET_ROOT" ] && [ -f "$OMNET_ROOT/setenv" ]; then
    OMNET_DIR="$OMNET_ROOT"
# Method 2: Search up directory tree for omnetpp installation
else
    OMNET_DIR=""
    search_dir="$SCRIPT_DIR"
    for i in {1..10}; do
        if [ -f "$search_dir/setenv" ] && [ -f "$search_dir/bin/omnetpp" ]; then
            OMNET_DIR="$search_dir"
            break
        fi
        search_dir="$(dirname "$search_dir")"
    done
fi

if [ -n "$OMNET_DIR" ] && [ -f "$OMNET_DIR/setenv" ]; then
    echo "Sourcing OMNeT++ environment from $OMNET_DIR/setenv"
    source "$OMNET_DIR/setenv" -q
else
    echo "ERROR: Cannot find OMNeT++ installation"
    echo "Searched from: $SCRIPT_DIR"
    echo "Please either:"
    echo "  1. Run from installed project: <install-dir>/omnetpp-6.0.1/samples/DDoSimu5G/simulations/..."
    echo "  2. Set OMNET_ROOT environment variable: export OMNET_ROOT=/path/to/omnetpp-6.0.1"
    exit 1
fi

# Find and source INET setenv
INET_DIR="$OMNET_DIR/samples/inet4.5"
if [ -f "$INET_DIR/setenv" ]; then
    echo "Sourcing INET environment from $INET_DIR/setenv"
    source "$INET_DIR/setenv" -q
else
    echo "ERROR: Cannot find INET at $INET_DIR"
    exit 1
fi

# Find and source Simu5G setenv
SIMU5G_DIR="$OMNET_DIR/samples/Simu5G"
if [ -f "$SIMU5G_DIR/setenv" ]; then
    echo "Sourcing Simu5G environment from $SIMU5G_DIR/setenv"
    cd "$SIMU5G_DIR" && source ./setenv -f && cd - > /dev/null
else
    echo "ERROR: Cannot find Simu5G at $SIMU5G_DIR"
    exit 1
fi

# Find and source DDoSimu5G setenv
DDOSIMU5G_DIR="$OMNET_DIR/samples/DDoSimu5G"
if [ -f "$DDOSIMU5G_DIR/setenv" ]; then
    echo "Sourcing DDoSimu5G environment from $DDOSIMU5G_DIR/setenv"
    source "$DDOSIMU5G_DIR/setenv" -q
else
    echo "ERROR: Cannot find DDoSimu5G at $DDOSIMU5G_DIR"
    exit 1
fi

# Now INET_ROOT, SIMU5G_ROOT, and DDOSIMU5G_ROOT are set - use absolute paths
export LD_LIBRARY_PATH="$INET_ROOT/src:$SIMU5G_ROOT/src:$DDOSIMU5G_ROOT/src:$LD_LIBRARY_PATH"

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
echo "4) MixedMoving with 400 extra stationary benign CbrUEs"
echo "5) MixedMoving with 400 extra moving benign CbrUEs"
echo "6) MixedMoving with 50 extra stationary benign VidUes"
read -p "Enter choice [1-6]: " CONFIG_CHOICE

case $CONFIG_CHOICE in
  1) CONFIG_NAME="TC-flood-DDoS-infec-5RAN-002-stationary" ;;
  2) CONFIG_NAME="TC-flood-DDoS-infec-5RAN-002-slowMoving" ;;
  3) CONFIG_NAME="TC-flood-DDoS-infec-5RAN-002-MixedMoving" ;;
  4) CONFIG_NAME="TC-flood-DDoS-infec-500cbrUEsSt-5RAN-002-MixedMoving" ;;
  5) CONFIG_NAME="TC-flood-DDoS-infec-500cbrUEsMv-5RAN-002-MixedMoving" ;;
  6) CONFIG_NAME="TC-flood-DDoS-infec-100cbrUEsMv-50VidUEsSt-5RAN-002-MixedMoving" ;;
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

#opp_run -r 0 -c TC-Base-DDoS-infec-5RAN-002 -n $PROJECT_ROOT_DIR/simulations/CaseID/networks/:$PROJECT_ROOT_DIR/src/:$samples_dir/inet4.5/src:$samples_dir/Simu5G/src -l $samples_dir/inet4.5/src/libINET.so -l $samples_dir/Simu5G/src/libsimu5g.so -l $PROJECT_ROOT_DIR/src/libDDoSimu5G.so -u Cmdenv -f  ../Test-cases-001/TC-Base-DDoS-infec-5RAN-002.ini -s --cmdenv-redirect-output=true > "$LOG_FILE" 2>&1

# Run simulation with selected repetition and optional parameter override
  #-c TC-Base-DDoS-infec-5RAN-002 \
  
opp_run -r $DDOS_FLAG \
  -c $CONFIG_NAME \
  -n $PROJECT_ROOT_DIR/simulations/CaseID/networks/:$PROJECT_ROOT_DIR/src/:$samples_dir/inet4.5/src:$samples_dir/Simu5G/src \
  -l $samples_dir/inet4.5/src/libINET.so \
  -l $samples_dir/Simu5G/src/libsimu5g.so \
  -l $PROJECT_ROOT_DIR/src/libDDoSimu5G.so \
  -u Cmdenv \
  -f ../Test-cases-001/TC-flood-DDoS-infec-5RAN-002.ini \
  -s \
  --cmdenv-redirect-output=true \
  > "$LOG_FILE" 2>&1


# End the timer
END_TIME=$(date +%s)

# Calculate and print the elapsed time
ELAPSED_TIME=$((END_TIME - START_TIME))
#echo "Simulation completed in $ELAPSED_TIME seconds."
echo "Simulation completed in $ELAPSED_TIME seconds. Log file saved to $LOG_FILE."
