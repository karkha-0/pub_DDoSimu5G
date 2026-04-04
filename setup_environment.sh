#!/bin/bash
# setup_environment.sh
# Sets up OMNeT++ 6.0.1, INET 4.5.0, and Simu5G 1.2.2 environment
# Part of pub_DDoSimu5G project
# https://github.com/karkha-0/pub_DDoSimu5G

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration - Hardcoded stable versions
OMNET_VERSION="6.0.1"
INET_VERSION="4.5.0"
# CRITICAL: INET directory MUST be "inet4.5" (not "inet4.5.0") to match bootstrap convention
# Simu5G makefiles and relative paths expect "../inet4.5" hardcoded
# Changing this will break Simu5G compilation with "cannot resolve import" errors
INET_DIR_NAME="inet4.5"  # Directory name (without patch version)
SIMU5G_VERSION="1.2.2"

OMNET_URL="https://github.com/omnetpp/omnetpp/releases/download/omnetpp-${OMNET_VERSION}/omnetpp-${OMNET_VERSION}-linux-x86_64.tgz"
INET_REPO="https://github.com/inet-framework/inet.git"
SIMU5G_REPO="https://github.com/Unipisa/Simu5G.git"

# Default settings
SKIP_PACKAGES=false
FORCE=false
INSTALL_DIR="$(pwd)"

# Logging functions
info() { echo -e "${GREEN}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }
die() { error "$*"; exit 1; }

show_help() {
  cat << EOF
Usage: $0 [OPTIONS]

Sets up OMNeT++ ${OMNET_VERSION}, INET ${INET_VERSION}, and Simu5G ${SIMU5G_VERSION}

OPTIONS:
  --install-dir DIR    Installation directory (default: current directory)
  --skip-packages      Skip system package installation (assumes already installed)
  --force              Force reinstallation even if already exists
  -h, --help           Show this help message

EXAMPLES:
  # Install in current directory
  $0

  # Install in specific directory
  $0 --install-dir ~/simulation

  # Skip package installation (if already done)
  $0 --skip-packages

REQUIREMENTS:
  - Disk space: 20GB
  - RAM: 8GB minimum, 16GB recommended
  - Internet connection
  - Ubuntu 20.04+, Debian 11+, Fedora 35+, or RHEL 8+

EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --install-dir)
      INSTALL_DIR="${2%/}"  # Remove trailing slash
      shift 2
      ;;
    --skip-packages)
      SKIP_PACKAGES=true
      shift
      ;;
    --force)
      FORCE=true
      shift
      ;;
    -h|--help)
      show_help
      exit 0
      ;;
    *)
      error "Unknown option: $1"
      show_help
      exit 1
      ;;
  esac
done

# Ensure install directory is absolute path
INSTALL_DIR="$(cd "$INSTALL_DIR" && pwd)"
OMNET_DIR="$INSTALL_DIR/omnetpp-${OMNET_VERSION}"

info "Installation directory: $INSTALL_DIR"
info "OMNeT++ will be installed at: $OMNET_DIR"

# Check system requirements
check_system_requirements() {
  info "Checking system requirements..."
  
  # Check disk space (need at least 20GB)
  available_space=$(df -BG "$INSTALL_DIR" | tail -1 | awk '{print $4}' | sed 's/G//')
  if [ "$available_space" -lt 20 ]; then
    warn "Low disk space: ${available_space}GB available, 20GB recommended"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
      die "Installation cancelled"
    fi
  fi
  
  # Check RAM
  total_ram=$(free -g | awk '/^Mem:/{print $2}')
  if [ "$total_ram" -lt 8 ]; then
    warn "Low RAM: ${total_ram}GB detected, 8GB minimum recommended"
    warn "Compilation may be slow or fail. Consider using --skip-packages if packages are already installed."
  fi
  
  info "✓ System requirements check passed"
}

# Detect package manager
detect_pkg_mgr() {
  if command -v apt-get >/dev/null 2>&1; then
    echo "apt"
  elif command -v dnf >/dev/null 2>&1; then
    echo "dnf"
  elif command -v yum >/dev/null 2>&1; then
    echo "yum"
  else
    echo "unknown"
  fi
}

# Install system packages
install_system_packages() {
  if [ "$SKIP_PACKAGES" = true ]; then
    info "Skipping system package installation (--skip-packages)"
    return
  fi
  
  local mgr
  mgr=$(detect_pkg_mgr)
  info "Installing system packages using: $mgr"
  
  case "$mgr" in
    apt)
      sudo apt-get update
      sudo apt-get install -y \
        build-essential git wget curl unzip \
        python3 python3-venv python3-pip \
        bison flex \
        libxml2-dev zlib1g-dev \
        nlohmann-json3-dev \
        xdg-utils \
        || die "Failed to install packages"
      # Qt6 is only available on Ubuntu 22.04+ — install if available, skip silently if not
      sudo apt-get install -y qt6-base-dev qt6-tools-dev qt6-tools-dev-tools 2>/dev/null || true
      ;;
    dnf)
      sudo dnf install -y \
        @development-tools git wget curl unzip \
        python3 python3-pip \
        bison flex \
        libxml2-devel zlib-devel \
        json-devel \
        xdg-utils \
        || warn "Some packages may have failed to install"
      ;;
    yum)
      sudo yum groupinstall -y 'Development Tools' || true
      sudo yum install -y \
        git wget curl unzip \
        python3 python3-pip \
        bison flex \
        libxml2-devel zlib-devel \
        xdg-utils \
        || warn "Some packages may have failed to install"
      ;;
    *)
      warn "Unknown package manager. Please install required packages manually:"
      warn "  - build-essential, git, wget, curl, python3, bison, flex"
      warn "  - build-essential, libxml2-dev, zlib-dev (Qt not needed - headless/cmdenv mode)"
      ;;
  esac
  
  info "✓ System packages installed"
}

# Install OMNeT++
install_omnetpp() {
  info "Installing OMNeT++ ${OMNET_VERSION}..."
  
  if [ -d "$OMNET_DIR" ] && [ "$FORCE" != true ]; then
    info "✓ OMNeT++ already exists at $OMNET_DIR (use --force to reinstall)"
    return
  fi
  
  if [ -d "$OMNET_DIR" ] && [ "$FORCE" = true ]; then
    info "Removing existing OMNeT++ installation..."
    rm -rf "$OMNET_DIR"
  fi
  
  # Download
  local tmp_dir
  tmp_dir=$(mktemp -d)
  info "Downloading OMNeT++ from $OMNET_URL"
  wget -q --show-progress -O "$tmp_dir/omnetpp.tgz" "$OMNET_URL" || die "Failed to download OMNeT++"
  
  # Extract
  info "Extracting OMNeT++..."
  mkdir -p "$INSTALL_DIR"
  tar xzf "$tmp_dir/omnetpp.tgz" -C "$INSTALL_DIR" || die "Failed to extract OMNeT++"
  rm -rf "$tmp_dir"
  
  cd "$OMNET_DIR"
  
  # Source environment
  # shellcheck disable=SC1091
  set +u
  source ./setenv
  set -u
  
  # Setup Python virtual environment for OMNeT++
  if [ ! -d "$INSTALL_DIR/venv" ]; then
    info "Creating Python virtual environment..."
    python3 -m venv "$INSTALL_DIR/venv"
  fi
  
  # shellcheck disable=SC1091
  set +u
  source "$INSTALL_DIR/venv/bin/activate"
  set -u
  
  # Install Python requirements
  info "Installing Python packages for OMNeT++..."

  # Check Python version - setuptools>=58 and pip>=21 dropped Python 3.5 support.
  # Ubuntu 16.04 ships Python 3.5; Ubuntu 20.04+ ships 3.8+.
  if python3 -c "import sys; exit(0 if sys.version_info >= (3, 6) else 1)" 2>/dev/null; then
    pip install -q --upgrade pip wheel setuptools || warn "Failed to upgrade pip tools"
  else
    warn "Python < 3.6 detected — pinning pip/setuptools to versions compatible with Python 3.5"
    warn "Consider upgrading the base OS to Ubuntu 22.04 for full functionality"
    pip install -q "pip<21" "setuptools<58" "wheel<0.37" || warn "Failed to install pinned pip tools"
  fi

  # Install packages required by OMNeT++ IDE and analysis tools
  pip install -q numpy scipy pandas matplotlib posix_ipc dpkt || warn "Failed to install some Python packages"
  
  # Install additional requirements if file exists
  if [ -f ./python/requirements.txt ]; then
    pip install -q -r ./python/requirements.txt || warn "Failed to install additional Python requirements"
  fi
  
  # Stub out xdg-desktop-menu to suppress "no writable system menu directory found"
  # errors in headless/Docker environments (Code Ocean, CI, etc.)
  local xdg_stub_dir
  xdg_stub_dir=$(mktemp -d)
  printf '#!/bin/sh\nexit 0\n' > "$xdg_stub_dir/xdg-desktop-menu"
  printf '#!/bin/sh\nexit 0\n' > "$xdg_stub_dir/xdg-icon-resource"
  chmod +x "$xdg_stub_dir/xdg-desktop-menu" "$xdg_stub_dir/xdg-icon-resource"
  export PATH="$xdg_stub_dir:$PATH"

  # Auto-detect Qt6: only disable if not available (e.g. Ubuntu 20.04, headless/Docker)
  # On Ubuntu 22.04+ with Qt6 installed, OMNeT++ IDE will be built automatically
  local qtenv_flag=""
  if ! command -v qmake6 >/dev/null 2>&1 && ! command -v qmake >/dev/null 2>&1; then
    info "Qt6 not found — configuring OMNeT++ for headless/cmdenv mode (no IDE)"
    qtenv_flag="WITH_QTENV=no"
  else
    info "Qt6 found — OMNeT++ IDE (Qtenv) will be built"
  fi

  # Configure
  info "Configuring OMNeT++ (this may take a minute)..."
  cat > configure.user << 'EOF'
# Enable NED file loading (required for simulations)
WITH_NETBUILDER=yes

# Disable 3D visualization (not needed)
WITH_OSG=no
WITH_OSGEARTH=no
EOF

  export VIRTUAL_ENV="$INSTALL_DIR/venv"
  export WITH_OSG=no
  export WITH_OSGEARTH=no
  # Pass WITH_QTENV as export if needed — OMNeT++ configure checks for qmake before reading configure.user
  if [ -n "${qtenv_flag:-}" ]; then
    export WITH_QTENV=no
  fi
  ./configure || die "OMNeT++ configure failed"
  
  # Build — release mode only, skip bundled samples (they are not needed for simulations)
  info "Building OMNeT++ (this may take 10-20 minutes)..."
  make -j"$(nproc)" MODE=release base || die "OMNeT++ build failed"
  
  info "✓ OMNeT++ ${OMNET_VERSION} installed successfully"
}

# Clone and build INET
build_inet() {
  info "Setting up INET ${INET_VERSION}..."
  
  local inet_dir="$OMNET_DIR/samples/$INET_DIR_NAME"
  
  if [ -d "$inet_dir" ] && [ "$FORCE" != true ]; then
    info "✓ INET already exists at $inet_dir (use --force to reinstall)"
    return
  fi
  
  if [ -d "$inet_dir" ] && [ "$FORCE" = true ]; then
    info "Removing existing INET installation..."
    rm -rf "$inet_dir"
  fi
  
  # Source OMNeT++ environment
  # shellcheck disable=SC1091
  set +u
  source "$OMNET_DIR/setenv"
  set -u
  
  mkdir -p "$OMNET_DIR/samples"
  cd "$OMNET_DIR/samples"
  
  # Clone
  info "Cloning INET ${INET_VERSION}..."
  git clone --depth 1 --branch "v${INET_VERSION}" "$INET_REPO" "$INET_DIR_NAME" || die "Failed to clone INET"
  
  cd "$INET_DIR_NAME"
  
  # Source INET environment (sets inet_root variable)
  if [ -f setenv ]; then
    # shellcheck disable=SC1091
    set +u
    source setenv
    set -u
  fi
  # INET setenv overwrites PATH — re-assert OMNeT++ tools
  export PATH="$OMNET_DIR/bin:$PATH"
  
  # Build
  info "Building INET (this may take 15-30 minutes)..."
  make makefiles || die "INET makefile generation failed"
  make -j"$(nproc)" MODE=release || die "INET build failed"
  
  info "✓ INET ${INET_VERSION} built successfully"
}

# Clone and build Simu5G
build_simu5g() {
  info "Setting up Simu5G ${SIMU5G_VERSION}..."
  
  local simu5g_dir="$OMNET_DIR/samples/Simu5G"
  local inet_dir="$OMNET_DIR/samples/$INET_DIR_NAME"
  
  if [ -d "$simu5g_dir" ] && [ "$FORCE" != true ]; then
    info "✓ Simu5G already exists at $simu5g_dir (use --force to reinstall)"
    return
  fi
  
  if [ -d "$simu5g_dir" ] && [ "$FORCE" = true ]; then
    info "Removing existing Simu5G installation..."
    rm -rf "$simu5g_dir"
  fi
  
  # Source OMNeT++ environment
  # shellcheck disable=SC1091
  set +u
  source "$OMNET_DIR/setenv"
  set -u
  # Explicitly ensure OMNeT++ tools are in PATH (setenv may not export reliably in all shells)
  export PATH="$OMNET_DIR/bin:$PATH"
  
  mkdir -p "$OMNET_DIR/samples"
  cd "$OMNET_DIR/samples"
  
  # Clone
  info "Cloning Simu5G ${SIMU5G_VERSION}..."
  git clone --depth 1 --branch "v${SIMU5G_VERSION}" "$SIMU5G_REPO" Simu5G || die "Failed to clone Simu5G"
  
  cd Simu5G
  
  # CRITICAL: Set INET_ROOT environment variable (required by Simu5G makefiles)
  export INET_ROOT="$inet_dir"
  info "Set INET_ROOT=$INET_ROOT"
  
  # Source INET environment (needed for Simu5G build)
  if [ -f "$inet_dir/setenv" ]; then
    info "Sourcing INET environment from $inet_dir/setenv"
    # shellcheck disable=SC1091
    set +u
    pushd "$inet_dir" >/dev/null
    . setenv -f 2>/dev/null || source setenv 2>/dev/null || warn "INET setenv failed (non-fatal)"
    popd >/dev/null
    set -u
    info "✓ INET environment sourced (INET_ROOT: ${INET_ROOT})"
  else
    warn "INET setenv not found at $inet_dir/setenv"
  fi
  
  # Source Simu5G setenv if it exists
  if [ -f "setenv" ]; then
    info "Sourcing Simu5G environment"
    # shellcheck disable=SC1091
    set +u
    . setenv -f 2>/dev/null || source setenv 2>/dev/null || warn "Simu5G setenv failed (non-fatal)"
    set -u
    info "✓ Simu5G environment sourced"
  fi
  # All setenv scripts may overwrite PATH — re-assert OMNeT++ tools
  export PATH="$OMNET_DIR/bin:$PATH"
  
  # Configure Simu5G features to use INET
  if [ ! -f ".oppfeaturestate" ]; then
    info "Configuring Simu5G features..."
    opp_featuretool enable VoIPStream 2>/dev/null || true
  fi
  
  # Configure Makefile to point to INET
  info "Configuring Simu5G Makefile..."
  if [ -f Makefile ]; then
    sed -i "s|^INET_PROJ=.*|INET_PROJ=../inet${INET_VERSION}|g" Makefile
  fi
  
  # Build
  info "Building Simu5G (this may take 10-20 minutes)..."
  
  # Try make makefiles first (uses .oppfeatures)
  if ! make makefiles 2>/dev/null; then
    warn "make makefiles failed, trying opp_makemake..."
    # Fallback: use opp_makemake with explicit INET paths
    cd src
    opp_makemake --make-so --deep -o simu5g -O ../out \
      -KINET_PROJ="$inet_dir" \
      -DINET_IMPORT \
      -I"$inet_dir/src" \
      -L"$inet_dir/src" \
      -lINET'$(D)' || die "opp_makemake failed"
    cd ..
  fi
  
  make -j"$(nproc)" MODE=release || die "Simu5G build failed"
  
  info "✓ Simu5G ${SIMU5G_VERSION} built successfully"
}

# Write environment info
write_env_info() {
  local env_info_file="$OMNET_DIR/.env-info.json"
  
  info "Writing environment info to $env_info_file"
  
  cat > "$env_info_file" << EOF
{
  "created": "$(date -Iseconds)",
  "versions": {
    "omnetpp": "${OMNET_VERSION}",
    "inet": "${INET_VERSION}",
    "simu5g": "${SIMU5G_VERSION}"
  },
  "paths": {
    "omnetpp": "${OMNET_DIR}",
    "inet": "${OMNET_DIR}/samples/${INET_DIR_NAME}",
    "simu5g": "${OMNET_DIR}/samples/Simu5G"
  }
}
EOF
  
  info "✓ Environment info written"
}

# Main installation
main() {
  info "=========================================="
  info "OMNeT++ + INET + Simu5G Environment Setup"
  info "=========================================="
  info "Versions:"
  info "  - OMNeT++: ${OMNET_VERSION}"
  info "  - INET: ${INET_VERSION}"
  info "  - Simu5G: ${SIMU5G_VERSION}"
  info "=========================================="
  echo
  
  check_system_requirements
  install_system_packages
  install_omnetpp
  build_inet
  build_simu5g
  write_env_info
  
  echo
  info "=========================================="
  info "✓ Environment Setup Complete!"
  info "=========================================="
  info "OMNeT++ installed at: $OMNET_DIR"
  info ""
  info "To use OMNeT++, source the environment:"
  info "  source $OMNET_DIR/setenv"
  info ""
  info "Next steps:"
  info "  1. Set up your project with setup_project.sh"
  info "  2. Or manually clone your project to $OMNET_DIR/samples/"
  info "=========================================="
}

main "$@"
