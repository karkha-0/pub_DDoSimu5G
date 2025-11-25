================================================================================
Test Case Suite 001: 5G Network Performance Under UDP Volumetric Flood Attacks
================================================================================

Purpose:
--------
This test suite measures 5G network performance metrics under UDP volumetric
flood DDoS attacks. The simulations evaluate the impact of distributed denial
of service attacks on Quality of Service (QoS) metrics in 5G networks with
varying mobility patterns and attack intensities.

Research Focus:
--------------
- Impact of volumetric UDP flood attacks on 5G network performance
- Network Performance degradation under different mobility scenarios
- Network behavior with varying numbers of benign and malicious UEs
- Performance of baseline vs , flooding DDoS attack variants

Test Scenarios:
--------------
1. TC-Base-DDoS-infec-5RAN-002-*     : Base DDoS infection attack
2. TC-flood-DDoS-infec-5RAN-002-*    : UDP flood attack

Mobility Variants:
-----------------
- stationary         : 100 UEs remain stationary
- slowMoving         : 100 UEs move at pedestrian speed (~1.5 m/s)
- MixedMoving        : 100 Mix of stationary and moving UEs
- 500cbrUEs          : Additional 400 stationary benign CBR UEs
- 500cbrUEsMv        : Additional 400 moving benign CBR UEs
- 50VidUEsSt         : Additional 50 stationary video streaming UEs

Network Configuration:
---------------------
- 5 gNodeBs (Base Stations) in hexagonal layout
- Base: 100 UEs (mix of benign and infected)
- Extended: Up to 500 UEs with additional benign traffic
- Infection-based DDoS activation (time-based infection traces)
- 3600s simulation time (1 hour)

Attack Mechanism:
----------------
- ONE D2D infection traces based on our malware propagation model define when UEs become compromised
- Infected UEs generate high-volume UDP traffic
- Targets: Both internal 5G network and external servers
- Attack types: Base and Flood

Measured Metrics:
----------------
- Packet delivery ratio
- End-to-end delay
- Throughput (per UE and aggregate)
- Data rate at various components
- Cell load and resource utilization
- Handover statistics (for mobile scenarios)
- Application-level metrics

Output Structure:
----------------
Test-Cases-001/results
  ├── <config-name>/
  │   ├── *.sca        # Scalar statistics
  │   └── *.vec        # Vector time-series data

Directory Structure:
-------------------
TC-Base-DDoS-infec-5RAN-002.ini              # Base attack configurations
TC-flood-DDoS-infec-5RAN-002.ini             # Flood attack configurations

Execution Scripts:
-----------------
Located in: ../script/
- runSim_TC-Base-DDoS-infec-5RAN-002.sh
- runSim_TC-flood-DDoS-infec-5RAN-002.sh

Version: 1.0
Created: 2024
Status: Published
Maintained by: DDoSimu5G Project
Publication: ACM SIGSIM Conference on Principles of Advanced Discrete Simulation (PADS)

Related Work:
------------
This test suite implements the simulation scenarios described in our PADS paper,
evaluating 5G network resilience against volumetric DDoS attacks using the
Simu5G framework integrated with OMNeT++.

================================================================================
