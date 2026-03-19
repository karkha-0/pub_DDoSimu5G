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

That's it! The script will:
1. Install OMNeT++ 6.0.1, INET 4.5.0, and Simu5G 1.2.2
2. Set up the pub_DDoSimu5G project
3. Build all components

### Running Your First Simulation

```bash
# Navigate to simulation scripts
cd omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/script

# Run a test case
./runSim_TC-Base-DDoS-infec-5RAN-002.sh
```

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

## Installed Components

After installation, you will have:

- **OMNeT++ 6.0.1**: Network simulation framework (WITH_NETBUILDER enabled)
- **INET 4.5.0**: Internet protocol suite for OMNeT++
- **Simu5G 1.2.2**: 5G NR network simulation library
- **pub_DDoSimu5G**: This project with DDoS simulation capabilities
- **Python venv**: Virtual environment with `posix_ipc` for result analysis
- **ONE Simulator 1.6.0**: Opportunistic Network Environment (optional)

Component versions are defined in `env-requirements.json`.

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
├── setup.sh                    # Master installation script
├── setup_environment.sh        # Environment-only installer
├── setup_project.sh           # Project-only installer
├── env-requirements.json      # Version requirements
├── CHANGELOG.md               # Version history
├── src/                       # Project source code
├── simulations/               # Simulation scenarios
│   └── CaseID/
│       └── script/           # Simulation run scripts
├── modifiedExternalFiles/    # Patches for INET/Simu5G
├── ONE_Simulator/            # Mobility trace generator
└── results/                  # Analysis notebooks and data
```

## Crediting
  
This project is licensed under the MIT License. You are free to use, modify, and distribute this software for academic and non-commercial purposes, provided proper credit is given to the original authors.

If you use DDoSimu5G in your research, please cite the repository.


The simulation run scripts are located in `simulations/CaseID/script/`:

```bash
cd omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/script

# Run a DDoS infection scenario
./runSim_TC-Base-DDoS-infec-5RAN-002.sh

# Run a baseline scenario (no attack)
./runSim_TC-Base-benign-5RAN-001.sh
```

The scripts automatically:
- Source the OMNeT++ environment
- Set up library paths
- Run the simulation with proper parameters

**Note**: Modified source files from `modifiedExternalFiles/` are automatically applied during installation by `setup_project.sh`.

### Convert D2D Model mobility traces from ONE to Simu5G

If you need to generate new mobility traces (existing traces are pre-generated):

```bash
cd omnetpp-6.0.1/samples/DDoSimu5G/simulations/CaseID/script/mob_tracesConversionPy/fromONEtoSimu5G/

# Convert JSON mobility traces to .mobility format
python3 convert_mob_traces.py <path-to-json-mobility> <output.movements>
```

### Results and Visualization
  
All plots are reproducible using the Jupyter notebooks in `results/Test-Cases-001/plotting/Test_cases_plots/`:

```bash
# Activate Python environment (automatically created during setup)
cd omnetpp-6.0.1
source venv/bin/activate

# Install Jupyter if not already installed
pip install jupyter pandas matplotlib numpy

# Open Jupyter notebooks
cd samples/DDoSimu5G/results/Test-Cases-001/plotting/Test_cases_plots/
jupyter notebook
```

**Python packages installed by setup**:
- `posix_ipc`: For inter-process communication and shared memory operations

Use the pre-exported `.csv` files in `results/Test-Cases-001/plotting/raw_data/` or regenerate with OMNeT++ scavetool.

### Opening Project in OMNeT++ IDE

```bash
# Launch OMNeT++ IDE
cd omnetpp-6.0.1
./bin/omnetpp

# In IDE: File → Open Projects from File System...
# Select: omnetpp-6.0.1/samples/DDoSimu5G
```

**Configure NED file locations** (if needed):
- Right-click project → Properties → OMNeT++ → NED Source Folders
- Ensure all source directories are included

## Migration from bootstrap_install.sh

If you previously used `bootstrap_install.sh`:

1. The old script is now `bootstrap_install.old.sh` (deprecated)
2. Use `setup.sh` for the same one-command installation
3. Or use the modular scripts for more control
4. See `CHANGELOG.md` for detailed migration guide

## Development and Contributing

### Building from Source

```bash
# If you modify the project source
cd omnetpp-6.0.1/samples/DDoSimu5G/src
make clean
make MODE=release
```

### Running Tests

Test cases are defined as simulation configurations. See `simulations/CaseID/omnetpp.ini` for all scenarios.

## Support and Documentation

- **Issues**: [GitHub Issues](https://github.com/karkha-0/pub_DDoSimu5G/issues)
- **Documentation**: See inline comments and `CHANGELOG.md`
- **OMNeT++ Manual**: `omnetpp-6.0.1/doc/manual/index.html`
- **INET Documentation**: [INET Framework Docs](https://inet.omnetpp.org/)
- **Simu5G Documentation**: [Simu5G Website](http://simu5g.org/)

## Version Information

- **Current Version**: v1.0.0-prep (release candidate)
- **OMNeT++**: 6.0.1
- **INET**: 4.5.0
- **Simu5G**: 1.2.2
- **ONE Simulator**: 1.6.0

See `env-requirements.json` for complete version specifications.

---

For questions, issues, or contributions, please open an issue or contact the maintainers via GitHub.


