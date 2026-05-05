# DDoSimu5G

## Overview

DDoSimu5G is a modular simulation framework for studying DDoS attacks in 5G network infrastructures. Built on OMNeT++ 6.0.1, INET 4.5, and Simu5G 1.2.2, it enables researchers to generate labeled datasets for training ML/AI-based intrusion detection systems.

### Key Features

- **Dynamic multi-protocol traffic generation** — concurrent UDP, TCP, DNS, and HTTP handlers within single UE applications, configured via JSON profiles
- **Adversarial application framework** — modular DDoS attacks (UDP Flood, TCP SYN Flood, DNS Amplification, HTTP Flood) with configurable intensity styles and behavior modes
- **Infection dynamics** — malware propagation timelines from ONE Simulator D2D traces or standalone JSON schedules
- **Ground-truth labeling** — automated out-of-band CSV traffic labels synchronized with dual-vantage PCAP captures (gNodeB + UPF)
- **Realistic IoT device profiles** — smart meters, wearables, connected vehicles, drones, smartphones, and video streamers
- **Configurable test cases** — benign baselines and adversarial scenarios across different mobility patterns

### Test Case Suites

The project includes two test case suites with different approaches:

| Suite | Approach | Description |
|-------|----------|-------------|
| **[Test-cases-001](DDoSimu5G/simulations/CaseID/Test-cases-001/README.md)** | CBR parameter modification + ONE Simulator | Volumetric UDP flood attacks with mobility variants. Requires `modifiedExternalFiles/` patches to INET/Simu5G. Published at ACM SIGSIM PADS. |
| **[Test-cases-002](DDoSimu5G/simulations/CaseID/Test-cases-002/README.md)** | Self-contained dynamic traffic + dedicated attack apps | Multi-protocol benign traffic with diverse DDoS attack types and styles. No external source modifications required. |

Each test case suite has its own README with instructions for running simulations, configuration details, and output structure.

For the software architecture and configuration reference, see [docs/TECHNICAL_REFERENCE.md](docs/TECHNICAL_REFERENCE.md).

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
./setup.sh
```

The script will:
1. Install OMNeT++ 6.0.1, INET 4.5.0, and Simu5G 1.2.2
2. Apply source modifications for Test-cases-001 compatibility
3. Build the DDoSimu5G project library
4. Compile the ONE Simulator (optional)

### Running Simulations

After installation, each test case suite provides its own run scripts:

- **Test-cases-001:** `simulations/CaseID/script/runSim_TC-*.sh`
- **Test-cases-002:** `simulations/CaseID/Test-cases-002/scripts/run_TC-002-*.sh`

See the individual test case READMEs for detailed instructions.

## Installation Options

### Modular Installation

You can install components separately for more control:

```bash
# Install just the environment (OMNeT++, INET, Simu5G)
./setup_environment.sh

# Later, add the DDoSimu5G project
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

## Installed Components

After installation, you will have:

- **OMNeT++ 6.0.1**: Network simulation framework (WITH_NETBUILDER enabled)
- **INET 4.5.0**: Internet protocol suite for OMNeT++
- **Simu5G 1.2.2**: 5G NR network simulation library
- **DDoSimu5G**: This project with DDoS simulation capabilities
- **Python venv**: Virtual environment with `posix_ipc` for result analysis
- **ONE Simulator 1.6.0**: Opportunistic Network Environment (optional)

Component versions are defined in [docs/env-requirements.json](docs/env-requirements.json).

## Project Structure

```
pub_DDoSimu5G/
├── setup.sh                      # Master installation script
├── setup_environment.sh          # Environment installer (OMNeT++, INET, Simu5G)
├── setup_project.sh              # Project installer
├── README.md
├── LICENSE
├── AUTHORS
├── docs/
│   ├── TECHNICAL_REFERENCE.md    # Software architecture and configuration guide
│   ├── CHANGELOG.md
│   ├── TROUBLESHOOTING.md
│   └── env-requirements.json     # Version requirements
├── DDoSimu5G/                    # Project source (installed into omnetpp samples/)
│   ├── Makefile
│   ├── src/
│   │   └── ddosimu5g/
│   │       ├── apps/
│   │       │   ├── dynamic/              # DynamicTrafficSender/Receiver
│   │       │   │   ├── protocols/        # UDP, TCP, DNS, HTTP handlers
│   │       │   │   │   └── receiver/     # Server-side protocol handlers
│   │       │   │   └── trafficlabel/     # Ground-truth labeling system
│   │       │   └── adversarialApps/      # BaseAttackApp + attack subclasses
│   │       │       └── ddos/             # UDP/TCP/DNS/HTTP flood attacks
│   │       ├── trafficcontroller/        # DataTrafficController (orchestration)
│   │       └── simcommands/              # Post-simulation commands
│   ├── simulations/
│   │   └── CaseID/
│   │       ├── Test-cases-001/           # CBR + ONE Simulator scenarios
│   │       ├── Test-cases-002/           # Dynamic traffic + attack scenarios
│   │       ├── config/                   # Shared configuration files
│   │       ├── networks/                 # NED network definitions
│   │       └── script/                   # TC-001 run scripts
│   ├── modifiedExternalFiles/            # INET/Simu5G source patches (TC-001)
│   │   ├── inet4.5/
│   │   └── Simu5G/
│   └── ONE_Simulator/                    # D2D malware propagation simulator
└── archive/
    └── legacy-bootstrap/                 # Deprecated installation scripts
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
```

**Q: Python module import errors**
```bash
# Activate the virtual environment
cd omnetpp-6.0.1
source venv/bin/activate
```

## Opening in OMNeT++ IDE

```bash
# Launch OMNeT++ IDE
cd omnetpp-6.0.1
./bin/omnetpp

# In IDE: File → Open Projects from File System...
# Select: omnetpp-6.0.1/samples/DDoSimu5G
```

## Development

### Building from Source

```bash
cd omnetpp-6.0.1/samples/DDoSimu5G/src
make clean
make MODE=release
```

### Migration from bootstrap_install.sh

The old `bootstrap_install.sh` is deprecated. Use `setup.sh` for equivalent behavior, or the modular scripts for more control. See [docs/CHANGELOG.md](docs/CHANGELOG.md) for the migration guide.

## Documentation

- **[Technical Reference](docs/TECHNICAL_REFERENCE.md)** — Software architecture, configuration, and extension guide
- **[Changelog](docs/CHANGELOG.md)** — Version history and migration guide
- **[OMNeT++ Manual](https://doc.omnetpp.org/)** — Simulation framework documentation
- **[INET Framework](https://inet.omnetpp.org/)** — Network protocol documentation
- **[Simu5G](http://simu5g.org/)** — 5G simulation library

## Version Information

| Component | Version |
|-----------|---------|
| DDoSimu5G | v1.0.0-prep |
| OMNeT++ | 6.0.1 |
| INET | 4.5.0 |
| Simu5G | 1.2.2 |
| ONE Simulator | 1.6.0 |

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

If you use DDoSimu5G in your research, please cite the repository.

---

For questions, issues, or contributions, please open an issue at [GitHub](https://github.com/karkha-0/pub_DDoSimu5G/issues).
