#!/usr/bin/env bash
set -e

cd /code
chmod +x setup.sh
./setup.sh --skip-packages

cd /code/omnetpp-6.0.1
source setenv

# 1. Run the simulation (option 9 = DDoS-Multi-Attack-Multi-Behavior)
cd /code/omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-002a/scripts
chmod +x run_all_configs.sh
./run_all_configs.sh <<< "9"

# 2. Activate venv and run pcap_to_csv.py on the latest timestamped run directory
source /code/venv/bin/activate

cd /code/omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-002a/scripts

CONFIG_NAME="DDoS-Multi-Attack-Multi-Behavior"
LATEST_RUN=$(find "../run_results/pcaps/$CONFIG_NAME" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sort | tail -1)
if [ -z "$LATEST_RUN" ]; then
    echo "WARNING: No PCAP run directory found for $CONFIG_NAME"
else
    echo "Processing PCAP run: $LATEST_RUN"
    python3 pcap_to_csv.py --input-dir "$LATEST_RUN" --output /results/ 

# 3. Copy simulation results (PCAPs, vectors, scalars) to /results/
RUN_RESULTS="/code/omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-002a/run_results"
if [ -d "$RUN_RESULTS" ]; then
    cp -r "$RUN_RESULTS"/* /results/ 2>/dev/null || true
else
    echo "WARNING: run_results directory not found at $RUN_RESULTS"
fi