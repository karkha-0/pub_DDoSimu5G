# CRITICAL: Directory Naming Conventions

## DO NOT CHANGE These Directory Names! ⚠️

### INET Directory Must Be `inet4.5` (NOT `inet4.5.0`)

**Why this matters:**
- Simu5G's makefiles and `opp_makemake` configurations hardcode relative paths to `../inet4.5`
- The OMNeT++ message compiler (`opp_msgtool`) uses these paths to resolve INET imports
- If INET is in `inet4.5.0` instead of `inet4.5`, ALL Simu5G `.msg` files will fail with:
  ```
  cannot resolve import 'inet.common.INETDefs'
  cannot resolve import 'inet.common.packet.chunk.Chunk'
  'FieldsChunk': unknown base class 'inet::FieldsChunk'
  ```

**Implementation:**
- `setup_environment.sh`: Uses `INET_DIR_NAME="inet4.5"` variable
- `setup_project.sh`: Hardcodes `inet4.5` in `apply_modifications()` function
- Both must match exactly or build will fail

**Historical Context:**
- Bootstrap install script (`bootstrap_install.sh`) always used `inet4.5`
- Original working configuration established this convention
- v1.0 restructure initially broke this by using `inet${INET_VERSION}` = `inet4.5.0`
- Fixed in commit 4888fd3: "Fix INET directory naming to match bootstrap convention"

### Project Directory Must Be `DDoSimu5G` (NOT `pub_DDoSimu5G`)

**Why this matters:**
- Makefiles reference `../DDoSimu5G/` for source files
- Environment info file stored at `DDoSimu5G/.environment_info.json`
- Build outputs go to `DDoSimu5G/out/`

**Implementation:**
- Git clones to: `$OMNET_DIR/samples/DDoSimu5G`
- All scripts use: `project_dir="$OMNET_DIR/samples/DDoSimu5G"`

### Version Variables vs Directory Names

```bash
# Version tags (for git clone)
INET_VERSION="4.5.0"        # Git tag: v4.5.0
SIMU5G_VERSION="1.2.2"      # Git tag: v1.2.2

# Directory names (for filesystem)
INET_DIR_NAME="inet4.5"     # Hardcoded - DO NOT change to match version!
SIMU5G_DIR="Simu5G"         # Standard name
PROJECT_DIR="DDoSimu5G"     # Project name
```

## Testing Checklist

Before changing any directory names, verify:
1. ✅ INET clones to `omnetpp-6.0.1/samples/inet4.5` (not `inet4.5.0`)
2. ✅ Simu5G clones to `omnetpp-6.0.1/samples/Simu5G`
3. ✅ Project clones to `omnetpp-6.0.1/samples/DDoSimu5G` (not `pub_DDoSimu5G`)
4. ✅ `INET_ROOT` environment variable points to `inet4.5` directory
5. ✅ Simu5G builds without "cannot resolve import" errors
6. ✅ Project builds and links against correct libraries

## Recovery from Wrong Directory Names

If you encounter import errors:

```bash
# Check current directory structure
ls omnetpp-6.0.1/samples/

# If you see inet4.5.0 instead of inet4.5:
cd omnetpp-6.0.1/samples
rm -rf inet4.5.0
git clone --depth 1 --branch v4.5.0 https://github.com/inet-framework/inet.git inet4.5

# Rebuild INET
cd inet4.5
. setenv -f
make makefiles
make MODE=release -j$(nproc)

# Then rebuild Simu5G
cd ../Simu5G
export INET_ROOT="../inet4.5"
make clean
make makefiles
make MODE=release -j$(nproc)
```

---

**Last Updated:** November 10, 2025  
**Related Commits:** 4888fd3 (INET directory naming fix)  
**References:** bootstrap_install.sh line 271, 505
