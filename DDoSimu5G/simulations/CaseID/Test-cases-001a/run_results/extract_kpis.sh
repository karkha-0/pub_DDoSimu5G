#!/usr/bin/env bash
# ============================================================================
#  extract_kpis.sh — Extract 5G KPIs from OMNeT++ result files using scavetool
#
#  Extracts:
#    1. UPF incoming data rate  (upf.pppIf.queue incomingDataRate)
#
#  Usage:  ./extract_kpis.sh [--results-dir DIR]
#          Default results source: the storage install run_results
#          Output: ../run_results/extracted_data/
# ============================================================================
set -eo pipefail

# ── Resolve paths from script location ──────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Output directory (inside workspace)
EXPORT_DIR="$TC_DIR/run_results/extracted_data"
mkdir -p "$EXPORT_DIR"

# Source run_results — default to the storage install, or pass --results-dir
RUN_RESULTS=""
if [[ "${1:-}" == "--results-dir" && -n "${2:-}" ]]; then
    RUN_RESULTS="$2"
else
    # Walk up from script to find an OMNeT++ install with run_results
    dir="$SCRIPT_DIR"
    for _ in 1 2 3 4 5 6 7 8; do
        dir="$(dirname "$dir")"
        candidate="$dir/omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-001a/run_results"
        if [[ -d "$candidate" ]]; then
            RUN_RESULTS="$candidate"
            break
        fi
    done
    # Also check the storage path directly
    if [[ -z "$RUN_RESULTS" || ! -d "$RUN_RESULTS" ]]; then
        for p in \
            #"/home/kkh/storage/ide/pub_DDoSimu5G/omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-001a/run_results" \
            "run_results" \
            "$TC_DIR/run_results"; do
            if [[ -d "$p" ]] && find "$p" -maxdepth 2 -name '*.vec' -print -quit 2>/dev/null | grep -q .; then
                RUN_RESULTS="$p"
                break
            fi
        done
    fi
fi

if [[ -z "$RUN_RESULTS" || ! -d "$RUN_RESULTS" ]]; then
    echo "ERROR: Cannot find run_results directory with .vec files."
    echo "  Pass explicitly:  $0 --results-dir /path/to/run_results"
    exit 1
fi

# ── Find OMNeT++ installation ───────────────────────────────────────────────
find_omnet_root() {
    local dir="$RUN_RESULTS"
    for _ in 1 2 3 4 5 6 7 8; do
        dir="$(dirname "$dir")"
        if [[ -f "$dir/setenv" && -f "$dir/bin/opp_scavetool" ]]; then
            echo "$dir"
            return 0
        fi
    done
    # Try OMNET_ROOT env var
    if [[ -n "${OMNET_ROOT:-}" && -f "$OMNET_ROOT/bin/opp_scavetool" ]]; then
        echo "$OMNET_ROOT"
        return 0
    fi
    return 1
}

OMNET_ROOT="$(find_omnet_root)" || {
    echo "ERROR: Cannot find OMNeT++ installation (need opp_scavetool)."
    exit 1
}

# Source OMNeT++ environment
set +u
source "$OMNET_ROOT/setenv" >/dev/null 2>&1
set -u

echo "============================================================"
echo "  DDoSimu5G KPI Extraction (scavetool)"
echo "============================================================"
echo "  OMNeT++:       $OMNET_ROOT"
echo "  Source data:   $RUN_RESULTS"
echo "  Export to:     $EXPORT_DIR"
echo "============================================================"
echo ""

# ── Map result folders to paper labels ──────────────────────────────────────
declare -A LABEL_MAP=(
    ["TC-Base-DDoS-infec-5RAN-002-stationary"]="Baseline-TC"
    ["TC-Base-DDoS-infec-5RAN-002-MixedMoving"]="TC001"
    ["TC-Base-DDoS-infec-500cbrUEs-5RAN-002-MixedMoving"]="TC002"
    ["TC-flood-DDoS-infec-500cbrUEsSt-5RAN-002-MixedMoving"]="TC003"
)

# ── Process each scenario ───────────────────────────────────────────────────
found=0
for result_dir in "$RUN_RESULTS"/*/; do
    dir_name="$(basename "$result_dir")"

    # Skip non-result folders (logs, extracted_data, kpi_exports, timestamped perf dirs, .out files)
    [[ "$dir_name" == "logs" ]] && continue
    [[ "$dir_name" == "extracted_data" ]] && continue
    [[ "$dir_name" == "kpi_exports" ]] && continue
    [[ "$dir_name" == *_20* ]] && continue
    [[ "$dir_name" == *.out ]] && continue

    # Find .vec file in this folder
    vec_file="$(find "$result_dir" -maxdepth 1 -name '*.vec' -print -quit 2>/dev/null)"
    if [[ -z "$vec_file" ]]; then
        echo "⚠  $dir_name — no .vec file found, skipping"
        continue
    fi

    label="${LABEL_MAP[$dir_name]:-$dir_name}"
    found=$((found + 1))

    echo "─────────────────────────────────────────────────"
    echo "  [$found]  $label  ($dir_name)"
    echo "─────────────────────────────────────────────────"

    # --- 1. UPF incoming data rate (pppIf interface) ---
    outfile="$EXPORT_DIR/${label}_upf_incomingDataRate.csv"
    echo -n "  UPF incomingDataRate ... "
    if opp_scavetool export -o "$outfile" -F CSV-R \
        -f 'module=~"**.upf.pppIf.queue" AND name=~"incomingDataRate:vector"' \
        "$vec_file" 2>&1; then
        lines=$(wc -l < "$outfile")
        if [[ $lines -gt 1 ]]; then
            echo "✓  ($((lines - 1)) data rows) → $(basename "$outfile")"
        else
            echo "✗  empty result (0 data rows)"
            rm -f "$outfile"
        fi
    else
        echo "✗  extraction failed"
        rm -f "$outfile"
    fi

    echo ""
done

if [[ $found -eq 0 ]]; then
    echo "ERROR: No result directories with .vec files found in:"
    echo "  $RUN_RESULTS"
    exit 1
fi

echo "============================================================"
echo "  Done — $found scenario(s) processed"
echo "  CSV files in: $EXPORT_DIR"
echo "============================================================"
echo ""
ls -lh "$EXPORT_DIR"/*.csv 2>/dev/null || echo "  (no CSV files produced)"
