#!/usr/bin/env bash
set -euo pipefail

# pub_DDoSimu5G bootstrap installer (project root)
# Enhanced: non-interactive flags, ONE source support, full build (OMNeT++/INET/Simu5G/project)
# Defaults: run in current working directory (where user invokes the script)

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
PWD_START="$(pwd)"
MODIFIED_DIR="$REPO_ROOT/modifiedExternalFiles"

# Defaults kept in deps.json (if present)
DEPS_FILE="$REPO_ROOT/deps.json"
OMNET_DEFAULT_VERSION="6.1.0"
INET_DEFAULT_REF="v4.5.4"  # Latest stable INET version
SIMU5G_DEFAULT_REF="Simu5G-1.3.0"
ONE_DEFAULT_REF="the-one-1.6.0"
JDK_DEFAULT="11"

# CLI defaults
ASSUME_YES=false
OMNET_DIR=""
OMNET_VER="${OMNET_DEFAULT_VERSION}"
WORKDIR="${PWD_START}"
INET_REF="$INET_DEFAULT_REF"
SIMU5G_REF="$SIMU5G_DEFAULT_REF"
ONE_SOURCE=""   # path or 'bundled' or git url
ONE_JAR=""
SKIP_OMNET=false
ANALYSIS_ONLY=false
FORCE=false

info(){ printf "[INFO] %s\n" "$*"; }
warn(){ printf "[WARN] %s\n" "$*"; }
err(){ printf "[ERROR] %s\n" "$*"; exit 1; }

confirm(){
  if [ "$ASSUME_YES" = true ] || [ "${CI:-}" = "true" ]; then
    return 0
  fi
  read -r -p "$1 [y/N]: " resp
  case "$resp" in
    [yY]|[yY][eE][sS]) return 0 ;;
    *) return 1 ;;
  esac
}

print_help(){
  echo "Usage: $0 [options]"
  echo "  -y, --yes                Assume yes to prompts (non-interactive)"
  echo "      --omnet-dir DIR       OMNeT++ install directory (default: PWD/omnetpp-<ver>)"
  echo "      --omnet-ver VER       OMNeT++ version/tag to download (default: ${OMNET_DEFAULT_VERSION})"
  echo "      --workdir DIR         Directory to clone/build INET/Simu5G/ONE (default: current working directory)"
  echo "      --inet-ref REF        INET tag/ref to checkout (default: ${INET_DEFAULT_REF})"
  echo "      --simu5g-ref REF      Simu5G tag/ref to checkout (default: ${SIMU5G_DEFAULT_REF})"
  echo "      --one-source PATH|bundled|giturl  Use ONE source from path, use bundled copy, or clone from giturl"
  echo "      --one-jar PATH        Use an existing ONE jar instead of source"
  echo "      --skip-omnet          Skip OMNeT++ download/build (assume preinstalled)"
  echo "      --analysis-only       Only setup Python environment and data analysis packages (skip OMNeT++/INET/Simu5G)"
  echo "      --force               Overwrite existing targets (use with care)"
  echo "  -h, --help               Show help"
}

parse_args(){
  while [ $# -gt 0 ]; do
    case "$1" in
      -y|--yes) ASSUME_YES=true; shift ;;
      --omnet-dir) OMNET_DIR="$2"; shift 2 ;;
      --omnet-ver) OMNET_VER="$2"; shift 2 ;;
      --workdir) WORKDIR="$2"; shift 2 ;;
      --inet-ref) INET_REF="$2"; shift 2 ;;
      --simu5g-ref) SIMU5G_REF="$2"; shift 2 ;;
      --one-source) ONE_SOURCE="$2"; shift 2 ;;
      --one-jar) ONE_JAR="$2"; shift 2 ;;
      --skip-omnet) SKIP_OMNET=true; shift ;;
      --analysis-only) ANALYSIS_ONLY=true; shift ;;
      --force) FORCE=true; shift ;;
      -h|--help) print_help; exit 0 ;;
      *) warn "Unknown arg: $1"; print_help; exit 1 ;;
    esac
  done
}

detect_pkg_mgr(){
  if command -v apt-get >/dev/null 2>&1; then
    echo apt
  elif command -v dnf >/dev/null 2>&1; then
    echo dnf
  elif command -v yum >/dev/null 2>&1; then
    echo yum
  else
    echo none
  fi
}

install_system_packages(){
  mgr=$(detect_pkg_mgr)
  info "Detected package manager: $mgr"
  if [ "$mgr" = "apt" ]; then
    sudo apt-get update
    sudo apt-get install -y build-essential git wget curl unzip python3 python3-venv python3-pip default-jdk rsync dos2unix \
      qtbase5-dev qtchooser qt5-qmake libqt5opengl5-dev libxml2-dev zlib1g-dev bison flex nlohmann-json3-dev pkg-config
  elif [ "$mgr" = "dnf" ]; then
    sudo dnf install -y @development-tools git wget curl unzip python3 python3-venv python3-pip java-11-openjdk rsync \
      qt5-qtbase-devel libxml2-devel zlib-devel bison flex nlohmann-json-devel pkgconfig || true
  elif [ "$mgr" = "yum" ]; then
    sudo yum groupinstall -y 'Development Tools' || true
    sudo yum install -y git wget curl unzip python3 python3-venv python3-pip java-11-openjdk rsync \
      qt5-qtbase-devel libxml2-devel zlib-devel bison flex nlohmann-json-devel pkgconfig || true
  else
    warn "No supported package manager found. Please install required packages manually: build tools, git, wget, python3, python3-venv, python3-pip, java (JDK), rsync, Qt dev packages, pkg-config"
  fi
}

download_and_extract(){
  url="$1"
  destdir="$2"
  info "Downloading $url"
  tmp=$(mktemp -d)
  pushd "$tmp" >/dev/null
  wget -c "$url"
  archive=$(ls -1)
  mkdir -p "$destdir"
  info "Extracting $archive -> $destdir"
  case "$archive" in
    *.tar.gz|*.tgz) tar xzf "$archive" -C "$destdir" --strip-components=1 ;;
    *.zip) unzip -q "$archive" -d "$destdir" ;;
    *) warn "Unknown archive format: $archive" ;;
  esac
  popd >/dev/null
  rm -rf "$tmp"
}

install_omnetpp(){
  target_dir="$1"
  version="$2"
  if [ -z "$target_dir" ]; then
    err "install_omnetpp requires a target directory"
  fi
  
  # Check if OMNeT++ is already installed and built
  if [ -f "$target_dir/bin/opp_featuretool" ] && [ -f "$target_dir/bin/opp_run" ]; then
    info "✓ OMNeT++ already installed and built at $target_dir (skipping)"
    return
  fi
  
  if [ -d "$target_dir" ] && [ "$FORCE" != true ]; then
    warn "Target directory $target_dir exists but not fully built. Use --force to overwrite or set a different --omnet-dir. Skipping OMNeT++ install."
    return
  fi

  mkdir -p "$target_dir"
  # Use official OMNeT++ download URL from GitHub releases
  if [ "$version" = "${OMNET_DEFAULT_VERSION}" ]; then
    url="https://github.com/omnetpp/omnetpp/releases/download/omnetpp-${version}/omnetpp-${version}-linux-x86_64.tgz"
  else
    warn "Custom version specified. Attempting GitHub releases URL..."
    url="https://github.com/omnetpp/omnetpp/releases/download/omnetpp-${version}/omnetpp-${version}-linux-x86_64.tgz"
  fi

  download_and_extract "$url" "$target_dir"

  info "OMNeT++ pre-built package extracted to $target_dir"
  info "Verifying OMNeT++ installation..."
  
  # Check if bin directory has executables
  if [ -f "$target_dir/bin/opp_featuretool" ]; then
    info "✓ OMNeT++ pre-built binaries found"
  else
    warn "Pre-built binaries not found. Building OMNeT++ from source (this may take several minutes)..."
    
    # OMNeT++ requires Python packages - install them in the project venv if it exists
    if [ -d "$REPO_ROOT/tf_env/bin" ]; then
      info "Using project Python venv for OMNeT++ build"
      export PATH="$REPO_ROOT/tf_env/bin:$PATH"
    else
      warn "Project Python venv not found. OMNeT++ may require system Python packages."
    fi
    
    pushd "$target_dir" >/dev/null
    
    # Source setenv before configure (required by OMNeT++)
    if [ -f ./setenv ]; then
      set +u
      source ./setenv
      set -u
    fi
    
    # Install Python requirements if they exist
    if [ -f ./python/requirements.txt ]; then
      info "Installing OMNeT++ Python requirements..."
      python3 -m pip install -r ./python/requirements.txt || warn "Failed to install Python requirements"
    fi
    
    # Create configure.user to disable optional features
    info "Creating configure.user to disable OpenSceneGraph and osgEarth (not needed for simulations)"
    cat > configure.user << 'EOF'
# Disable 3D visualization features (not needed for simulations)
WITH_OSG=no
WITH_OSGEARTH=no
EOF
    
    if [ -x ./configure ]; then
      ./configure
      make -j"$(nproc)"
      info "✓ OMNeT++ built successfully"
    else
      err "OMNeT++ configure script not found. Installation may be incomplete."
    fi
    popd >/dev/null
  fi
}

clone_repo(){
  url="$1"
  dest="$2"
  ref="$3"
  if [ -d "$dest" ]; then
    if [ "$FORCE" = true ]; then
      rm -rf "$dest"
    else
      info "$dest already exists — skipping clone"
      return
    fi
  fi
  git clone "$url" "$dest"
  if [ -n "$ref" ]; then
    pushd "$dest" >/dev/null
    git fetch --all --tags || true
    git checkout "$ref" || true
    popd >/dev/null
  fi
}

copy_modified_files(){
  if [ ! -d "$MODIFIED_DIR" ]; then
    warn "No $MODIFIED_DIR directory found — skipping copy of modified external files."
    return
  fi

  inet_target="$WORKDIR/inet4.5"
  simu5g_target="$WORKDIR/Simu5G"

  do_copy(){
    src="$1"
    dst="$2"
    if [ ! -d "$src" ]; then
      warn "Source $src does not exist — skipping"
      return
    fi
    if [ ! -d "$dst" ]; then
      warn "Destination $dst does not exist — skipping"
      return
    fi
    info "Copying $src -> $dst (rsync, creating backups)"
    rsync -av --backup --suffix=".orig.$(date +%s)" --exclude='.git' "$src/" "$dst/"
  }

  if [ -d "$MODIFIED_DIR/inet4.5" ]; then
    info "Applying INET modifications"
    do_copy "$MODIFIED_DIR/inet4.5" "$inet_target"
  fi
  if [ -d "$MODIFIED_DIR/Simu5G" ]; then
    info "Applying Simu5G modifications"
    do_copy "$MODIFIED_DIR/Simu5G" "$simu5g_target"
  fi
}

build_inet_and_simu5g(){
  if [ -z "$OMNET_DIR" ]; then
    warn "OMNET_DIR not set; cannot source OMNeT++ setenv. Skipping build of INET/Simu5G."
    return
  fi
  
  if [ -d "$WORKDIR/inet4.5" ]; then
    # Check if INET is already built
    if [ -f "$WORKDIR/inet4.5/src/INET" ] || [ -f "$WORKDIR/inet4.5/src/libINET.so" ]; then
      info "✓ INET already built at $WORKDIR/inet4.5 (skipping build)"
    else
      info "Building INET in $WORKDIR/inet4.5"
      pushd "$WORKDIR/inet4.5" >/dev/null
      
      # Source OMNeT++ setenv first to get opp_* tools in PATH
      if [ -f "$OMNET_DIR/setenv" ]; then
        info "Sourcing OMNeT++ environment from $OMNET_DIR/setenv"
        # shellcheck disable=SC1090
        set +u
        source "$OMNET_DIR/setenv"
        set -u
        info "✓ OMNeT++ environment sourced. PATH includes: $(echo $PATH | grep -o '[^:]*omnetpp[^:]*' || echo 'NOT FOUND')"
        info "✓ Checking opp_featuretool: $(command -v opp_featuretool || echo 'NOT FOUND')"
      else
        warn "OMNeT++ setenv not found at $OMNET_DIR/setenv"
      fi
      
      # Source INET's setenv if it exists
      if [ -f "setenv" ]; then
        info "Sourcing INET environment from setenv"
        # shellcheck disable=SC1091
        set +u
        # Use -f flag to allow sourcing from non-interactive scripts
        . setenv -f 2>/dev/null || source setenv 2>/dev/null || warn "INET setenv failed (this is usually safe to ignore)"
        set -u
        info "✓ INET environment sourced"
      else
        info "No INET setenv found (not required)"
      fi
      
      make makefiles
      make -j"$(nproc)" || warn "INET make failed"
      popd >/dev/null
    fi
  else
    warn "INET not found at $WORKDIR/inet4.5 — skipping"
  fi

  if [ -d "$WORKDIR/Simu5G" ]; then
    # Check if Simu5G is already built
    if [ -f "$WORKDIR/Simu5G/src/simu5g" ] || [ -f "$WORKDIR/Simu5G/src/libsimu5g.so" ]; then
      info "✓ Simu5G already built at $WORKDIR/Simu5G (skipping build)"
    else
      info "Building Simu5G in $WORKDIR/Simu5G"
      pushd "$WORKDIR/Simu5G" >/dev/null
      
      # Source OMNeT++ setenv first to get opp_* tools in PATH
      if [ -f "$OMNET_DIR/setenv" ]; then
        info "Sourcing OMNeT++ environment from $OMNET_DIR/setenv"
        # shellcheck disable=SC1090
        set +u
        source "$OMNET_DIR/setenv"
        set -u
        info "✓ OMNeT++ environment sourced"
      else
        warn "OMNeT++ setenv not found at $OMNET_DIR/setenv"
      fi
      
      # Source INET setenv to get INET paths (required for Simu5G)
      if [ -f "$WORKDIR/inet4.5/setenv" ]; then
        info "Sourcing INET environment from $WORKDIR/inet4.5/setenv"
        # shellcheck disable=SC1090
        set +u
        pushd "$WORKDIR/inet4.5" >/dev/null
        . setenv -f 2>/dev/null || source setenv 2>/dev/null || warn "INET setenv failed"
        popd >/dev/null
        set -u
        info "✓ INET environment sourced (INET path: ${INET_ROOT:-NOT SET})"
      else
        warn "INET setenv not found - Simu5G build may fail"
      fi
      
      # Source Simu5G's setenv if it exists
      if [ -f "setenv" ]; then
        info "Sourcing Simu5G environment from setenv"
        # shellcheck disable=SC1091
        set +u
        # Use -f flag to allow sourcing from non-interactive scripts
        . setenv -f 2>/dev/null || source setenv 2>/dev/null || warn "Simu5G setenv failed (this is usually safe to ignore)"
        set -u
        info "✓ Simu5G environment sourced"
      else
        info "No Simu5G setenv found (not required)"
      fi
      
      # Create .oppfeaturestate if needed to link INET
      if [ ! -f ".oppfeaturestate" ] && [ -d "$WORKDIR/inet4.5" ]; then
        info "Configuring Simu5G to use INET from $WORKDIR/inet4.5"
        opp_featuretool enable VoIPStream 2>/dev/null || true
      fi
      
      make makefiles
      make -j"$(nproc)" || warn "Simu5G make failed"
      popd >/dev/null
    fi
  else
    warn "Simu5G not found at $WORKDIR/Simu5G — skipping"
  fi
}

setup_python_venv_and_requirements(){
  info "Setting up Python virtual environment and requirements"
  venv_default="$REPO_ROOT/tf_env"
  
  # Check if venv already exists and has packages
  if [ -d "$venv_default" ] && [ -f "$venv_default/bin/python" ]; then
    # Check if key packages are already installed
    if "$venv_default/bin/python" -c "import numpy, scipy, pandas, matplotlib" 2>/dev/null; then
      info "✓ Python venv already exists with required packages (skipping package installation)"
      return
    else
      info "Python venv exists but missing some packages, will install..."
    fi
  fi
  
  mapfile -t reqfiles < <(find "$REPO_ROOT" -type f -name requirements.txt || true)
  if [ ! -d "$venv_default" ]; then
    info "Creating python venv at $venv_default"
    python3 -m venv "$venv_default"
  fi
  pip_exec="$venv_default/bin/pip"
  "$pip_exec" install --upgrade pip setuptools wheel
  if [ ${#reqfiles[@]} -gt 0 ]; then
    for rf in "${reqfiles[@]}"; do
      info "Installing $rf into $venv_default"
      "$pip_exec" install -r "$rf" || warn "Some packages failed to install from $rf"
    done
  else
    info "No requirements.txt found; installing safe defaults into $venv_default"
    "$pip_exec" install numpy scipy pandas matplotlib jupyter tensorflow || true
  fi
}

build_project(){
  # Check if project is already built
  if [ -f "$REPO_ROOT/src/ddosim5g" ] || [ -f "$REPO_ROOT/src/libddosim5g.so" ] || [ -f "$REPO_ROOT/out/gcc-release/src/ddosim5g" ]; then
    info "✓ Project already built (skipping)"
    return
  fi
  
  if [ -f "$OMNET_DIR/setenv" ]; then
    # shellcheck disable=SC1090
    # Temporarily disable -u to avoid unbound variable errors in setenv
    set +u
    source "$OMNET_DIR/setenv"
    set -u
  fi
  info "Building pub_DDoSimu5G project"
  pushd "$REPO_ROOT" >/dev/null
  make -j"$(nproc)" || warn "Project make failed"
  popd >/dev/null
}

check_java(){
  info "Checking Java environment..."
  if ! command -v java >/dev/null 2>&1; then
    err "Java not found. Please install JDK ${JDK_DEFAULT} or higher"
  fi
  java_ver=$(java -version 2>&1 | grep -i version | cut -d'"' -f2 | cut -d'.' -f1)
  if [ "$java_ver" -lt "${JDK_DEFAULT}" ]; then
    err "Java ${JDK_DEFAULT} or higher required (found version $java_ver)"
  fi
  info "✓ Java version $java_ver found"
}

setup_one(){
  check_java
  
  # Prefer local bundled ONE in repo if present
  if [ -z "$ONE_SOURCE" ] || [ "$ONE_SOURCE" = "bundled" ]; then
    if [ -d "$REPO_ROOT/ONE_Simulator/the-one-1.6.0" ]; then
      info "Using bundled ONE source at $REPO_ROOT/ONE_Simulator/the-one-1.6.0"
      ONE_DIR="$REPO_ROOT/ONE_Simulator/the-one-1.6.0"
      
      # Setup ONE simulator
      info "Setting up ONE simulator environment"
      pushd "$ONE_DIR" >/dev/null
      
      # Make scripts executable
      chmod +x one.sh one_noGUI.sh || warn "Could not make ONE scripts executable"
      
      # Check if compile scripts exist and make them executable
      for script in compile.bat compile_new.bat compile_withWarning.bat; do
        if [ -f "$script" ]; then
          dos2unix "$script" 2>/dev/null || warn "dos2unix not found, scripts may have incorrect line endings"
          chmod +x "$script" || warn "Could not make $script executable"
        fi
      done
      
      # Compile ONE simulator using compile_new.bat (compatible with Java 21+, no warnings)
      if [ -f "compile_new.bat" ]; then
        info "Compiling ONE simulator with Java $(java -version 2>&1 | awk -F '"' '/version/ {print $2}')..."
        ./compile_new.bat
        
        if [ $? -eq 0 ]; then
          info "✓ ONE simulator compiled successfully"
        else
          warn "ONE simulator compilation failed. This is non-fatal - mobility traces are pre-generated."
          info "You can manually compile later or use the pre-generated traces."
        fi
      elif [ -f "compile_withWarning.bat" ]; then
        info "Using compile_withWarning.bat (will show deprecation warnings)..."
        ./compile_withWarning.bat
      elif [ -f "compile.bat" ]; then
        warn "Only compile.bat found (uses deprecated -extdirs). Please use compile_new.bat for Java 21+."
      else
        warn "ONE simulator compile scripts not found"
      fi
      
      popd >/dev/null
      return
    fi
  fi

  if [ -n "$ONE_SOURCE" ]; then
    if [ -d "$ONE_SOURCE" ]; then
      info "Using ONE source from $ONE_SOURCE"
      ONE_DIR="$ONE_SOURCE"
      return
    elif [[ "$ONE_SOURCE" =~ ^https?:// ]]; then
      # clone
      ONE_DIR="$WORKDIR/the-one"
      clone_repo "$ONE_SOURCE" "$ONE_DIR" "$ONE_DEFAULT_REF"
      return
    fi
  fi

  if [ -n "$ONE_JAR" ]; then
    info "ONE jar provided at $ONE_JAR"
    ONE_JAR_PATH="$ONE_JAR"
    return
  fi

  warn "No ONE source or jar configured. If you need to regenerate mobility traces, supply --one-source or --one-jar."
}

load_deps_from_file(){
  if [ -f "$DEPS_FILE" ]; then
    info "Loading dependency defaults from $DEPS_FILE"
    # minimal JSON parsing without jq
    OMNET_VER_CFG=$(grep -o '"omnet": *"[^"]*"' "$DEPS_FILE" | sed 's/"omnet": *"\([^"]*\)"/\1/') || true
    INET_REF_CFG=$(grep -o '"inet": *"[^"]*"' "$DEPS_FILE" | sed 's/"inet": *"\([^"]*\)"/\1/') || true
    SIMU5G_REF_CFG=$(grep -o '"simu5g": *"[^"]*"' "$DEPS_FILE" | sed 's/"simu5g": *"\([^"]*\)"/\1/') || true
    ONE_REF_CFG=$(grep -o '"one": *"[^"]*"' "$DEPS_FILE" | sed 's/"one": *"\([^"]*\)"/\1/') || true
    if [ -n "$OMNET_VER_CFG" ]; then OMNET_VER="$OMNET_VER_CFG"; fi
    if [ -n "$INET_REF_CFG" ]; then INET_REF="$INET_REF_CFG"; fi
    if [ -n "$SIMU5G_REF_CFG" ]; then SIMU5G_REF="$SIMU5G_REF_CFG"; fi
    if [ -n "$ONE_REF_CFG" ]; then ONE_DEFAULT_REF="$ONE_REF_CFG"; fi
  fi
}

main(){
  parse_args "$@"
  load_deps_from_file

  info "Bootstrap starting in working directory: $WORKDIR"

  if [ "$ANALYSIS_ONLY" = true ]; then
    info "Analysis-only mode: only setting up Python environment and data requirements"
    setup_python_venv_and_requirements
    info "Analysis-only bootstrap complete"
    exit 0
  fi

  if [ "$SKIP_OMNET" != true ]; then
    if confirm "Install system packages and build toolchain (may require sudo)?"; then
      install_system_packages
    else
      warn "Skipping system package install. Ensure you have build tools, JDK and Qt dev packages installed."
    fi
  else
    info "Skipping OMNeT++ install as requested (--skip-omnet)"
  fi

  # Create Python venv early so OMNeT++ can use it during configure
  info "Setting up Python virtual environment before building OMNeT++"
  setup_python_venv_and_requirements

  # Default OMNET_DIR if not provided: use working dir
  if [ -z "$OMNET_DIR" ]; then
    OMNET_DIR="$WORKDIR/omnetpp-${OMNET_VER}"
  fi

  if [ "$SKIP_OMNET" != true ]; then
    if confirm "Download and build OMNeT++ ${OMNET_VER} into ${OMNET_DIR}?"; then
      install_omnetpp "$OMNET_DIR" "$OMNET_VER"
    else
      warn "Skipping OMNeT++ install."
    fi
  fi

  info "Setting up INET and Simu5G in $WORKDIR"
  mkdir -p "$WORKDIR"
  
  # Download INET from release
  info "Downloading INET ${INET_REF}"
  inet_url="https://github.com/inet-framework/inet/releases/download/${INET_REF}/inet-${INET_REF#v}-src.tgz"
  download_and_extract "$inet_url" "$WORKDIR/inet4.5"
  
  # Clone Simu5G from Unipisa repository
  clone_repo "https://github.com/Unipisa/Simu5G.git" "$WORKDIR/Simu5G" "$SIMU5G_REF"

  setup_one

  copy_modified_files

  build_inet_and_simu5g

  build_project

  # Validate environment setup
  info "Validating environment setup..."
  if [ -f "$OMNET_DIR/setenv" ]; then
    # shellcheck disable=SC1090
    # Temporarily disable -u to avoid unbound variable errors in setenv
    set +u
    source "$OMNET_DIR/setenv"
    set -u
    info "✓ Successfully sourced OMNeT++ environment"
  else
    warn "Could not source OMNeT++ environment from $OMNET_DIR/setenv"
  fi

  if [ -d "$REPO_ROOT/tf_env" ]; then
    # shellcheck disable=SC1090
    source "$REPO_ROOT/tf_env/bin/activate"
    info "✓ Successfully activated Python virtual environment"
    
    # Verify numpy installation
    if python3 -c "import numpy; print('✓ NumPy version:', numpy.__version__)" 2>/dev/null; then
      info "✓ Successfully verified NumPy installation"
    else
      warn "Could not verify NumPy installation"
    fi
  else
    warn "Python virtual environment not found at $REPO_ROOT/tf_env"
  fi

  info "Bootstrap finished. If any step failed, review output and rerun with --force to overwrite targets where appropriate."
}

main "$@"