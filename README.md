#DDoSimu5G 

This project provides a modular simulation framework combining Simu5G and The ONE Simulator to study the impact of malware-based DDoS attacks in 5G infrastructures. It includes:

    - Mobility and device-to-device infection modeling (via ONE)

    - Traffic orchestration and malware triggers

    - Realistic CBR and flooding traffic patterns (UDP-based)

    - KPI extraction (data rate, packet loss, propagation)

    - Configurable test cases for benign and adversarial scenarios

    - Jupyter-based result analysis

DDoSimu5G useful for researchers and practitioners studying IoT-borne malware, network resilience, and 5G traffic behavior under stress.

All required packages and their versions are listed in the .txt files provided in the repository. Please ensure to use these files when setting up your simulation environment for consistent results.

- Crediting
  +++++++++
This project is licensed under the MIT License. You are free to use, modify, and distribute this software for academic and non-commercial purposes, provided proper credit is given to the original authors.

If you use DDoSimu5G in your research, please cite the repository

- Run simulation from scripts:
  ++++++++++++++++++++++++++++++++++++++++
 - Before running Simu5G, Omnet++, and inet, you need to copy the modified src files under modifiedExternalFiles/ to the respective src files as indicated in the path and rebuild. 

  - under /simulations/CaseID/script, there are two shell files (need to be adjusted)

- Convert D2D Model mobility traces from ONE to Simu5G:
  +++++++++++++++++++++++++++++++++++++++++++++++++++++
  - under simulations/CaseID/script/mob_tracesConversionPy/fromONEtoSimu5G/, there is a script to convert json mobility traces to .mobility suitable to be injected into Simu5G
    - command:
      python3 convert_mob_traces.py (path-to-json-mobility) (.movements filename)

- Results and Visualization
  ++++++++++++++++++++++++
All plots are reproducible using the Jupyter notebooks in results/Test-Cases-001/plotting/Test_cases_plots
Use the pre-exported .csv files in results/Test-Cases-001/plotting/raw_data/ or regenerate with OMNeT++

- To change the ned file locations in the project we need to: 
  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  - Right-click on your project > Properties > OMNeT++ > NED Source Folders.




For questions, issues, or contributions, please open an issue or contact [GitHub profile].


