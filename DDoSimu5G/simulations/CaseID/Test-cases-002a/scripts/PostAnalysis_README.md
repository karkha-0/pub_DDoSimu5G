# Dataset Analysis Scripts

Post-simulation scripts for converting PCAPs to labelled CSVs and producing
publication-grade statistical analysis (Markdown reports, JSON metrics, LaTeX
tables, and plots).

## 1. `pcap_to_csv.py` — PCAP → Labelled CSV

Converts raw simulation PCAPs into per-packet CSVs with labelling
(benign / malicious), attack-type classification (via TOS markers), and
UL/DL direction detection (from ground-truth label files).

### Folder mode (recommended)

Processes every PCAP in a simulation run directory automatically.
UE IPs are extracted from the ground-truth label CSVs found under `labels/`.

```bash
# Process BOTH upf.pcap and all gnb*.pcap → upf.csv + gnb.csv
python3 pcap_to_csv.py --input-dir run_results/pcaps/<Config>/<timestamp>/

# Only gNB PCAPs (merges gnb1–gnb5 into one gnb.csv)
python3 pcap_to_csv.py --input-dir run_results/pcaps/<Config>/<timestamp>/ --type gnb
```

### Single-file mode

```bash
python3 pcap_to_csv.py --type upf --input upf.pcap output/upf.csv
python3 pcap_to_csv.py --type gnb --input gnb1.pcap output/gnb1.csv
```

### Flow aggregation

Append `--make-flows` to generate additional flow-level CSVs alongside the
packet CSV:

```bash
python3 pcap_to_csv.py --input-dir run_results/pcaps/<Config>/<timestamp>/ --make-flows
```

### Key output columns

| Column | Description |
|--------|-------------|
| `label` | `benign` or `malicious` (from TOS attack markers) |
| `attack_type` | `udp_flood`, `tcp_syn_flood`, `http_flood`, `dns_amplification`, or empty |
| `direction` | `UL` (uplink) or `DL` (downlink), inferred from UE IP membership |
| `app_protocol` | `HTTP`, `HTTPS`, `DNS`, `TCP`, `UDP`, `ICMP` (from standard ports) |
| `capture_point` | `upf` or `gnb` |

---

## 2. `analyze_curated_v2.py` — Statistical Analysis

Reads the CSV(s) produced by `pcap_to_csv.py` and generates a full analysis
suite suitable for a scientific paper.

### Basic usage (single vantage point)

```bash
python3 analyze_curated_v2.py \
  --packets output/upf.csv \
  --outdir analysis_upf
```

### With flows

```bash
python3 analyze_curated_v2.py \
  --packets output/upf.csv \
  --flows output/upf_flows_timewindow.csv \
  --outdir analysis_upf
```

### Dual vantage-point mode (UPF + gNB)

Compares packet counts across two capture points and produces a
cross-validation parity table:

```bash
python3 analyze_curated_v2.py \
  --packets output/upf.csv \
  --packets2 output/gnb.csv \
  --labels run_results/pcaps/<Config>/<timestamp>/labels/ \
  --vantage-names "UPF,gNodeB" \
  --outdir analysis_upfv2
```

### Options

| Flag | Default | Description |
|------|---------|-------------|
| `--time-bin` | `10` | Timeline bin size in seconds |
| `--top-n` | `10` | Number of top IPs to display |
| `--no-plots` | off | Skip PNG plot generation |
| `--no-latex` | off | Skip LaTeX table generation |

### Output structure

```
analysis_upfv2/
├── summary_report.md          # Full Markdown report
├── metrics.json               # Machine-readable metrics
├── latex/
│   ├── table_overview.tex
│   ├── table_direction_label.tex
│   ├── table_protocol_label.tex
│   ├── table_statistical_tests.tex   # Welch t, Mann-Whitney U, χ², K-S
│   ├── table_direction_descriptive.tex
│   ├── table_attack_types.tex
│   ├── table_class_imbalance.tex
│   ├── table_benign_app_protocol.tex
│   ├── table_benign_app_protocol_stats.tex
│   ├── table_cv_count_parity.tex     # Cross-validation (dual vantage)
│   └── table_cv_dual_vantage.tex
└── plots/
    ├── label_distribution_pie.png
    ├── direction_label_bar.png
    ├── traffic_timeline.png
    ├── traffic_timeline_combined.png
    ├── packet_size_hist_label.png
    ├── packet_size_hist_direction.png
    ├── protocol_label_stacked.png
    ├── attack_type_bar.png
    ├── attack_type_direction.png
    └── direction_label_heatmap.png
```

### Statistical tests included

- **Welch's *t*-test** — packet size / payload / TTL (benign vs malicious, UL vs DL)
- **Mann–Whitney *U*** — non-parametric alternative
- **Kolmogorov–Smirnov** — distribution shape comparison
- **Chi-square (χ²)** — direction × label, protocol × label independence
- **Cohen's *d*** — effect sizes for all comparisons
- **Cramér's *V*** — association strength for contingency tables
- **Shannon entropy** — label, direction, protocol distributions

---

## Typical end-to-end workflow

```bash
# 1. Convert PCAPs to CSVs (with flows)
python3 pcap_to_csv.py \
  --input-dir run_results/pcaps/Baseline/20260304-16:29:05/ \
  --make-flows

# 2. Run full analysis (dual vantage, with labels cross-validation)
python3 analyze_curated_v2.py \
  --packets output/upf.csv \
  --packets2 output/gnb.csv \
  --labels run_results/pcaps/Baseline/20260304-16:29:05/labels/ \
  --vantage-names "UPF,gNodeB" \
  --outdir analysis_upfv2

# 3. Include LaTeX tables in your paper
#    \input{analysis_upfv2/latex/table_overview.tex}
```