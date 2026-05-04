# DDoSimu5G 

## General 
This project provides a modular simulation framework combining Simu5G and The ONE Simulator to study the impact of malware-based DDoS attacks in 5G infrastructures. It includes:

- Mobility and device-to-device infection modeling (via ONE)
- Traffic orchestration and malware triggers
- Realistic CBR and flooding traffic patterns (UDP-based)
- KPI extraction (data rate, packet loss, propagation)
- Configurable test cases for benign and adversarial scenarios
- Jupyter-based result analysis

DDoSimu5G is useful for researchers and practitioners studying IoT-borne malware, network resilience, and 5G traffic behavior under stress.

## Authors & Contacts

- **Karim Khalil** *(corresponding author)* — karim.khalil@eit.lth.se — Department of EIT, Lund University
- **Christian Gehrmann** — christian.gehrmann@eit.lth.se — Department of EIT, Lund University
- **Sara Ramezanian** — sara.ramezanian@kau.se — Lund University & Karlstad University
- **Jakob Sternby** — jakob.sternby@ericsson.com — Ericsson Research

## Quick Start

### Prerequisites
- Ubuntu 20.04+ / Debian 11+ / Fedora 35+ / RHEL 8+
- 20GB free disk space
- 8GB RAM minimum (16GB recommended)
- Internet connection
- sudo access (for package installation)

### Installation (Complete Setup)

```bash
# Clone the repository
git clone https://github.com/karkha-0/pub_DDoSimu5G.git
cd pub_DDoSimu5G

# Run complete installation (30-45 minutes)
sudo chmod +x setup.sh
./setup.sh
```

### Installed Components

After installation, you will have:

- **OMNeT++ 6.0.1**: Network simulation framework (WITH_NETBUILDER enabled)
- **INET 4.5.0**: Internet protocol suite for OMNeT++
- **Simu5G 1.2.2**: 5G NR network simulation library
- **pub_DDoSimu5G**: This project with DDoS simulation capabilities
- **Python venv**: Virtual environment with `posix_ipc` for result analysis
- **ONE Simulator 1.6.0**: Opportunistic Network Environment (optional)

Component versions are defined in `env-requirements.json`.

That's it! The script will:
1. Install OMNeT++ 6.0.1, INET 4.5.0, and Simu5G 1.2.2
2. Set up the pub_DDoSimu5G project
3. Build all components

## Installation Options

### Modular Installation

You can install components separately for more control:

```bash
# Install just the environment (OMNeT++, INET, Simu5G)
./setup_environment.sh

# Later, add the pub_DDoSimu5G project
./setup_project.sh
```

This is useful if you:
- Want to use the environment for multiple projects
- Already have OMNeT++ installed
- Need to rebuild just the project

### Custom Installation

```bash
# Install to a specific directory
./setup.sh --install-dir ~/simulation

# Use your own fork
./setup.sh --repo-url https://github.com/yourname/your-fork.git

# Skip ONE Simulator (if you don't need mobility trace generation)
./setup.sh --without-one

# Force reinstallation
./setup.sh --force
```

### Advanced Options

**Environment Setup** (`setup_environment.sh`):
- `--install-dir DIR`: Installation directory (default: current directory)
- `--skip-packages`: Skip system package installation
- `--force`: Force reinstallation even if exists
- `--help`: Show detailed help

**Project Setup** (`setup_project.sh`):
- `--omnet-dir DIR`: OMNeT++ installation directory
- `--repo-url URL`: Custom repository URL
- `--branch BRANCH`: Git branch to checkout
- `--with-one` / `--without-one`: Control ONE Simulator installation
- `--force`: Force reinstallation
- `--help`: Show detailed help

## System Requirements

| Component | Requirement |
|-----------|-------------|
| OS | Ubuntu 20.04+, Debian 11+, Fedora 35+, RHEL 8+ |
| Disk Space | 20GB free |
| RAM | 8GB minimum, 16GB recommended |
| CPU | Multi-core recommended for compilation |
| Python | 3.8+ with pip and venv |
| Internet | Required for initial download |

## Hardware/Software Configuration Used by Authors

The experiments reported in the paper were run on:

| Component | Specification |
|-----------|---------------|
| CPU | Intel Core i7-8700K @ 3.70GHz (6 cores / 12 threads) |
| RAM | 32 GB DDR4 |
| OS | Ubuntu 24.04.3 LTS (kernel 6.8.0) |
| Compiler | GCC (system default) |
| Simulator | OMNeT++ 6.0.1 Cmdenv mode |

## Running the Simulations

After installation, run the paper test cases (Table 5 of the paper):

```bash
cd omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-001a/
./script/run_all_configs.sh
```

The interactive menu offers:

| Option | Paper Label | Scenario |
|--------|------------|----------|
| 1 | Baseline-TC | 100 stationary UEs, benign CBR |
| 2 | TC001 | 100 mobile UEs (RWP), benign CBR |
| 3 | TC002 | 100 mobile + 400 stationary, benign |
| 4 | TC003 | DDoS flooding (100 mobile + 400 stationary) |
| 14 | — | Run all 4 paper test cases |

For a quick **smoke test**, select option 1 (Baseline-TC) — runs in ~16 minutes.

The script automatically:
- Detects the OMNeT++, INET, Simu5G, and DDoSimu5G installations
- Sets up library paths and environment
- Runs simulations in Cmdenv (headless) mode
- Collects performance metrics (`perf.json`) per run
- Logs output to `run_results/logs/`

A successful run produces:
- Simulation log in `Test-cases-001a/run_results/logs/`
- Performance stats in `Test-cases-001a/run_results/<label>_<config>_<timestamp>/perf.json`
- OMNeT++ result files (`.sca`, `.vec`) in `Test-cases-001a/run_results/`

**Note**: Modified source files from `modifiedExternalFiles/` are automatically applied during installation by `setup_project.sh`.

### Expected Runtimes

| Test Case | Paper Label | Description | Expected Runtime | Peak RAM |
|-----------|------------|-------------|-----------------|----------|
| Option 1 | Baseline-TC | 100 stationary UEs, benign CBR | ~16 min | ~210 MB |
| Option 2 | TC001 | 100 mobile UEs, benign CBR | ~22 min | ~285 MB |
| Option 3 | TC002 | 100 mobile + 400 stationary, benign | ~139 min | ~614 MB |
| Option 4 | TC003 | DDoS flooding (500 UEs) | ~231 min | ~2,124 MB |
| **All 4** | — | Full paper reproduction | **~6–7 hours** | — |

> Runtimes measured on the authors' hardware (see above). Your times may vary.


## Custom Experiments

New experiments require only two steps: add a config block to an INI file, then run it.

### Step 1 — Define a new configuration

Open `DDoSimu5G/simulations/CaseID/Test-cases-001a/TC-Base-DDoS-infec-5RAN-002.ini` and append a block that `extends` any existing config:

```ini
[Config My-50UE-Experiment]
description = "Quick experiment with 50 stationary UEs"
extends = TC-Base-DDoS-infec-5RAN-002

*.numCbrUe = 50
*.numVidUe = 0
*.cbrUe[*].mobility.typename = "StationaryMobility"
```

Key parameters you can override:

| Parameter | Default (Baseline-TC) | Effect |
|---|---|---|
| `*.numCbrUe` | 100 | Number of CBR UEs |
| `*.numVidUe` | 0 | Number of video-streaming UEs |
| `**.cbrUe[*].app[0].PacketSize` | 100 (bytes) | Benign traffic packet size |
| `**.cbrUe[*].app[0].sampling_time` | 1 s | Inter-packet interval |
| `*.trafficController.enableTrafficMod` | false | Enable DDoS trigger |
| `sim-time-limit` | 3600 s | Simulation duration |

### Step 2 — Run the new configuration

```bash
cd omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-001a
source ../../../../../setenv

opp_run -r 0 -m -u Cmdenv -c My-50UE-Experiment \
  -n "../networks:../../../src:../../../../../../inet*/src:../../../../../../Simu5G*/src" \
  -l ../../../src/DDoSimu5G \
  -l ../../../../../../inet*/src/INET \
  -l ../../../../../../Simu5G*/src/simu5g \
  TC-Base-DDoS-infec-5RAN-002.ini
```

Alternatively, add an entry to `script/run_all_configs.sh` under the `EXTRA_CONFIGS` array:

```bash
"My-Exp|My-50UE-Experiment|$BASE_INI|0|50 stationary UEs, benign CBR|extra"
```

then select it from the interactive menu.

> The existing 9 Extra scenarios (`Extra-1` – `Extra-9`) in `run_all_configs.sh` already demonstrate further reuse: varying UE count (100/500), mobility model (stationary/slow/mixed), and DDoS flag, all within the same framework without changing any C++ code.

## Results Analysis 
### Convert D2D Model mobility traces from ONE to Simu5G

If you need to generate new mobility traces (existing traces are pre-generated):

```bash
cd omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/script/mob_tracesConversionPy/fromONEtoSimu5G/

# Convert JSON mobility traces to .mobility format
python3 convert_mob_traces.py <path-to-json-mobility> <output.movements>
```

### Results and Visualization

#### D2D Infection & Mobility Analysis

Plots for the D2D malware propagation model are in:

```bash
# Activate the project virtual environment (created by setup.sh)
cd omnetpp-6.0.1
source venv/bin/activate

cd samples/DDoSimu5G/results_analysis/D2D_analysis/plotting_results_MalwareInfection/
jupyter notebook
```

Notebooks:
- `D2D_Infection_Analysis.ipynb` — infection rate, survival curves, propagation chains
- `D2D_Contact_Analysis.ipynb` — node connectivity, contact variability
- `Plotting Infection Propagation.ipynb` — mobility trajectories & infection locations

All required Python packages (pandas, matplotlib, seaborn, lifelines, networkx, etc.) are pre-installed in the virtual environment by `setup.sh`.

#### 5G KPI Extraction & Visualization

After running the simulations, extract and visualize 5G network KPIs:

**Step 1 — Extract UPF data rate from simulation results:**

```bash
cd omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/Test-cases-001a/run_results
./extract_kpis.sh
```

This uses `opp_scavetool` to export UPF incoming data rate vectors from each test case's `.vec` files into CSV-R format under `run_results/extracted_data/`.

**Step 2 — Open the analysis notebook:**

```bash
# Activate the project virtual environment while standing on the repo's base directory
source venv/bin/activate

cd samples/DDoSimu5G/results_analysis/5G_KPI_analysis/
jupyter notebook KPI_Analysis.ipynb
```

The notebook produces:
- **Plot 1** — Baseline: UPF data rate with 100 stationary UEs (kbps)
- **Plot 2** — TC001: UPF data rate with 100 roaming UEs (kbps)
- **Plot 3** — TC002: UPF data rate with 500 UEs, benign traffic (kbps)
- **Plot 4** — TC003: UPF data rate under DDoS flooding (Mbps)

### Opening Project in OMNeT++ IDE

```bash
# Launch OMNeT++ IDE
source venv/bin/activate
cd omnetpp-6.0.1
./bin/omnetpp

# In IDE: File → Open Projects from File System...
# Select: omnetpp-6.0.1/samples/DDoSimu5G
```

**Configure NED file locations** (if needed):
- Right-click project → Properties → OMNeT++ → NED Source Folders
- Ensure all source directories are included

## Development and Contributing

### Building from Source

```bash
# If you modify the project source
cd omnetpp-6.0.1/samples/DDoSimu5G/src
make clean
make MODE=release
```

## Troubleshooting

### Installation Issues

**Q: Installation fails with package errors**
```bash
# Try skipping package installation and install manually
./setup_environment.sh --skip-packages
```

**Q: OMNeT++ compilation fails**
- Ensure you have at least 8GB RAM
- Check internet connection (downloads ~500MB)
- See logs in `omnetpp-6.0.1/config.log`

**Q: Project build fails**
```bash
# Rebuild just the project
./setup_project.sh --force
```

**Q: "OMNeT++ not found" error**
```bash
# Specify OMNeT++ location manually
./setup_project.sh --omnet-dir /path/to/omnetpp-6.0.1
```

### Runtime Issues

**Q: NED file loading error**
- Ensure OMNeT++ was built with `WITH_NETBUILDER=yes` (automatic in setup scripts)
- Check project properties: Right-click project → Properties → OMNeT++ → NED Source Folders

**Q: Library not found errors**
```bash
# Source the OMNeT++ environment
cd omnetpp-6.0.1
source setenv

# The simulation scripts automatically handle this
```

**Q: Python module import errors**
```bash
# Activate the virtual environment
cd omnetpp-6.0.1
source venv/bin/activate

# Install additional packages if needed
pip install jupyter pandas matplotlib numpy
```

## Project Structure

```
pub_DDoSimu5G/
├── setup.sh                        # Master installation script
├── setup_environment.sh            # Environment-only installer
├── setup_project.sh                # Project-only installer
├── AUTHORS                         # Authors and citation info
├── LICENSE                         # MIT License
├── docs/
│   └── env-requirements.json       # Version requirements
├── DDoSimu5G/                      # Main project (installed into omnetpp samples/)
│   ├── src/                        # DDoSimu5G C++ source code
│   ├── simulations/CaseID/
│   │   ├── config/                 # Shared XML configs & infection traces
│   │   ├── networks/               # NED network topologies
│   │   ├── Test-cases-001a/        # ★ Self-contained TC-001 (paper test cases)
│   │   │   ├── TC-Base-DDoS-infec-5RAN-002.ini   # Benign configs
│   │   │   ├── TC-flood-DDoS-infec-5RAN-002.ini  # DDoS flood configs
│   │   │   ├── config/             # Local XML + infection traces
│   │   │   ├── networks/           # Local NED files
│   │   │   ├── run_results/
│   │   │   │   ├── extract_kpis.sh     # ★ KPI extraction via scavetool
│   │   │   │   └── extracted_data/     # CSV output from extract_kpis.sh
│   │   │   └── script/
│   │   │       └── run_all_configs.sh  # ★ Unified runner (paper test cases)
│   │   └── Test-cases-001/         # Legacy (reference only)
│   ├── results_analysis/
│   │   ├── 5G_KPI_analysis/        # ★ 5G KPI extraction & visualization
│   │   │   └── KPI_Analysis.ipynb  # UPF data rate & RLC packet loss plots
│   │   └── D2D_analysis/           # ★ D2D infection & mobility analysis
│   │       ├── plotting_results_MalwareInfection/  # Jupyter notebooks + plots
│   │       └── simulation_results_MalwareInfection/ # Raw ONE Simulator run data
│   ├── modifiedExternalFiles/      # Patches for INET/Simu5G
│   └── ONE_Simulator/              # Mobility trace generator
└── archive/                        # Deprecated scripts
```

## Crediting
  
This project is licensed under the MIT License. You are free to use, modify, and distribute this software for academic and non-commercial purposes, provided proper credit is given to the original authors.

If you use DDoSimu5G in your research, please cite the repository.

## Support and Documentation

- **Issues**: [GitHub Issues](https://github.com/karkha-0/pub_DDoSimu5G/issues)
- **Documentation**: See inline comments and `CHANGELOG.md`
- **OMNeT++ Manual**: `omnetpp-6.0.1/doc/manual/index.html`
- **INET Documentation**: [INET Framework Docs](https://inet.omnetpp.org/)
- **Simu5G Documentation**: [Simu5G Website](http://simu5g.org/)

## Version Information

- **Current Version**: v1.1.0
- **OMNeT++**: 6.0.1
- **INET**: 4.5.0
- **Simu5G**: 1.2.2
- **ONE Simulator**: 1.6.0

See `env-requirements.json` for complete version specifications.

---

For questions, issues, or contributions, please open an issue or contact the maintainers via GitHub.


