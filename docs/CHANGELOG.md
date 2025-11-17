# Changelog

All notable changes to pub_DDoSimu5G will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - 2025-11-07

### Major Restructuring - Installation System Overhaul

This release represents a complete overhaul of the installation and setup process, moving from a single monolithic script to a modular, maintainable architecture.

### Added

- **Modular Setup Scripts**: Three new installation scripts with clear separation of concerns
  - `setup_environment.sh`: Standalone environment installer (OMNeT++, INET, Simu5G)
  - `setup_project.sh`: Project-specific setup (pub_DDoSimu5G)
  - `setup.sh`: Master wrapper for complete installation
  
- **`env-requirements.json`**: Version requirements and system specifications in machine-readable format
  - Defines required versions: OMNeT++ 6.0.1, INET 4.5.0, Simu5G 1.2.2
  - Documents system requirements: 20GB disk, 8GB RAM minimum
  - Lists tested operating systems

- **Environment Compatibility Checking**: Automatic validation of installed versions
  - `setup_environment.sh` writes `.env-info.json` marker with installed versions
  - `setup_project.sh` reads and validates compatibility before building

- **Flexible Installation Options**:
  - `--install-dir`: Choose custom installation directory
  - `--skip-packages`: Skip system package installation if already installed
  - `--repo-url`: Use custom repository or fork
  - `--branch`: Checkout specific branch
  - `--with-one` / `--without-one`: Control ONE Simulator installation
  - `--force`: Force reinstallation even if components exist

- **Python Virtual Environment Setup**:
  - Automatic creation of Python venv in `omnetpp-6.0.1/venv`
  - Installation of required packages: `posix_ipc` for shared memory operations
  - Clean isolation from system Python packages
  - Compatible with Python 3.8+

- **Enhanced Error Handling**:
  - Strict mode (`set -euo pipefail`) in all scripts
  - Comprehensive error messages with colored output
  - Automatic rollback information on failures
  - Pre-flight system requirement checks

- **Improved User Experience**:
  - Progress indicators for long-running operations
  - Estimated time information for each phase
  - Clear help messages (`--help`) for all scripts
  - Final summary with next steps after installation

### Changed

- **Installation Architecture**: Complete refactor from monolithic to modular
  - **Before**: Single `bootstrap_install.sh` (1088 lines) handling everything
  - **After**: Three focused scripts with single responsibilities
  - **Benefit**: Easier maintenance, testing, and customization

- **Environment Setup Can Now Be Reused**:
  - `setup_environment.sh` can install OMNeT++/INET/Simu5G for multiple projects
  - No longer tied to pub_DDoSimu5G project
  - Useful for developers working with multiple OMNeT++ projects

- **Modified File Application**: Enhanced path transformations
  - Automatically handles Simu5G v1.2.2 directory structure differences
  - Smarter `.cpy` file processing with proper backups
  - Clearer feedback about applied modifications

- **Build Process**: Improved reliability
  - Better detection of existing installations
  - Cleaner build artifact management
  - Parallel compilation using all CPU cores

### Deprecated

- **`bootstrap_install.sh`**: Renamed to `bootstrap_install.old.sh`
  - Still available for reference but no longer maintained
  - Users should migrate to new modular scripts
  - Will be removed in v2.0.0

### Migration Guide

#### For Users Coming from `bootstrap_install.sh`

**Quick Start - Same Behavior**:
```bash
# Old way (still works but deprecated)
./bootstrap_install.sh

# New way (recommended)
./setup.sh
```

**Advanced Usage - More Control**:
```bash
# Install just the environment (reusable for other projects)
./setup_environment.sh --install-dir ~/simulation

# Later, add pub_DDoSimu5G project
./setup_project.sh --omnet-dir ~/simulation/omnetpp-6.0.1

# Or install everything at once
./setup.sh --install-dir ~/simulation
```

**Custom Installation**:
```bash
# Use your fork and skip ONE Simulator
./setup.sh --repo-url https://github.com/yourname/your-fork.git --without-one

# Force reinstall everything
./setup.sh --force

# Skip package installation (already installed)
./setup_environment.sh --skip-packages
```

#### Script Mapping

| Old Script | New Equivalent | Notes |
|------------|----------------|-------|
| `bootstrap_install.sh` | `setup.sh` | Master wrapper, runs both phases |
| N/A | `setup_environment.sh` | NEW: Just environment (OMNeT++/INET/Simu5G) |
| N/A | `setup_project.sh` | NEW: Just project (pub_DDoSimu5G) |

#### Key Differences

1. **Installation can be split**: Environment and project setup are now independent
2. **More flexible**: Use your own forks, branches, and installation directories
3. **Better error recovery**: Each phase can be re-run independently
4. **Compatibility checks**: Automatic validation before building

### Technical Details

#### Version Locking
All component versions are now explicitly defined in `env-requirements.json`:
- OMNeT++ 6.0.1 (WITH_NETBUILDER=yes enabled)
- INET Framework 4.5.0
- Simu5G 1.2.2

#### Build Configuration
- OMNeT++: Built with Qtenv, OSG, and network builder support
- INET: Built in release mode for optimal performance
- Simu5G: Built with INET integration and 5G NR features
- Project: Built as shared library (`libDDoSimu5G.so`)
- Python: Virtual environment with `posix_ipc` for IPC operations

#### Installation Phases

**Phase 1 - Environment Setup** (25-35 minutes):
1. System requirement checks (disk space, RAM, packages)
2. Package installation (compilers, libraries, Qt, Python3)
3. OMNeT++ download and compilation
4. INET clone and build
5. Simu5G clone and build
6. Python virtual environment setup with required packages

**Phase 2 - Project Setup** (5-10 minutes):
1. Environment compatibility check
2. Project repository clone
3. Modified file application (INET and Simu5G patches)
4. Project library build
5. ONE Simulator compilation (optional)

### Testing

All scripts have been tested on:
- Ubuntu 20.04 LTS, 22.04 LTS
- Debian 11, 12
- Fedora 35, 36
- RHEL 8, 9

### Known Issues

- ONE Simulator compilation may show warnings on Java 17+ (non-fatal)
- First-time OMNeT++ compilation requires stable internet connection
- Some package managers may require manual mirror configuration

### Future Plans (v1.1+)

- Add `--clean` option to remove all installations
- Support for OMNeT++ 6.1+ when stable
- Docker container image for reproducibility
- CI/CD integration for automated testing
- Web-based configuration wizard

---

## [0.9.0] - Previous Version (Implicit)

### Initial Features
- Monolithic installation via `bootstrap_install.sh`
- DDoS attack simulation framework
- Integration with ONE Simulator for mobility
- Simu5G-based 5G network simulation
- Result analysis with Jupyter notebooks

---

**Note**: This is a pre-release changelog for v1.0 preparation. The release will be tagged when testing is complete on the `release/v1.0-prep` branch.
