#!/bin/bash
# setup.sh
# Master setup script for pub_DDoSimu5G
# Runs both environment and project setup
# Part of pub_DDoSimu5G project
# https://github.com/karkha-0/pub_DDoSimu5G

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
info() { echo -e "${GREEN}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }
die() { error "$*"; exit 1; }

show_help() {
  cat << EOF
Usage: $0 [OPTIONS]

Complete setup of pub_DDoSimu5G simulation environment and project

This is a convenience wrapper that runs both:
  1. setup_environment.sh - Installs OMNeT++, INET, Simu5G
  2. setup_project.sh - Sets up pub_DDoSimu5G project

COMMON OPTIONS:
  --install-dir DIR    Installation directory (default: current directory)
  --repo-url URL       Custom repository URL
  --with-one           Install ONE Simulator (default: enabled)
  --without-one        Skip ONE Simulator installation
  --force              Force reinstallation even if already exists
  -h, --help           Show this help message

ENVIRONMENT-SPECIFIC OPTIONS:
  --skip-packages      Skip system package installation (if already installed)

PROJECT-SPECIFIC OPTIONS:
  --branch BRANCH      Git branch to checkout (default: main)

EXAMPLES:
  # Complete installation with defaults
  $0

  # Install to specific directory
  $0 --install-dir ~/simulation

  # Use custom fork without ONE Simulator
  $0 --repo-url https://github.com/myuser/my-fork.git --without-one

  # Force reinstallation
  $0 --force

REQUIREMENTS:
  - Ubuntu 20.04+ / Debian 11+ / Fedora 35+ / RHEL 8+
  - 20GB free disk space
  - 8GB RAM (16GB recommended)
  - Internet connection
  - sudo access (for package installation)

DURATION:
  First-time installation typically takes 30-45 minutes:
  - Environment setup: 25-35 minutes
  - Project setup: 5-10 minutes

For more control, you can run the individual scripts:
  - ./setup_environment.sh --help
  - ./setup_project.sh --help

EOF
}

# Check if we're in the right place
check_location() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  
  if [ ! -f "$script_dir/setup_environment.sh" ]; then
    die "setup_environment.sh not found in $script_dir"
  fi
  
  if [ ! -f "$script_dir/setup_project.sh" ]; then
    die "setup_project.sh not found in $script_dir"
  fi
}

# Parse arguments and separate into environment vs project args
ENV_ARGS=()
PROJECT_ARGS=()
INSTALL_DIR=""

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      show_help
      exit 0
      ;;
    --install-dir)
      INSTALL_DIR="$2"
      ENV_ARGS+=("$1" "$2")
      shift 2
      ;;
    --skip-packages)
      ENV_ARGS+=("$1")
      shift
      ;;
    --repo-url)
      PROJECT_ARGS+=("$1" "$2")
      shift 2
      ;;
    --branch)
      PROJECT_ARGS+=("$1" "$2")
      shift 2
      ;;
    --with-one)
      PROJECT_ARGS+=("$1")
      shift
      ;;
    --without-one)
      PROJECT_ARGS+=("$1")
      shift
      ;;
    --force)
      ENV_ARGS+=("$1")
      PROJECT_ARGS+=("$1")
      shift
      ;;
    *)
      error "Unknown option: $1"
      show_help
      exit 1
      ;;
  esac
done

main() {
  info "=========================================="
  info "pub_DDoSimu5G Complete Setup"
  info "=========================================="
  info "This will install:"
  info "  1. OMNeT++ 6.0.1 simulation framework"
  info "  2. INET 4.5.0 network simulation library"
  info "  3. Simu5G 1.2.2 5G simulation library"
  info "  4. pub_DDoSimu5G project"
  info ""
  info "Estimated time: 30-45 minutes"
  info "=========================================="
  echo
  
  check_location
  
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  
  # Phase 1: Environment setup
  info "=========================================="
  info "Phase 1/2: Environment Setup"
  info "=========================================="
  echo
  
  bash "$script_dir/setup_environment.sh" "${ENV_ARGS[@]}" || die "Environment setup failed"
  
  echo
  info "=========================================="
  info "Phase 2/2: Project Setup"
  info "=========================================="
  echo
  
  # Determine OMNeT++ directory for project setup
  local omnet_dir
  if [ -n "$INSTALL_DIR" ]; then
    omnet_dir="$INSTALL_DIR/omnetpp-6.0.1"
  else
    omnet_dir="$(pwd)/omnetpp-6.0.1"
  fi
  
  # Add --omnet-dir to project args
  PROJECT_ARGS+=("--omnet-dir" "$omnet_dir")
  
  bash "$script_dir/setup_project.sh" "${PROJECT_ARGS[@]}" || die "Project setup failed"
  
  echo
  info "=========================================="
  info "✓✓✓ Complete Setup Finished! ✓✓✓"
  info "=========================================="
  info ""
  info "Installation location: $omnet_dir"
  info ""
  info "Next steps:"
  info "  1. Source the OMNeT++ environment:"
  info "     cd $omnet_dir"
  info "     source setenv"
  info ""
  info "  2. Run a simulation:"
  info "     cd samples/DDoSimu5G/simulations/CaseID/script"
  info "     ./runSim_TC-Base-DDoS-infec-5RAN-002.sh"
  info ""
  info "  3. Open in OMNeT++ IDE:"
  info "     cd $omnet_dir"
  info "     ./bin/omnetpp"
  info "     File > Open Projects... > samples/DDoSimu5G"
  info ""
  info "For help, see:"
  info "  - README.md in the project directory"
  info "  - https://github.com/karkha-0/pub_DDoSimu5G"
  info "=========================================="
}

main "$@"
