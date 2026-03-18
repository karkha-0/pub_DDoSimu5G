#!/bin/bash
##############################################################################
# TC-001a — Interactive Configuration Runner
#
# Maps to the test cases described in the SIGSIM-PADS paper:
#   Baseline-TC : 100 stationary UEs, benign CBR
#   TC001       : 100 mobile UEs (RWP), benign CBR
#   TC002       : 100 mobile + 400 stationary UEs, benign CBR
#   TC003       : 100 mobile + 400 stationary UEs, D2D-based UDP DDoS flooding
#
# Additional scenarios extend TC001 with slow-moving traces, moving extra
# UEs, or video-streaming UEs.
#
# Usage:
#   cd Test-cases-001a/script && ./run_all_configs.sh
##############################################################################

set -euo pipefail

# ── Locate and source environments ──────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"        # Test-cases-001a
SIM_DIR="$(cd "$TC_DIR/../.." && pwd)"        # simulations/CaseID

# Method 1: OMNET_ROOT already set
if [ -n "${OMNET_ROOT:-}" ] && [ -f "$OMNET_ROOT/setenv" ]; then
    OMNET_DIR="$OMNET_ROOT"
# Method 2: Walk up directory tree
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

INET_DIR="$OMNET_DIR/samples/inet4.5"
if [ -f "$INET_DIR/setenv" ]; then
    echo "Sourcing INET environment from $INET_DIR/setenv"
    source "$INET_DIR/setenv" -q
else
    echo "ERROR: Cannot find INET at $INET_DIR"; exit 1
fi

SIMU5G_DIR="$OMNET_DIR/samples/Simu5G"
if [ -f "$SIMU5G_DIR/setenv" ]; then
    echo "Sourcing Simu5G environment from $SIMU5G_DIR/setenv"
    cd "$SIMU5G_DIR" && source ./setenv -f && cd - > /dev/null
else
    echo "ERROR: Cannot find Simu5G at $SIMU5G_DIR"; exit 1
fi

DDOSIMU5G_DIR="$OMNET_DIR/samples/DDoSimu5G"
if [ -f "$DDOSIMU5G_DIR/setenv" ]; then
    echo "Sourcing DDoSimu5G environment from $DDOSIMU5G_DIR/setenv"
    source "$DDOSIMU5G_DIR/setenv" -q
else
    echo "ERROR: Cannot find DDoSimu5G at $DDOSIMU5G_DIR"; exit 1
fi

export LD_LIBRARY_PATH="${INET_ROOT}/src:${SIMU5G_ROOT}/src:${DDOSIMU5G_ROOT}/src:${LD_LIBRARY_PATH:-}"
export PROJECT_ROOT_DIR="$DDOSIMU5G_DIR"

# ── INI files (self-contained inside TC-001a) ──────────────────────────────
BASE_INI="$TC_DIR/TC-Base-DDoS-infec-5RAN-002.ini"
FLOOD_INI="$TC_DIR/TC-flood-DDoS-infec-5RAN-002.ini"

# ── Configuration definitions ──────────────────────────────────────────────
# Format: "paper_label|config_name|ini_file|run_index|description|category"
#
# run_index: 0 = ddosFlag=false (benign), 1 = ddosFlag=true (DDoS active)

declare -a PAPER_CONFIGS=(
    "Baseline-TC|TC-Base-DDoS-infec-5RAN-002-stationary|$BASE_INI|0|100 stationary UEs, benign CBR (Table 5 — Baseline-TC)|paper"
    "TC001|TC-Base-DDoS-infec-5RAN-002-MixedMoving|$BASE_INI|0|100 mobile UEs (RWP 1–5 m/s), benign CBR (Table 5 — TC001)|paper"
    "TC002|TC-Base-DDoS-infec-500cbrUEs-5RAN-002-MixedMoving|$BASE_INI|0|100 mobile + 400 stationary UEs, benign CBR (Table 5 — TC002)|paper"
    "TC003|TC-flood-DDoS-infec-500cbrUEsSt-5RAN-002-MixedMoving|$FLOOD_INI|1|100 mobile + 400 stationary, D2D UDP DDoS flooding (Table 5 — TC003)|paper"
)

declare -a EXTRA_CONFIGS=(
    "Extra-1|TC-Base-DDoS-infec-5RAN-002-slowMoving|$BASE_INI|0|100 UEs slow-moving (RWP 1–1 m/s), benign CBR|extra"
    "Extra-2|TC-Base-DDoS-infec-500cbrUEsMv-5RAN-002-MixedMoving|$BASE_INI|0|100 mobile + 400 moving UEs, benign CBR|extra"
    "Extra-3|TC-Base-DDoS-infec-100cbrUEsMv-50VidUEsSt-5RAN-002-MixedMoving|$BASE_INI|0|100 mobile CBR + 50 stationary video UEs|extra"
    "Extra-4|TC-Base-DDoS-infec-5RAN-002-stationary|$BASE_INI|1|100 stationary UEs, DDoS enabled (Baseline + DDoS)|extra"
    "Extra-5|TC-Base-DDoS-infec-5RAN-002-MixedMoving|$BASE_INI|1|100 mobile UEs (RWP), DDoS enabled (TC001 + DDoS)|extra"
    "Extra-6|TC-flood-DDoS-infec-5RAN-002-stationary|$FLOOD_INI|1|100 stationary UEs, flood-rate DDoS|extra"
    "Extra-7|TC-flood-DDoS-infec-5RAN-002-MixedMoving|$FLOOD_INI|1|100 mobile UEs, flood-rate DDoS|extra"
    "Extra-8|TC-flood-DDoS-infec-500cbrUEsMv-5RAN-002-MixedMoving|$FLOOD_INI|1|100 mobile + 400 moving, flood-rate DDoS|extra"
    "Extra-9|TC-flood-DDoS-infec-100cbrUEsMv-50VidUEsSt-5RAN-002-MixedMoving|$FLOOD_INI|1|100 mobile CBR + 50 video, flood-rate DDoS|extra"
)

declare -a ALL_CONFIGS=()
ALL_CONFIGS+=("${PAPER_CONFIGS[@]}")
ALL_CONFIGS+=("${EXTRA_CONFIGS[@]}")

# ── Interactive menu ───────────────────────────────────────────────────────
clear
echo "=========================================================================="
echo "  TC-001a — DDoSimu5G Configuration Runner"
echo "  (SIGSIM-PADS paper test cases + additional scenarios)"
echo "=========================================================================="
echo
echo "  PAPER TEST CASES (Table 5):"
echo "  ─────────────────────────────────────────────────────────────────────"
for i in "${!PAPER_CONFIGS[@]}"; do
    IFS='|' read -r label name ini run desc cat <<< "${PAPER_CONFIGS[$i]}"
    printf "  [%2d]  %-14s %s\n" "$((i + 1))" "$label" "$desc"
done

echo
echo "  ADDITIONAL SCENARIOS:"
echo "  ─────────────────────────────────────────────────────────────────────"
EXTRA_START=$(( ${#PAPER_CONFIGS[@]} + 1 ))
for i in "${!EXTRA_CONFIGS[@]}"; do
    IFS='|' read -r label name ini run desc cat <<< "${EXTRA_CONFIGS[$i]}"
    printf "  [%2d]  %-14s %s\n" "$((EXTRA_START + i))" "$label" "$desc"
done

TOTAL=${#ALL_CONFIGS[@]}
echo
echo "  BATCH OPTIONS:"
echo "  ─────────────────────────────────────────────────────────────────────"
printf "  [%2d]  Run all PAPER test cases (Baseline-TC, TC001, TC002, TC003)\n" "$((TOTAL + 1))"
printf "  [%2d]  Run all EXTRA scenarios\n" "$((TOTAL + 2))"
printf "  [%2d]  Run EVERYTHING\n" "$((TOTAL + 3))"
printf "  [ 0]  Exit\n"
echo "=========================================================================="
echo

read -p "Enter your choice(s) (space-separated, e.g. '1 2 3 4'): " choices

# ── Parse selections ───────────────────────────────────────────────────────
declare -a SELECTED_INDICES=()

[[ "$choices" == "0" ]] && { echo "Exiting..."; exit 0; }

RUN_PAPER=$((TOTAL + 1))
RUN_EXTRA=$((TOTAL + 2))
RUN_ALL=$((TOTAL + 3))

for choice in $choices; do
    if [[ "$choice" == "$RUN_PAPER" ]]; then
        for ((i=0; i<${#PAPER_CONFIGS[@]}; i++)); do SELECTED_INDICES+=($i); done
    elif [[ "$choice" == "$RUN_EXTRA" ]]; then
        for ((i=${#PAPER_CONFIGS[@]}; i<${#ALL_CONFIGS[@]}; i++)); do SELECTED_INDICES+=($i); done
    elif [[ "$choice" == "$RUN_ALL" ]]; then
        for ((i=0; i<${#ALL_CONFIGS[@]}; i++)); do SELECTED_INDICES+=($i); done
    elif [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= TOTAL )); then
        SELECTED_INDICES+=($((choice - 1)))
    else
        echo "WARNING: Invalid choice '$choice' — skipping"
    fi
done

# De-duplicate and sort
SELECTED_INDICES=($(echo "${SELECTED_INDICES[@]}" | tr ' ' '\n' | sort -nu | tr '\n' ' '))

if [[ ${#SELECTED_INDICES[@]} -eq 0 ]]; then
    echo "ERROR: No valid configurations selected!"; exit 1
fi

# ── Prepare output directories ─────────────────────────────────────────────
LOG_DIR="$TC_DIR/run_results/logs"
mkdir -p "$LOG_DIR"

# System info (collected once)
CPU_MODEL=$(grep -m1 "model name" /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs 2>/dev/null || echo "unknown")
CPU_CORES=$(nproc 2>/dev/null || echo "1")

# ── Confirm ────────────────────────────────────────────────────────────────
echo
echo "=========================================================================="
echo "  Selected configurations:"
echo "=========================================================================="
for idx in "${SELECTED_INDICES[@]}"; do
    IFS='|' read -r label name ini run desc cat <<< "${ALL_CONFIGS[$idx]}"
    printf "  • %-14s %s\n" "$label" "$desc"
done
echo "=========================================================================="
echo
read -p "Press Enter to start, or Ctrl+C to cancel..."

# ── Run simulations ────────────────────────────────────────────────────────
OVERALL_START=$(date +%s)
SUCCESS_COUNT=0
FAILED_COUNT=0
declare -a FAILED_LIST=()

for seq in "${!SELECTED_INDICES[@]}"; do
    idx="${SELECTED_INDICES[$seq]}"
    IFS='|' read -r LABEL CONFIG_NAME INI_FILE RUN_INDEX DESCRIPTION CATEGORY <<< "${ALL_CONFIGS[$idx]}"

    NUM=$((seq + 1))
    TOTAL_SEL=${#SELECTED_INDICES[@]}

    echo
    echo "=========================================================================="
    echo "  [$NUM/$TOTAL_SEL]  $LABEL — $CONFIG_NAME"
    echo "  $DESCRIPTION"
    echo "=========================================================================="

    DATE_TIME=$(date +"%Y%m%d_%H%M%S")
    LOG_FILE="$LOG_DIR/${LABEL}_${CONFIG_NAME}_${DATE_TIME}.log"
    echo "  Log: $LOG_FILE"

    START_TIME=$(date +%s)

    # Temp file for /usr/bin/time stats
    TIME_FILE=$(mktemp /tmp/ddosimu5g_time_XXXXXX)

    # Run simulation — working directory = TC-001a so relative paths in INI resolve correctly
    /usr/bin/time -f "%M %P" -o "$TIME_FILE" \
    opp_run -r "$RUN_INDEX" \
      -c "$CONFIG_NAME" \
      -n "$TC_DIR/networks/:$DDOSIMU5G_ROOT/src/:$INET_ROOT/src:$SIMU5G_ROOT/src" \
      -l "$INET_ROOT/src/libINET.so" \
      -l "$SIMU5G_ROOT/src/libsimu5g.so" \
      -l "$DDOSIMU5G_ROOT/src/libDDoSimu5G.so" \
      --image-path="$INET_ROOT/images:$SIMU5G_ROOT/images" \
      -u Cmdenv \
      -f "$INI_FILE" \
      --result-dir="$TC_DIR/run_results" \
      --cmdenv-redirect-output=true \
      > "$LOG_FILE" 2>&1 \
    || true   # don't abort set -e on sim failure

    EXIT_CODE=${PIPESTATUS[0]:-$?}
    END_TIME=$(date +%s)
    ELAPSED=$((END_TIME - START_TIME))

    if [ $EXIT_CODE -eq 0 ]; then
        echo "  ✓ SUCCESS — $ELAPSED seconds"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))

        # ── Collect perf stats ──
        TIME_STATS=$(cat "$TIME_FILE" 2>/dev/null || echo "0 0%")
        PEAK_RSS_KB=$(echo "$TIME_STATS" | awk '{print $1+0}')
        CPU_PCT=$(echo "$TIME_STATS" | awk '{gsub(/%/,"",$2); print $2+0}')
        PEAK_RSS_MB=$(LC_NUMERIC=C awk "BEGIN {printf \"%.1f\", $PEAK_RSS_KB / 1024}")

        PERF_DIR="$TC_DIR/run_results/${LABEL}_${CONFIG_NAME}_${DATE_TIME}"
        mkdir -p "$PERF_DIR"
        cat > "$PERF_DIR/perf.json" << PERF_EOF
{
  "paper_label": "$LABEL",
  "scenario": "$CONFIG_NAME",
  "description": "$DESCRIPTION",
  "category": "$CATEGORY",
  "ini_file": "$(basename "$INI_FILE")",
  "run_index": $RUN_INDEX,
  "run_timestamp": "$DATE_TIME",
  "wall_time_s": $ELAPSED,
  "cpu": {
    "model": "$CPU_MODEL",
    "cores_available": $CPU_CORES,
    "avg_utilization_pct": $CPU_PCT
  },
  "memory": {
    "peak_rss_mb": $PEAK_RSS_MB
  }
}
PERF_EOF
        echo "  perf.json → $PERF_DIR/perf.json"
    else
        ERROR_LOG="${LOG_FILE%.log}_ERROR.log"
        mv "$LOG_FILE" "$ERROR_LOG"
        echo "  ✗ FAILED — exit code $EXIT_CODE ($ELAPSED seconds)"
        echo "  --- Last 20 lines ---"
        tail -n 20 "$ERROR_LOG"
        echo "  ----------------------"
        FAILED_COUNT=$((FAILED_COUNT + 1))
        FAILED_LIST+=("$LABEL|$CONFIG_NAME|$ERROR_LOG")
    fi

    rm -f "$TIME_FILE"
done

# ── Summary ────────────────────────────────────────────────────────────────
OVERALL_END=$(date +%s)
OVERALL_ELAPSED=$((OVERALL_END - OVERALL_START))

echo
echo "=========================================================================="
echo "  SUMMARY"
echo "=========================================================================="
echo "  Total run      : ${#SELECTED_INDICES[@]}"
echo "  Successful     : $SUCCESS_COUNT"
echo "  Failed         : $FAILED_COUNT"
echo "  Total time     : $OVERALL_ELAPSED seconds"
echo

if [[ $FAILED_COUNT -gt 0 ]]; then
    echo "  Failed configurations:"
    for f in "${FAILED_LIST[@]}"; do
        IFS='|' read -r fl fn fe <<< "$f"
        echo "    ✗ $fl ($fn) → $fe"
    done
    echo
    exit 1
else
    echo "  ✓ All configurations completed successfully!"
    echo
    echo "  Results in: $TC_DIR/run_results/"
    echo
    exit 0
fi
