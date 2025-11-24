# pub_DDoSimu5G bootstrap — quick reference

This document explains the bootstrap installer included in this repository and how to use it to reproduce the environment used for the DDoSimu5G experiments.

Files added/used
- `bootstrap_install.sh` — main installer script (supports interactive and non-interactive modes).
- `deps.json` — pinned default versions used by the installer.

Quick goals
- Download and (optionally) build OMNeT++ (default: 6.0.1).
- Clone INET and Simu5G (defaults: `inet4.5`, `Simu5G-1.2.2`) into the workspace.
- Apply patches from `modifiedExternalFiles/` into the checked-out INET/Simu5G directories (rsync with backups).
-- Optionally use the bundled ONE simulator (or a provided source/jar) to reproduce mobility traces.
-- Create a Python virtual environment for analysis (default: `tf_env`) and install requirements.

Usage examples

Run interactively (default):
```bash
cd /path/where/you/want/workspace
# from repo root or anywhere; script uses working dir by default
/path/to/repo/pub_DDoSimu5G/bootstrap_install.sh
```

Run non-interactively (assume yes to prompts):
```bash
cd /path/where/you/want/workspace
/path/to/repo/pub_DDoSimu5G/bootstrap_install.sh --yes --workdir . --omnet-ver 6.0.1
```

Only prepare analysis environment (no OMNeT++/INET/Simu5G builds):
```bash
/path/to/repo/pub_DDoSimu5G/bootstrap_install.sh --analysis-only
```

Using the bundled ONE simulator
```bash
# the script will auto-detect a bundled copy in pub_DDoSimu5G/ONE_Simulator/the-one-1.6.0
/path/to/repo/pub_DDoSimu5G/bootstrap_install.sh --one-source bundled
```

Using a prebuilt ONE jar
```bash
/path/to/repo/pub_DDoSimu5G/bootstrap_install.sh --one-jar /path/to/the-one.jar
```

Configuration (deps.json)
Place `pub_DDoSimu5G/deps.json` to set defaults for the bootstrap. Example:

```json
{
  "omnet": "6.0.1",
  "inet": "inet4.5",
  "simu5g": "Simu5G-1.2.2",
  "one": "the-one-1.6.0",
  "jdk": "11"
}
```

Notes, Caveats and Troubleshooting
- Building OMNeT++ and INET/Simu5G may require additional development packages on your distribution (Qt dev packages, libxml, zlib, etc.). The script attempts to install common packages via apt/dnf/yum but may not cover every distro.
- If `bootstrap_install.sh` cannot find OMNeT++'s `setenv`, source the OMNeT++ environment manually before building INET/Simu5G:

```bash
source /path/to/omnetpp-<ver>/setenv
cd /path/to/inet
make -j$(nproc)
```

- The installer uses `rsync` to apply `modifiedExternalFiles/` patches and saves backups with a timestamped `.orig.<ts>` suffix. Inspect backups if something looks off.

- The `PostSimulationCommand` module will try to run `opp_scavetool` and write outputs into `sim_dataset/` under the project root when possible. To make the post-simulation step write results inside this repository, set the environment variable `PROJECT_ROOT_DIR` before running the simulation, for example:

```bash
export PROJECT_ROOT_DIR="/path/to/your/clone/pub_DDoSimu5G"
```

If `PROJECT_ROOT_DIR` is not set the module falls back to a relative `../../sim_dataset/` path.

Recommended next steps after bootstrap completes
- Source OMNeT++'s `setenv` and run a small example INI in the built project to ensure the environment is correct.
-- Verify Python venv by activating `tf_env/bin/activate` and running `python -c 'import numpy; print(numpy.__version__)'`.

