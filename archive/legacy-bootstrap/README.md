# Legacy Bootstrap Files (Pre v1.0)

This directory contains files from the original bootstrap installation system, archived for reference.

## Files

- **bootstrap_install.old.sh** - Original monolithic installation script
  - Single large script that handled all installation steps
  - Supported multiple configuration options (stable/latest)
  - Used deps.*.json files for version management

- **deps.json** - Default dependency versions (symlink to deps.stable.json)
- **deps.stable.json** - Stable configuration (OMNeT++ 6.0.1, INET 4.5.0, Simu5G 1.2.2)
- **deps.latest.json** - Latest configuration (OMNeT++ 6.2.0, INET 4.5.4, Simu5G 1.4.1)

## Why Archived?

As of **v1.0**, the project was restructured to use a modular setup system:

**New Modular System:**
- `setup.sh` - Master wrapper script
- `setup_environment.sh` - OMNeT++/INET/Simu5G installation
- `setup_project.sh` - pub_DDoSimu5G project setup

**Benefits:**
- Separation of concerns (environment vs project)
- Better maintainability and testing
- Versions hardcoded in scripts (no external config files needed)
- Clearer error handling and logging

## Reference

These files are kept for:
- Historical reference
- Understanding original implementation decisions
- Troubleshooting by comparing with working bootstrap approach
- Migration guide for users with existing bootstrap installations

## Usage (Not Recommended)

If you need to use the old bootstrap script:

```bash
cd archive/legacy-bootstrap
bash bootstrap_install.old.sh --config stable
```

**However, it's strongly recommended to use the new modular scripts instead:**

```bash
cd ../../
./setup.sh --install-dir ~/simulation
```

---

**Last Updated:** November 17, 2025  
**Archived Version:** Pre-v1.0 bootstrap system  
**Replacement:** Modular setup scripts (v1.0+)
