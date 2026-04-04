#!/bin/bash
##############################################################################
# TC-002 Dynamic Traffic - Interactive Configuration Runner
# 
# This script presents an interactive menu to run test configurations
# from TC-002-dynamic-traffic.ini
# Skips: [General] and [Config DDoS-General-Settings] (base configs only)
#
# Usage:
#   ./run_all_configs.sh              # Interactive menu
##############################################################################

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
# Explicitly set DDOSIMU5G_ROOT — BASH_SOURCE resolution may fail in Docker/non-interactive shells
export DDOSIMU5G_ROOT="$DDOSIMU5G_DIR"
export PATH="$OMNET_DIR/bin:$DDOSIMU5G_DIR/bin:$PATH"

# Now INET_ROOT, SIMU5G_ROOT, and DDOSIMU5G_ROOT are set - use absolute paths
export LD_LIBRARY_PATH="$INET_ROOT/src:$SIMU5G_ROOT/src:$DDOSIMU5G_ROOT/src:${LD_LIBRARY_PATH:-}"
export PROJECT_ROOT_DIR="$DDOSIMU5G_ROOT"

# Configuration
INI_FILE="../scenarios/TC-002-dynamic-traffic.ini"

# Define all available configurations
# Format: "config_name|description|category"
declare -a BENIGN_CONFIGS=(
    "Dynamic-Simple-Test|Simple test with ONE Benign traffic profile only (smart meter)|benign"
    "All-Profiles-Test|Test with ALL 6 Benign traffic profiles|benign"
    "Stress-Test|Stress test with 30 UEs cycling through all 6 Benign profiles|benign"
)

declare -a ATTACK_CONFIGS=(
    "DDoS-Single-UE-UDP-Intense|Test 1: Single UE with UDP Flood - Intense style|attack"
    "DDoS-Attack-UDP-Style-Comparison|Test 2: 3 UEs comparing UDP-Flooding attack styles|attack"
    "DDoS-Attack-DNS-Style-Comparison|Test 3: 3 UEs comparing DNS-Amplification attack styles|attack"
    "DDoS-Attack-TCP-Style-Comparison|Test 4: 3 UEs comparing TCP SYN Flood attack styles|attack"
    "DDoS-Attack-HTTP-Style-Comparison|Test 5: 3 UEs comparing HTTP Flood attack styles|attack"
    "DDoS-Multi-Attack-Multi-Behavior|Test 6: 12 UEs testing all attack types with different behavior modes|attack"
)

# Combine all configurations with numbering
declare -a ALL_CONFIGS=()
ALL_CONFIGS+=("${BENIGN_CONFIGS[@]}")
ALL_CONFIGS+=("${ATTACK_CONFIGS[@]}")

# Show interactive menu
clear
echo "=========================================="
echo "TC-002 Dynamic Traffic - Configuration Menu"
echo "=========================================="
echo "INI File: $INI_FILE"
echo
echo "BENIGN TRAFFIC TESTS:"
echo "----------------------------------------"
for i in "${!BENIGN_CONFIGS[@]}"; do
    config="${BENIGN_CONFIGS[$i]}"
    IFS='|' read -r name desc category <<< "$config"
    printf "  [%2d] %s\n" "$((i + 1))" "$desc"
done

echo
echo "ATTACK TESTS:"
echo "----------------------------------------"
ATTACK_START=$((${#BENIGN_CONFIGS[@]} + 1))
for i in "${!ATTACK_CONFIGS[@]}"; do
    config="${ATTACK_CONFIGS[$i]}"
    IFS='|' read -r name desc category <<< "$config"
    printf "  [%2d] %s\n" "$((ATTACK_START + i))" "$desc"
done

echo
echo "SPECIAL OPTIONS:"
echo "----------------------------------------"
printf "  [%2d] Run ALL benign tests (1-%d)\n" "$((${#ALL_CONFIGS[@]} + 1))" "${#BENIGN_CONFIGS[@]}"
printf "  [%2d] Run ALL attack tests (%d-%d)\n" "$((${#ALL_CONFIGS[@]} + 2))" "$ATTACK_START" "${#ALL_CONFIGS[@]}"
printf "  [%2d] Run EVERYTHING\n" "$((${#ALL_CONFIGS[@]} + 3))"
printf "  [ 0] Exit\n"
echo "=========================================="
echo

# Get user input
read -p "Enter your choice(s) (space-separated for multiple, e.g., '1 3 5'): " choices

# Parse user input
declare -a SELECTED_INDICES=()

# Check for exit
if [[ "$choices" == "0" ]]; then
    echo "Exiting..."
    exit 0
fi

# Check for special options
TOTAL_CONFIGS=${#ALL_CONFIGS[@]}
RUN_ALL_BENIGN=$((TOTAL_CONFIGS + 1))
RUN_ALL_ATTACK=$((TOTAL_CONFIGS + 2))
RUN_EVERYTHING=$((TOTAL_CONFIGS + 3))

for choice in $choices; do
    if [[ "$choice" == "$RUN_ALL_BENIGN" ]]; then
        # Add all benign indices
        for ((i=0; i<${#BENIGN_CONFIGS[@]}; i++)); do
            SELECTED_INDICES+=($i)
        done
    elif [[ "$choice" == "$RUN_ALL_ATTACK" ]]; then
        # Add all attack indices
        for ((i=${#BENIGN_CONFIGS[@]}; i<${#ALL_CONFIGS[@]}; i++)); do
            SELECTED_INDICES+=($i)
        done
    elif [[ "$choice" == "$RUN_EVERYTHING" ]]; then
        # Add all indices
        for ((i=0; i<${#ALL_CONFIGS[@]}; i++)); do
            SELECTED_INDICES+=($i)
        done
    elif [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "$TOTAL_CONFIGS" ]; then
        # Valid individual choice
        SELECTED_INDICES+=($((choice - 1)))
    else
        echo "WARNING: Invalid choice '$choice' - skipping"
    fi
done

# Remove duplicates and sort
SELECTED_INDICES=($(echo "${SELECTED_INDICES[@]}" | tr ' ' '\n' | sort -nu | tr '\n' ' '))

if [ ${#SELECTED_INDICES[@]} -eq 0 ]; then
    echo "ERROR: No valid configurations selected!"
    exit 1
fi

# Build list of configs to run
declare -a CONFIGS_TO_RUN=()
for idx in "${SELECTED_INDICES[@]}"; do
    CONFIGS_TO_RUN+=("${ALL_CONFIGS[$idx]}")
done

# Create directories
LOG_DIR="../run_results/logs"
mkdir -p "$LOG_DIR"

# Collect system info once for perf.json (constant across all runs)
CPU_CORES=$(nproc 2>/dev/null || echo "1")
CPU_CORES_TOTAL=$(nproc --all 2>/dev/null || echo "$CPU_CORES")
CPU_MODEL=$(grep -m1 "model name" /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs 2>/dev/null || echo "unknown")

# Print header
echo
echo "=========================================="
echo "Selected Configurations:"
echo "=========================================="
for i in "${!CONFIGS_TO_RUN[@]}"; do
    config="${CONFIGS_TO_RUN[$i]}"
    IFS='|' read -r name desc category <<< "$config"
    printf "  %d. %s\n" "$((i + 1))" "$desc"
done
echo "=========================================="
echo
read -p "Press Enter to start, or Ctrl+C to cancel..."
echo

# Start overall timer
OVERALL_START_TIME=$(date +%s)

# Run each configuration
SUCCESS_COUNT=0
FAILED_COUNT=0
declare -a FAILED_CONFIGS=()

for i in "${!CONFIGS_TO_RUN[@]}"; do
    config="${CONFIGS_TO_RUN[$i]}"
    IFS='|' read -r CONFIG_NAME DESCRIPTION CATEGORY <<< "$config"
    
    CONFIG_NUM=$((i + 1))
    TOTAL_CONFIGS=${#CONFIGS_TO_RUN[@]}
    
    echo
    echo "=========================================="
    echo "Running [$CONFIG_NUM/$TOTAL_CONFIGS]: $CONFIG_NAME"
    echo "Description: $DESCRIPTION"
    echo "Category: $CATEGORY"
    echo "=========================================="
    
    # Create log file
    DATE_TIME=$(date +"%Y%m%d_%H%M%S")
    LOG_FILE="$LOG_DIR/${CONFIG_NAME}_${DATE_TIME}.log"
    
    echo "Log file: $LOG_FILE"
    echo
    
    # Start timer for this config
    START_TIME=$(date +%s)

    # Temp files for performance measurement
    TIME_OUTPUT_FILE=$(mktemp /tmp/ddosimu5g_time_XXXXXX)
    RUN_MARKER_FILE=$(mktemp /tmp/ddosimu5g_marker_XXXXXX)

    # Run simulation wrapped with /usr/bin/time to capture peak RSS and CPU utilization
    # /usr/bin/time -f format: %M=peak RSS (KB), %P=avg CPU utilization (%)
    /usr/bin/time -f "%M %P" -o "$TIME_OUTPUT_FILE" \
    opp_run -r 0 \
      -c "$CONFIG_NAME" \
      -n "$DDOSIMU5G_ROOT/simulations/CaseID/Test-cases-002a/:$DDOSIMU5G_ROOT/simulations/CaseID/Test-cases-002a/scenarios/ned/:$DDOSIMU5G_ROOT/src/:$INET_ROOT/src:$SIMU5G_ROOT/src" \
      -l "$INET_ROOT/src/libINET.so" \
      -l "$SIMU5G_ROOT/src/libsimu5g.so" \
      -l "$DDOSIMU5G_ROOT/src/libDDoSimu5G.so" \
      --image-path="$INET_ROOT/images:$SIMU5G_ROOT/images" \
      -u Cmdenv \
      -f "$INI_FILE" \
      --result-dir="$LOG_DIR" \
      --cmdenv-redirect-output=true \
      > "$LOG_FILE" 2>&1
    
    EXIT_CODE=$?
    
    # End timer for this config
    END_TIME=$(date +%s)
    ELAPSED_TIME=$((END_TIME - START_TIME))
    
    # Check if simulation succeeded
    if [ $EXIT_CODE -eq 0 ]; then
        echo "✓ SUCCESS - Completed in $ELAPSED_TIME seconds"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        echo "Log saved to: $LOG_FILE"

        # --- Collect performance stats for perf.json ---

        # Parse /usr/bin/time output: "%M %P" -> "peak_rss_kb cpu_pct%"
        TIME_STATS=$(cat "$TIME_OUTPUT_FILE" 2>/dev/null || echo "0 0%")
        PEAK_RSS_KB=$(echo "$TIME_STATS" | awk '{print $1+0}')
        CPU_PERCENT_RAW=$(echo "$TIME_STATS" | awk '{gsub(/%/,"",$2); print $2+0}')
        PEAK_RSS_MB=$(LC_NUMERIC=C awk "BEGIN {printf \"%.1f\", $PEAK_RSS_KB / 1024}")
        CPU_CORES_USED=$(LC_NUMERIC=C awk "BEGIN {printf \"%.2f\", $CPU_PERCENT_RAW / 100}")

        # Parse OMNeT++ simulation stats
        # OMNeT++ writes cmdenv output to --result-dir/<config>-#0.out
        OMNET_OUT_FILE="$LOG_DIR/${CONFIG_NAME}-#0.out"
        [ ! -f "$OMNET_OUT_FILE" ] && OMNET_OUT_FILE="$LOG_DIR/${CONFIG_NAME}-0.out"
        STATS_FILE="$LOG_FILE"
        if [ -f "$OMNET_OUT_FILE" ] && grep -q "Simulation time limit\|End simulation\|simulation stopped" "$OMNET_OUT_FILE" 2>/dev/null; then
            STATS_FILE="$OMNET_OUT_FILE"
        fi

        # OMNeT++ end line: "<!> Simulation time limit reached -- at t=600s, event #54648028"
        END_SIM_LINE=$(grep -iE "simulation time limit reached|end simulation|simulation stopped" "$STATS_FILE" 2>/dev/null | tail -1)
        EVENTS_TOTAL=$(echo "$END_SIM_LINE" | grep -oP 'event #\K[0-9]+' 2>/dev/null)
        SIM_DURATION_ACTUAL=$(echo "$END_SIM_LINE" | grep -oP '\bt=\K[0-9.]+' 2>/dev/null | tr -d 's')
        [ -z "$EVENTS_TOTAL" ] && EVENTS_TOTAL="0"
        [ -z "$SIM_DURATION_ACTUAL" ] && SIM_DURATION_ACTUAL="600"

        # Compute averages from totals (wall_time is ground truth, avoids per-sample noise)
        SIM_SPEED=$(LC_NUMERIC=C awk "BEGIN {printf \"%.4f\", $SIM_DURATION_ACTUAL / ($ELAPSED_TIME > 0 ? $ELAPSED_TIME : 1)}")
        EVENTS_PER_SEC=$(LC_NUMERIC=C awk "BEGIN {printf \"%d\", $EVENTS_TOTAL / ($ELAPSED_TIME > 0 ? $ELAPSED_TIME : 1)}")

        # Find PCAP run directory created during this simulation run
        PCAP_BASE_DIR="../run_results/pcaps/$CONFIG_NAME"
        PCAP_RUN_DIR=""
        if [ -d "$PCAP_BASE_DIR" ]; then
            PCAP_RUN_DIR=$(find "$PCAP_BASE_DIR" -maxdepth 1 -mindepth 1 -type d \
                           -newer "$RUN_MARKER_FILE" 2>/dev/null | sort | tail -1)
            # Fallback: newest existing subdirectory
            [ -z "$PCAP_RUN_DIR" ] && \
                PCAP_RUN_DIR=$(find "$PCAP_BASE_DIR" -maxdepth 1 -mindepth 1 -type d \
                               2>/dev/null | sort | tail -1)
        fi

        # Collect storage stats
        PCAP_SIZE_MB="0"; LABEL_FILES="0"; LABEL_ENTRIES="0"; LABEL_SIZE_MB="0"
        if [ -n "$PCAP_RUN_DIR" ] && [ -d "$PCAP_RUN_DIR" ]; then
            _PCAP_KB=$(find "$PCAP_RUN_DIR" -maxdepth 1 -name '*.pcap' \
                       -exec du -sk {} + 2>/dev/null | awk '{s+=$1} END {print s+0}')
            PCAP_SIZE_MB=$(LC_NUMERIC=C awk "BEGIN {printf \"%.1f\", ${_PCAP_KB:-0} / 1024}")
            if [ -d "$PCAP_RUN_DIR/labels" ]; then
                LABEL_FILES=$(find "$PCAP_RUN_DIR/labels" -name '*.csv' 2>/dev/null \
                              | wc -l | tr -d ' ')
                # Count total labeled packet entries (data rows, one header per CSV excluded)
                LABEL_ENTRIES=$(find "$PCAP_RUN_DIR/labels" -name '*.csv' 2>/dev/null \
                    | xargs -r awk 'FNR==1{next}{n++}END{print n+0}' 2>/dev/null || echo 0)
                _LABEL_KB=$(du -sk "$PCAP_RUN_DIR/labels" 2>/dev/null | awk '{print $1+0}')
                LABEL_SIZE_MB=$(LC_NUMERIC=C awk "BEGIN {printf \"%.1f\", ${_LABEL_KB:-0} / 1024}")
            fi
        fi

        # Write perf.json into the timestamped PCAP run directory
        RUN_TIMESTAMP=$(basename "${PCAP_RUN_DIR:-}" 2>/dev/null || echo "$DATE_TIME")
        PERF_JSON_DIR="${PCAP_RUN_DIR:-$LOG_DIR/$CONFIG_NAME}"
        mkdir -p "$PERF_JSON_DIR"
        PERF_JSON_FILE="$PERF_JSON_DIR/perf.json"
        CPU_MODEL_ESC=$(echo "$CPU_MODEL" | sed 's/\\/\\\\/g; s/"/\\"/g')
        DESC_ESC=$(echo "$DESCRIPTION" | sed 's/\\/\\\\/g; s/"/\\"/g')

        cat > "$PERF_JSON_FILE" << PERF_EOF
{
  "scenario": "$CONFIG_NAME",
  "description": "$DESC_ESC",
  "category": "$CATEGORY",
  "run_timestamp": "$RUN_TIMESTAMP",
  "sim_duration_s": $SIM_DURATION_ACTUAL,
  "wall_time_s": $ELAPSED_TIME,
  "sim_speed_ratio": $SIM_SPEED,
  "events_processed": $EVENTS_TOTAL,
  "events_per_sec": $EVENTS_PER_SEC,
  "cpu": {
    "model": "$CPU_MODEL_ESC",
    "cores_available": $CPU_CORES,
    "cores_total": $CPU_CORES_TOTAL,
    "avg_utilization_pct": $CPU_PERCENT_RAW,
    "avg_cores_used": $CPU_CORES_USED
  },
  "memory": {
    "peak_rss_mb": $PEAK_RSS_MB
  },
  "storage": {
    "pcap_size_mb": $PCAP_SIZE_MB,
    "label_files": $LABEL_FILES,
    "label_entries": $LABEL_ENTRIES,
    "label_size_mb": $LABEL_SIZE_MB
  }
}
PERF_EOF

        echo "  perf.json: $PERF_JSON_FILE"
    else
        # Rename log file to indicate error
        ERROR_LOG_FILE="${LOG_FILE%.log}_ERROR.log"
        mv "$LOG_FILE" "$ERROR_LOG_FILE"
        
        echo "✗ FAILED - Exit code: $EXIT_CODE (after $ELAPSED_TIME seconds)"
        echo "ERROR log saved to: $ERROR_LOG_FILE"
        
        # Show last 20 lines of error log
        echo
        echo "--- Last 20 lines of error log ---"
        tail -n 20 "$ERROR_LOG_FILE"
        echo "--- End of error excerpt ---"
        echo
        
        FAILED_COUNT=$((FAILED_COUNT + 1))
        FAILED_CONFIGS+=("$CONFIG_NAME|$ERROR_LOG_FILE")
    fi

    # Clean up temp files from this run
    rm -f "$TIME_OUTPUT_FILE" "$RUN_MARKER_FILE"
done

# End overall timer
OVERALL_END_TIME=$(date +%s)
OVERALL_ELAPSED=$((OVERALL_END_TIME - OVERALL_START_TIME))

# Print summary
echo "========================================"
echo "Total configurations run: ${#CONFIGS_TO_RUN[@]}"
echo "Successful: $SUCCESS_COUNT"
echo "Failed: $FAILED_COUNT"
echo "Total time: $OVERALL_ELAPSED seconds"
echo

if [ $FAILED_COUNT -gt 0 ]; then
    echo "Failed configurations:"
    for failed_config in "${FAILED_CONFIGS[@]}"; do
        echo "  ✗ $failed_config"
    done
    echo
    exit 1
else
    echo "✓ All configurations completed successfully!"
    echo
    echo "Results available in:"
    echo "  - Logs: $LOG_DIR/"
    echo "  - Scalars: ../run_results/scalars/"
    echo "  - Vectors: ../run_results/vectors/"
    echo "  - PCAPs: ../run_results/pcaps/"
    echo
    exit 0
fi
