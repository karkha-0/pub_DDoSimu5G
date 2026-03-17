#!/bin/bash
##############################################################################
# DDoSimu5G Simple Compilation Script
# 
# Sources OMNeT++ environment and compiles DDoSimu5G in release mode
#
# Usage:
#   ./compile_project.sh        # Compile DDoSimu5G
#   ./compile_project.sh clean  # Clean and rebuild
##############################################################################

set -e  # Exit on error

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# DDoSimu5G project root (4 levels up: scripts/ -> Test-cases-002/ -> CaseID/ -> simulations/ -> DDoSimu5G/)
DDOSIMU5G_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# OMNeT++ root (2 levels up from DDoSimu5G: DDoSimu5G/ -> samples/ -> omnetpp-6.0.1/)
OMNETPP_ROOT="$(cd "$DDOSIMU5G_ROOT/../.." && pwd)"

echo "========================================"
echo "DDoSimu5G Compilation"
echo "========================================"
echo ""

# Source OMNeT++ environment
echo "Sourcing OMNeT++ environment from: $OMNETPP_ROOT/setenv"
if [ -f "$OMNETPP_ROOT/setenv" ]; then
    source "$OMNETPP_ROOT/setenv"
    echo "✓ OMNeT++ environment loaded"
else
    echo "✗ ERROR: $OMNETPP_ROOT/setenv not found!"
    exit 1
fi

echo ""
echo "Compiling DDoSimu5G at: $DDOSIMU5G_ROOT"
cd "$DDOSIMU5G_ROOT"

# Clean if requested
if [ "$1" = "clean" ]; then
    echo "Cleaning..."
    # Clean the specific MODE=release build (cleanall doesn't clean mode-specific builds)
    make clean MODE=release
    
    # Force remove shared library (Makefile might not remove it due to hard links)
    if [ -f "$DDOSIMU5G_ROOT/src/libDDoSimu5G.so" ]; then
        echo "Removing stale libDDoSimu5G.so..."
        rm -f "$DDOSIMU5G_ROOT/src/libDDoSimu5G.so"
        echo "✓ Removed"
    fi
    echo ""
fi

# Compile
echo "Building (MODE=release)..."
if make MODE=release -j$(nproc); then
    echo ""
    
    # Build shared library if not created by default
    if [ ! -f "$DDOSIMU5G_ROOT/src/libDDoSimu5G.so" ]; then
        echo "Building shared library..."
        if make MODE=release shared -j$(nproc) 2>/dev/null; then
            echo "✓ Shared library built"
        else
            # Try alternative: link from executable
            echo "Creating shared library from object files..."
            cd "$DDOSIMU5G_ROOT/out/clang-release"
            find . -name "*.o" -exec echo {} \; > /tmp/objlist.txt
            
            # Create shared library manually
            cd "$DDOSIMU5G_ROOT"
            clang++ -shared -o src/libDDoSimu5G.so $(find out/clang-release -name "*.o") || {
                echo "⚠ Warning: Could not create shared library"
                echo "   Run script may use executable mode instead"
            }
        fi
    fi
    
    echo "========================================"
    echo "✓ Compilation successful!"
    echo "========================================"
    
    # Verify library
    if [ -f "$DDOSIMU5G_ROOT/src/libDDoSimu5G.so" ]; then
        echo "✓ libDDoSimu5G.so created"
    else
        echo "⚠ Warning: libDDoSimu5G.so not found"
        echo "   (Executable mode: out/clang-release/DDoSimu5G)"
    fi
else
    echo ""
    echo "========================================"
    echo "✗ Compilation failed"
    echo "========================================"
    exit 1
fi
