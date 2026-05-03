#!/bin/bash
# setup_project.sh
# Sets up pub_DDoSimu5G project in OMNeT++ environment
# Part of pub_DDoSimu5G project
# https://github.com/karkha-0/pub_DDoSimu5G

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default configuration
DEFAULT_REPO_URL="https://github.com/karkha-0/pub_DDoSimu5G.git"
PROJECT_REPO_URL="$DEFAULT_REPO_URL"
PROJECT_BRANCH="main"
OMNET_DIR=""
WITH_ONE=true  # Install ONE Simulator by default
FORCE=false
USE_LOCAL_SOURCE=true  # Use local DDoSimu5G/ if available (disabled when --repo-url specified)

# Logging functions
info() { echo -e "${GREEN}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }
die() { error "$*"; exit 1; }

show_help() {
  cat << EOF
Usage: $0 [OPTIONS]

Sets up pub_DDoSimu5G project in an existing OMNeT++ environment

OPTIONS:
  --omnet-dir DIR      OMNeT++ installation directory (auto-detected if not specified)
  --repo-url URL       Custom repository URL (forces git clone, ignores local source)
  --branch BRANCH      Git branch to checkout (default: main)
  --with-one           Install ONE Simulator (default: enabled)
  --without-one        Skip ONE Simulator installation
  --force              Force reinstallation even if already exists
  -h, --help           Show this help message

EXAMPLES:
  # Auto-detect OMNeT++ and use local source if available
  $0

  # Specify OMNeT++ location (still uses local source if available)
  $0 --omnet-dir ~/simulation/omnetpp-6.0.1

  # Force clone from specific repository (ignores local source)
  $0 --repo-url https://github.com/myuser/my-fork.git

  # Clone from specific branch
  $0 --repo-url https://github.com/karkha-0/pub_DDoSimu5G.git --branch dev

  # Skip ONE Simulator
  $0 --without-one

REQUIREMENTS:
  - OMNeT++ 6.0.1 environment (run setup_environment.sh first)
  - Java 11+ (for ONE Simulator, if --with-one)

EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --omnet-dir)
      OMNET_DIR="${2%/}"  # Remove trailing slash
      shift 2
      ;;
    --repo-url)
      PROJECT_REPO_URL="$2"
      USE_LOCAL_SOURCE=false  # Disable local source when explicit repo specified
      shift 2
      ;;
    --branch)
      PROJECT_BRANCH="$2"
      shift 2
      ;;
    --with-one)
      WITH_ONE=true
      shift
      ;;
    --without-one)
      WITH_ONE=false
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

# Find OMNeT++ installation
find_omnetpp() {
  info "Looking for OMNeT++ installation..."
  
  if [ -n "$OMNET_DIR" ]; then
    if [ ! -f "$OMNET_DIR/setenv" ]; then
      die "OMNeT++ not found at specified location: $OMNET_DIR"
    fi
    info "✓ Using OMNeT++ at: $OMNET_DIR"
    return
  fi
  
  # Auto-detect: check current directory and parent directories
  local search_dir="$(pwd)"
  for i in {1..3}; do
    if [ -f "$search_dir/omnetpp-6.0.1/setenv" ]; then
      OMNET_DIR="$search_dir/omnetpp-6.0.1"
      info "✓ Found OMNeT++ at: $OMNET_DIR"
      return
    fi
    search_dir="$(dirname "$search_dir")"
  done
  
  # Check common locations
  for dir in ~/omnetpp-6.0.1 ~/simulation/omnetpp-6.0.1 /opt/omnetpp-6.0.1; do
    if [ -f "$dir/setenv" ]; then
      OMNET_DIR="$dir"
      info "✓ Found OMNeT++ at: $OMNET_DIR"
      return
    fi
  done
  
  die "OMNeT++ installation not found. Please run setup_environment.sh first or specify --omnet-dir"
}

# Check environment compatibility
check_env_compatibility() {
  info "Checking environment compatibility..."
  
  local env_info_file="$OMNET_DIR/.env-info.json"
  if [ ! -f "$env_info_file" ]; then
    warn "Environment info file not found: $env_info_file"
    warn "This environment may not have been set up with setup_environment.sh"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
      die "Installation cancelled"
    fi
    return
  fi
  
  # Simple JSON parsing (no jq dependency)
  local omnet_ver
  local inet_ver
  local simu5g_ver
  
  # Extract only from "versions" section, not "paths" section
  omnet_ver=$(grep -A 5 '"versions"' "$env_info_file" | grep -o '"omnetpp": *"[^"]*"' | sed 's/"omnetpp": *"\([^"]*\)"/\1/' | tr -d '\n\r' | xargs)
  inet_ver=$(grep -A 5 '"versions"' "$env_info_file" | grep -o '"inet": *"[^"]*"' | sed 's/"inet": *"\([^"]*\)"/\1/' | tr -d '\n\r' | xargs)
  simu5g_ver=$(grep -A 5 '"versions"' "$env_info_file" | grep -o '"simu5g": *"[^"]*"' | sed 's/"simu5g": *"\([^"]*\)"/\1/' | tr -d '\n\r' | xargs)
  
  info "Detected environment versions:"
  info "  - OMNeT++: $omnet_ver"
  info "  - INET: $inet_ver"
  info "  - Simu5G: $simu5g_ver"
  
  # Check if versions match requirements (basic check)
  local version_ok=true
  
  # Debug: show what we're comparing
  # info "DEBUG: Comparing '$omnet_ver' with '6.0.1'"
  
  if [ "$omnet_ver" != "6.0.1" ]; then
    warn "OMNeT++ version mismatch: expected 6.0.1, found $omnet_ver"
    version_ok=false
  fi
  
  if [ "$inet_ver" != "4.5.0" ]; then
    warn "INET version mismatch: expected 4.5.0, found $inet_ver"
    version_ok=false
  fi
  
  if [ "$simu5g_ver" != "1.2.2" ]; then
    warn "Simu5G version mismatch: expected 1.2.2, found $simu5g_ver"
    version_ok=false
  fi
  
  if [ "$version_ok" = true ]; then
    info "✓ Environment compatibility check passed"
  else
    warn "Version mismatches detected - proceeding with caution"
  fi
}

# Detect project source directory structure
# Returns the path to actual project source (handles both old flat and new DDoSimu5G/ subdirectory structure)
get_project_source_dir() {
  local project_dir="$OMNET_DIR/samples/DDoSimu5G"
  
  # Check if using new structure (DDoSimu5G/ subdirectory in cloned repo)
  if [ -d "$project_dir/DDoSimu5G/src" ]; then
    echo "$project_dir/DDoSimu5G"
  else
    # Legacy flat structure
    echo "$project_dir"
  fi
}

# Clone project
clone_project() {
  info "Setting up pub_DDoSimu5G project..."
  
  local project_dir="$OMNET_DIR/samples/DDoSimu5G"
  
  if [ -d "$project_dir" ] && [ "$FORCE" != true ]; then
    info "✓ Project already exists at $project_dir (use --force to reinstall)"
    return 0
  fi
  
  if [ -d "$project_dir" ] && [ "$FORCE" = true ]; then
    info "Removing existing project..."
    rm -rf "$project_dir"
  fi
  
  # Ensure samples directory exists
  mkdir -p "$OMNET_DIR/samples" || die "Failed to create samples directory"
  
  # Check for local DDoSimu5G/ directory (from GitHub release download)
  # Only use if --repo-url was NOT specified (i.e., USE_LOCAL_SOURCE=true)
  if [ "$USE_LOCAL_SOURCE" = true ]; then
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local local_source="$script_dir/DDoSimu5G"
    
    if [ -d "$local_source/src" ]; then
      info "Found local DDoSimu5G source (from release download)"
      info "Moving to: $project_dir"
      mv "$local_source" "$project_dir" || die "Failed to move local source"
      info "✓ Project installed from local source (offline-capable)"
      return 1  # Return 1 to indicate fresh install (need to build)
    fi
  fi
  
  # No local source or --repo-url specified: clone from repository
  cd "$OMNET_DIR/samples" || die "Failed to navigate to samples directory"
  
  info "Cloning project from: $PROJECT_REPO_URL"
  info "Branch: $PROJECT_BRANCH"
  git clone --branch "$PROJECT_BRANCH" "$PROJECT_REPO_URL" DDoSimu5G_temp || die "Failed to clone project"
  
  # Detect repository structure and copy accordingly
  if [ -d "DDoSimu5G_temp/DDoSimu5G" ]; then
    info "Detected new repository structure (DDoSimu5G/ subdirectory)"
    mv "DDoSimu5G_temp/DDoSimu5G" "DDoSimu5G"
    rm -rf "DDoSimu5G_temp"
  else
    info "Detected legacy repository structure (flat)"
    mv "DDoSimu5G_temp" "DDoSimu5G"
  fi
  
  info "✓ Project cloned to $project_dir"
  
  return 1  # Return 1 to indicate fresh clone (need to build)
}

# Apply modified files
apply_modifications() {
  info "Applying modifications to INET and Simu5G..."
  
  local project_dir="$OMNET_DIR/samples/DDoSimu5G"
  local project_source_dir="$(get_project_source_dir)"
  local modified_source="$project_source_dir/modifiedExternalFiles"
  
  if [ ! -d "$modified_source" ]; then
    warn "No modifications directory found at $modified_source"
    return
  fi
  
  # CRITICAL: Use inet4.5 directory name to match setup_environment.sh convention
  local inet_target="$OMNET_DIR/samples/inet4.5"
  local simu5g_target="$OMNET_DIR/samples/Simu5G"
  
  # Apply INET modifications
  if [ -d "$modified_source/inet4.5" ] && [ -d "$inet_target" ]; then
    info "Applying INET modifications..."
    local count=0
    while IFS= read -r -d '' file; do
      local rel_path="${file#$modified_source/inet4.5/}"
      local target_path="${rel_path%.cpy}"
      local target_file="$inet_target/$target_path"
      
      # Backup original if exists
      if [ -f "$target_file" ]; then
        cp "$target_file" "${target_file}.orig.$(date +%s)" 2>/dev/null || true
      fi
      
      mkdir -p "$(dirname "$target_file")"
      cp "$file" "$target_file"
      count=$((count + 1))
    done < <(find "$modified_source/inet4.5" -type f -name "*.cpy" -print0)
    
    if [ $count -gt 0 ]; then
      info "  ✓ Applied $count INET modifications"
    fi
  fi
  
  # Apply Simu5G modifications
  if [ -d "$modified_source/Simu5G" ] && [ -d "$simu5g_target" ]; then
    info "Applying Simu5G modifications..."
    local count=0
    while IFS= read -r -d '' file; do
      local rel_path="${file#$modified_source/Simu5G/}"
      local target_path="${rel_path%.cpy}"
      
      # Handle version-specific path transformations for v1.2.2
      # Note: Simu5G v1.2.2 uses the following structure:
      #   - src/stack/phy/layer/  (layer subdirectory is preserved)
      #   - src/nodes/NR/         (NR subdirectory is preserved)
      # No path transformations needed - use paths as-is from modifiedExternalFiles
      
      local target_file="$simu5g_target/$target_path"
      
      # Backup original if exists
      if [ -f "$target_file" ]; then
        cp "$target_file" "${target_file}.orig.$(date +%s)" 2>/dev/null || true
      fi
      
      mkdir -p "$(dirname "$target_file")"
      cp "$file" "$target_file"
      count=$((count + 1))
    done < <(find "$modified_source/Simu5G" -type f -name "*.cpy" -print0)
    
    if [ $count -gt 0 ]; then
      info "  ✓ Applied $count Simu5G modifications"
    fi
  fi
  
  info "✓ Modifications applied"
}

# Build project
build_project() {
  info "Building pub_DDoSimu5G project..."
  
  local project_dir="$OMNET_DIR/samples/DDoSimu5G"
  local project_source_dir="$(get_project_source_dir)"
  
  if [ ! -d "$project_source_dir" ]; then
    die "Project source directory not found: $project_source_dir"
  fi
  
  cd "$project_source_dir"
  
  # Source OMNeT++ environment
  # shellcheck disable=SC1091
  set +u
  source "$OMNET_DIR/setenv"
  set -u
  
  # Check if already built
  if [ -f "src/libDDoSimu5G.so" ] && [ "$FORCE" != true ]; then
    info "✓ Project already built (use --force to rebuild)"
    return
  fi
  
  # Clean old build artifacts
  info "Cleaning old build artifacts..."
  rm -rf out/
  find . -name "*.o" -delete 2>/dev/null || true
  find . -name "Makefile" -not -path "*/src/Makefile" -delete 2>/dev/null || true
  
  # Generate makefiles
  info "Generating project makefiles..."
  cd src
  # Locate nlohmann/json.hpp - prefer system install, fall back to bundled copy
  local nlohmann_inc
  if [ -d "/usr/include/nlohmann" ]; then
    nlohmann_inc="/usr/include"
  elif [ -d "/usr/local/include/nlohmann" ]; then
    nlohmann_inc="/usr/local/include"
  else
    # Bundle single-header as fallback
    warn "nlohmann/json.hpp not found as system package — downloading single-header fallback"
    mkdir -p ../include/nlohmann
    wget -q -O ../include/nlohmann/json.hpp \
      https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
      || die "Failed to download nlohmann/json.hpp"
    nlohmann_inc="$(cd ../include && pwd)"
  fi

  opp_makemake -f --make-so --deep -o DDoSimu5G -O ../out \
    -KINET4_5_PROJ=../../inet4.5 \
    -KSIMU5G_PROJ=../../Simu5G \
    -DINET_IMPORT \
    -I. \
    -I"$nlohmann_inc" \
    -I'$(INET4_5_PROJ)/src' -L'$(INET4_5_PROJ)/src' -lINET'$(D)' \
    -I'$(SIMU5G_PROJ)/src' -L'$(SIMU5G_PROJ)/src' -lsimu5g'$(D)' \
    || die "Makefile generation failed"
  
  # Build (stay in src directory where Makefile is)
  info "Building project library (this may take 5-10 minutes)..."
  make -j"$(nproc)" MODE=release || die "Project build failed"
  cd ..
  
  # Verify build
  if [ -f "src/libDDoSimu5G.so" ]; then
    local size
    size=$(ls -lh src/libDDoSimu5G.so | awk '{print $5}')
    info "✓ Project library built: src/libDDoSimu5G.so ($size)"
  else
    die "Build succeeded but library not found"
  fi
}

# Setup ONE Simulator
setup_one() {
  if [ "$WITH_ONE" != true ]; then
    info "Skipping ONE Simulator setup (--without-one)"
    return
  fi
  
  info "Setting up ONE Simulator..."
  
  local project_source_dir="$(get_project_source_dir)"
  local one_dir="$project_source_dir/ONE_Simulator/the-one-1.6.0"
  
  if [ ! -d "$one_dir" ]; then
    warn "ONE Simulator source not found at $one_dir"
    warn "This is optional - mobility traces are pre-generated"
    return
  fi
  
  # Check Java
  if ! command -v java >/dev/null 2>&1; then
    warn "Java not found. ONE Simulator compilation skipped."
    warn "Install Java 11+ if you need to regenerate mobility traces"
    return
  fi
  
  local java_ver
  java_ver=$(java -version 2>&1 | grep -i version | cut -d'"' -f2 | cut -d'.' -f1)
  if [ "$java_ver" -lt 11 ]; then
    warn "Java $java_ver found, but Java 11+ recommended for ONE Simulator"
  fi
  
  info "Compiling ONE Simulator with Java $java_ver..."
  cd "$one_dir"
  
  # Make scripts executable
  chmod +x *.sh *.bat 2>/dev/null || true
  
  # Compile using compile_new.bat (compatible with Java 11+)
  if [ -f "compile_new.bat" ]; then
    dos2unix compile_new.bat 2>/dev/null || true
    chmod +x compile_new.bat
    ./compile_new.bat >/dev/null 2>&1 || warn "ONE compilation warnings (non-fatal)"
    
    # Check if compiled successfully
    if [ -f "core/DTNSim.class" ]; then
      local class_count
      class_count=$(find . -name "*.class" 2>/dev/null | wc -l)
      info "✓ ONE Simulator compiled ($class_count .class files)"
    else
      warn "ONE Simulator compilation may have failed (this is non-fatal)"
    fi
  else
    warn "ONE Simulator compile script not found"
  fi
}

# Main setup
main() {
  info "=========================================="
  info "pub_DDoSimu5G Project Setup"
  info "=========================================="
  info "Repository: $PROJECT_REPO_URL"
  info "Branch: $PROJECT_BRANCH"
  info "ONE Simulator: $([ "$WITH_ONE" = true ] && echo "enabled" || echo "disabled")"
  info "=========================================="
  echo
  
  find_omnetpp
  check_env_compatibility
  
  # Clone returns 0 if exists, 1 if fresh clone
  if clone_project; then
    # Project already exists, check if we need to rebuild
    if [ "$FORCE" = true ]; then
      apply_modifications
      build_project
    fi
  else
    # Fresh clone, need to build
    apply_modifications
    build_project
  fi
  
  setup_one
  
  echo
  info "=========================================="
  info "✓ Project Setup Complete!"
  info "=========================================="
  info "Project location: $OMNET_DIR/samples/DDoSimu5G"
  info ""
  info "To run simulations:"
  info "  1. cd $OMNET_DIR/samples/DDoSimu5G/simulations/CaseID/script"
  info "  2. ./runSim_TC-Base-DDoS-infec-5RAN-002.sh"
  info ""
  info "The simulation scripts will automatically set up the environment."
  info "=========================================="
}

main "$@"
