# Test-cases-002: Dynamic Traffic with DDoS Attack Simulation

## Overview

Test-cases-002 implements a self-contained DDoS simulation framework with:

- **Multi-protocol benign traffic** — JSON-configured IoT device profiles generating concurrent UDP, TCP, DNS, and HTTP traffic
- **Dedicated attack applications** — four DDoS attack types (UDP Flood, TCP SYN Flood, DNS Amplification, HTTP Flood) with configurable styles and behavior modes
- **Ground-truth labeling** — synchronized CSV labels with dual-vantage PCAP capture
- **No external modifications** — does not require patches to INET or Simu5G source code

For the software architecture and configuration details, see [TECHNICAL_REFERENCE.md](../../../docs/TECHNICAL_REFERENCE.md).

## Prerequisites

- DDoSimu5G installed (see root [README.md](../../../README.md))
- OMNeT++ 6.0.1 environment sourced (`source omnetpp-6.0.1/setenv`)

## Directory Structure

```
Test-cases-002/
├── config/
│   ├── infectionTraces/           # Infection timeline JSONs
│   │   ├── infection_single_ue.json
│   │   ├── infection_3ue_styles.json
│   │   └── infection_12ue_comprehensive.json
│   ├── trafficSchedules/          # Benign + attack profile JSONs
│   └── network/                   # Network topology configuration
├── scenarios/
│   ├── ned/                       # NED network definitions
│   └── TC-002-dynamic-traffic.ini
├── scripts/
│   ├── run_all_configs.sh
│   ├── compile_project.sh
│   ├── pcap_to_csv.py             # PCAP feature extraction
│   └── analyze_curated_v2.py      # Statistical analysis and reporting
├── run_results/                   # Simulation output
│   └── pcaps/
└── docs/                          # Historical documentation (archived)
```

## Scenarios

| Scenario | INI File | Description |
|----------|----------|-------------|
| Dynamic Traffic | `TC-002-dynamic-traffic.ini` | Mixed benign + attack traffic with infection dynamics |

## Running Simulations

```bash
# Source OMNeT++ environment
cd <omnetpp-dir>
source setenv

# Run all configurations
cd samples/DDoSimu5G/simulations/CaseID/Test-cases-002/scripts
./run_all_configs.sh
```

## Configuration

### Benign Traffic Profiles

Located in `config/trafficSchedules/`:

| Profile | Device Type | Protocols |
|---------|-------------|-----------|
| `iot_smart_meter.json` | Utility meter | UDP, HTTP, DNS |
| `iot_wearable_health_monitor.json` | Health tracker | UDP, TCP, HTTP, DNS |
| `iot_connected_vehicle.json` | Connected car | UDP, TCP, HTTP, DNS |
| `iot_drone_control.json` | Drone | UDP, TCP |
| `iot_industrial_sensor.json` | Industrial sensor | UDP, DNS |
| `iot_asset_tracker.json` | Asset tracker | UDP, DNS |
| `smartphone_user.json` | Smartphone | TCP, HTTP, UDP, DNS |
| `video_streamer.json` | Video device | TCP, HTTP, UDP, DNS |

### Attack Profiles

| Profile | Attack Type | Style | Behavior |
|---------|------------|-------|----------|
| `attack_udp_intense_coexist.json` | UDP Flood | Intense | Coexistence |
| `attack_udp_pulsing_replace.json` | UDP Flood | Pulsing | Replace |
| `attack_udp_slowrate_hybrid.json` | UDP Flood | Slow rate | Hybrid |
| `attack_tcp_intense_coexistence.json` | TCP SYN Flood | Intense | Coexistence |
| `attack_tcp_intense_replace.json` | TCP SYN Flood | Intense | Replace |
| `attack_tcp_ramping_hybrid.json` | TCP SYN Flood | Ramping | Hybrid |
| `attack_tcp_stealthy_replace.json` | TCP SYN Flood | Stealthy | Replace |
| `attack_dns_intense_replace.json` | DNS Amplification | Intense | Replace |
| `attack_dns_pulsing_hybrid.json` | DNS Amplification | Pulsing | Hybrid |
| `attack_dns_stealthy_coexist.json` | DNS Amplification | Stealthy | Coexistence |
| `attack_http_pulsing_replace.json` | HTTP Flood | Pulsing | Replace |
| `attack_http_ramping_coexist.json` | HTTP Flood | Ramping | Coexistence |
| `attack_http_slowrate_coexist.json` | HTTP Flood | Slow rate | Coexistence |

### Infection Timelines

Located in `config/infectionTraces/`:

| File | Description |
|------|-------------|
| `infection_single_ue.json` | Single UE infection |
| `infection_3ue_styles.json` | Three UEs with different attack styles |
| `infection_12ue_comprehensive.json` | 12 UEs, comprehensive mixed scenario |

## Output Structure

```
run_results/
├── pcaps/
│   └── <configname>/
│       └── <datetime>/
│           ├── gnb1.pcap          # gNodeB 1 (GTP-U encapsulated)
│           ├── gnb2.pcap          # gNodeB 2
│           ├── ...
│           └── upf.pcap           # UPF (decapsulated IP)
└── labels/
    └── ue{N}/
        └── app{M}.csv            # Per-app ground-truth labels
```

## Post-Simulation Analysis

Two Python scripts are provided for dataset extraction and statistical analysis.

### PCAP to CSV Extraction

```bash
# Convert a single PCAP to CSV
python3 pcap_to_csv.py --input upf.pcap --output features.csv

# Process all PCAPs in a directory
python3 pcap_to_csv.py --input-dir run_results/pcaps/<config>/<datetime>/ --output-dir csvs/

# Extract per-flow statistics
python3 pcap_to_csv.py --input upf.pcap --output flows.csv --mode flows
```

### Statistical Analysis

```bash
# Basic analysis
python3 analyze_curated_v2.py --csv features.csv --output-dir results/

# Flow-level analysis
python3 analyze_curated_v2.py --csv flows.csv --output-dir results/ --mode flows

# Dual-vantage comparison (gNodeB + UPF)
python3 analyze_curated_v2.py --csv features_gnb.csv --csv2 features_upf.csv --output-dir results/ --mode dual
```

See [PostAnalysis_README.md](scripts/PostAnalysis_README.md) for detailed script documentation.
