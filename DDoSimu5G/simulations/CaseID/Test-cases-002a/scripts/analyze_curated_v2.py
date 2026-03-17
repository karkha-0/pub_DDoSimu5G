#!/usr/bin/env python3
"""
analyze_curated_v2.py
---------------------
Publication-grade statistical analyser for CSV outputs produced by ``pcap_to_csv.py``.

Produces:
  • Markdown summary report (tables + narrative)
  • JSON metrics file (machine-readable)
  • LaTeX tables ready for inclusion in a scientific paper
  • Publication-quality plots (PNG, 300 dpi)

All statistics are broken down by **label** (benign / malicious),
**direction** (UL / DL), **protocol**, and **attack type**.

Statistical tests included:
  • Welch's t-test (packet sizes, IAT between benign vs. malicious)
  • Mann–Whitney U (non-parametric alternative)
  • Chi-square test of independence (label × direction)
  • Kolmogorov–Smirnov two-sample test
  • Cohen's d effect size
  • Cramér's V association strength
  • Shannon entropy of label & protocol distributions

Usage:
  python3 analyze_curated_v2.py --packets upf.csv --outdir analysis_upf
  python3 analyze_curated_v2.py --packets upf.csv --flows upf_flows_timewindow.csv --outdir analysis_upf
"""

__version__ = "2.0.0"

import argparse
import csv
import json
import math
import statistics
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# ---------------------------------------------------------------------------
# Optional heavy imports
# ---------------------------------------------------------------------------
try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import matplotlib.ticker as mticker
    HAS_PLOT = True
except ImportError:
    HAS_PLOT = False

try:
    from scipy import stats as sp_stats
    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False


# ============================================================================
# CONFIGURATION
# ============================================================================

@dataclass
class AnalysisConfig:
    """All tunables live here."""
    outdir: str = "analysis_curated"
    plot_dpi: int = 300
    time_bin_sec: int = 10          # for timeline plots
    flow_timeout_sec: float = 60.0  # only relevant if flows are provided
    top_n: int = 10                 # top-N IPs shown

    # derived paths (set in __post_init__)
    plots_dir: str = field(init=False)
    latex_dir: str = field(init=False)

    def __post_init__(self):
        self.plots_dir = str(Path(self.outdir) / "plots")
        self.latex_dir = str(Path(self.outdir) / "latex")
        Path(self.plots_dir).mkdir(parents=True, exist_ok=True)
        Path(self.latex_dir).mkdir(parents=True, exist_ok=True)
        Path(self.outdir).mkdir(parents=True, exist_ok=True)


# ============================================================================
# HELPERS
# ============================================================================

def _safe_int(v, default=0):
    try:
        return int(v)
    except (TypeError, ValueError):
        return default


def _safe_float(v, default=0.0):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def _pct(num, den):
    return 100.0 * num / den if den else 0.0


def _fmt(n):
    """Human-readable number with thousands separator."""
    return f"{n:,}"


def _tex_esc(s: str) -> str:
    """Escape underscores for LaTeX."""
    return s.replace("_", r"\_")


def _descriptive(values: List[float]) -> Dict:
    """Return descriptive statistics dict for a list of numbers."""
    if not values:
        return {}
    n = len(values)
    s = sorted(values)
    mn = s[0]
    mx = s[-1]
    mean = statistics.mean(s)
    med = statistics.median(s)
    std = statistics.pstdev(s) if n > 1 else 0.0

    def percentile(p):
        k = (n - 1) * p / 100.0
        f_idx = int(k)
        c_idx = min(f_idx + 1, n - 1)
        return s[f_idx] + (s[c_idx] - s[f_idx]) * (k - f_idx)

    return {
        "n": n,
        "mean": mean,
        "median": med,
        "std": std,
        "min": mn,
        "max": mx,
        "p25": percentile(25),
        "p75": percentile(75),
        "p95": percentile(95),
        "p99": percentile(99),
    }


def _shannon_entropy(counts: Dict) -> float:
    """Shannon entropy (bits) for a frequency dict."""
    total = sum(counts.values())
    if total == 0:
        return 0.0
    ent = 0.0
    for c in counts.values():
        if c > 0:
            p = c / total
            ent -= p * math.log2(p)
    return ent


def _cohens_d(a: List[float], b: List[float]) -> Optional[float]:
    """Cohen's d effect size (pooled std)."""
    if len(a) < 2 or len(b) < 2:
        return None
    m1, m2 = statistics.mean(a), statistics.mean(b)
    s1, s2 = statistics.pstdev(a), statistics.pstdev(b)
    pooled = math.sqrt((s1**2 + s2**2) / 2)
    return (m1 - m2) / pooled if pooled else None


def _normalize_type(s: str) -> str:
    """Normalise attack-type / traffic-type strings for cross-source comparison."""
    return s.strip().lower().replace(" ", "_").replace("-", "_")



def normalize_label(raw: str) -> str:
    if not raw:
        return "unlabeled"
    s = raw.strip().lower()
    if s in ("benign", "normal", "ok", "legit"):
        return "benign"
    if s in ("malicious", "malicpous", "attack", "mal", "bad"):
        return "malicious"
    return "unlabeled"


def load_csv(path: str) -> List[Dict]:
    rows: List[Dict] = []
    with open(path, "r", newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            rows.append(row)
    return rows


def coerce_packet(row: Dict) -> Dict:
    """In-place type coercion for packet rows."""
    row["timestamp"] = _safe_float(row.get("timestamp"))
    row["packet_size"] = _safe_int(row.get("packet_size"))
    row["ip_len"] = _safe_int(row.get("ip_len"))
    row["l4_payload_len"] = _safe_int(row.get("l4_payload_len"))
    row["ttl"] = _safe_int(row.get("ttl"))
    row["src_port"] = _safe_int(row.get("src_port"))
    row["dst_port"] = _safe_int(row.get("dst_port"))
    row["tcp_flags"] = _safe_int(row.get("tcp_flags"))
    row["label"] = normalize_label(row.get("label", ""))
    row["direction"] = (row.get("direction") or "").strip().upper()
    row["protocol"] = (row.get("protocol") or "").upper()
    row["app_protocol"] = (row.get("app_protocol") or "").upper()
    row["attack_type"] = (row.get("attack_type") or "")
    return row


def coerce_flow(row: Dict) -> Dict:
    """In-place type coercion for flow rows."""
    for k in ("start_time", "end_time", "duration", "packets_per_sec", "bytes_per_sec",
              "pkt_size_mean", "pkt_size_std", "iat_mean", "iat_std",
              "frac_syn", "frac_ack", "frac_fin", "frac_rst",
              "pkt_p25", "pkt_p50", "pkt_p75", "pkt_p90", "pkt_size_entropy"):
        row[k] = _safe_float(row.get(k))
    for k in ("packet_count", "byte_count", "pkt_size_min", "pkt_size_max",
              "pkts_src_to_dst", "pkts_dst_to_src",
              "bytes_src_to_dst", "bytes_dst_to_src"):
        row[k] = _safe_int(row.get(k))
    row["label"] = normalize_label(row.get("label", ""))
    return row


# ============================================================================
# STATISTICS ENGINE
# ============================================================================

class StatsEngine:
    """All computation lives here — pure functions, no I/O."""

    # ------------------------------------------------------------------ #
    # Overview
    # ------------------------------------------------------------------ #
    @staticmethod
    def overview(pkts: List[Dict]) -> Dict:
        total = len(pkts)
        labels = Counter(p["label"] for p in pkts)
        directions = Counter(p["direction"] for p in pkts)
        protocols = Counter(p["protocol"] for p in pkts)
        app_protocols = Counter(p["app_protocol"] for p in pkts)
        attack_types = Counter(p["attack_type"] for p in pkts if p["attack_type"])

        timestamps = [p["timestamp"] for p in pkts]
        t_min = min(timestamps) if timestamps else 0
        t_max = max(timestamps) if timestamps else 0
        duration = t_max - t_min

        sizes = [p["ip_len"] for p in pkts]
        total_bytes = sum(sizes)

        return {
            "total_packets": total,
            "duration_sec": duration,
            "total_bytes": total_bytes,
            "total_mb": total_bytes / (1024 * 1024),
            "labels": dict(labels),
            "directions": dict(directions),
            "protocols": dict(protocols),
            "app_protocols": dict(app_protocols),
            "attack_types": dict(attack_types),
            "size_stats": _descriptive(sizes),
            "time_range": (t_min, t_max),
        }

    # ------------------------------------------------------------------ #
    # Per-direction breakdown
    # ------------------------------------------------------------------ #
    @staticmethod
    def per_direction(pkts: List[Dict]) -> Dict:
        """Return statistics grouped by direction (UL / DL / '')."""
        groups: Dict[str, List[Dict]] = defaultdict(list)
        for p in pkts:
            groups[p["direction"]].append(p)

        result = {}
        for direction, grp in sorted(groups.items()):
            labels = Counter(p["label"] for p in grp)
            protocols = Counter(p["protocol"] for p in grp)
            attack_types = Counter(p["attack_type"] for p in grp if p["attack_type"])
            sizes = [p["ip_len"] for p in grp]
            result[direction or "(none)"] = {
                "count": len(grp),
                "labels": dict(labels),
                "protocols": dict(protocols),
                "attack_types": dict(attack_types),
                "size_stats": _descriptive(sizes),
            }
        return result

    # ------------------------------------------------------------------ #
    # Per-direction × label cross-tab
    # ------------------------------------------------------------------ #
    @staticmethod
    def direction_label_crosstab(pkts: List[Dict]) -> Dict:
        """Build a direction × label contingency table."""
        ct: Dict[str, Dict[str, int]] = defaultdict(lambda: defaultdict(int))
        for p in pkts:
            d = p["direction"] or "(none)"
            lbl = p["label"]
            ct[d][lbl] += 1
        return {d: dict(v) for d, v in ct.items()}

    # ------------------------------------------------------------------ #
    # Protocol × label
    # ------------------------------------------------------------------ #
    @staticmethod
    def protocol_label_crosstab(pkts: List[Dict]) -> Dict:
        ct: Dict[str, Dict[str, int]] = defaultdict(lambda: defaultdict(int))
        for p in pkts:
            ct[p["protocol"]][p["label"]] += 1
        return {pr: dict(v) for pr, v in ct.items()}

    # ------------------------------------------------------------------ #
    # Attack-type breakdown (with direction)
    # ------------------------------------------------------------------ #
    @staticmethod
    def attack_type_stats(pkts: List[Dict]) -> Dict:
        groups: Dict[str, List[Dict]] = defaultdict(list)
        for p in pkts:
            at = p["attack_type"] or "benign"
            groups[at].append(p)
        result = {}
        for at, grp in sorted(groups.items()):
            sizes = [p["ip_len"] for p in grp]
            directions = Counter(p["direction"] for p in grp)
            protocols = Counter(p["protocol"] for p in grp)
            result[at] = {
                "count": len(grp),
                "directions": dict(directions),
                "protocols": dict(protocols),
                "size_stats": _descriptive(sizes),
            }
        return result

    # ------------------------------------------------------------------ #
    # Temporal timeline (time-binned, with direction)
    # ------------------------------------------------------------------ #
    @staticmethod
    def temporal_timeline(pkts: List[Dict], bin_sec: int = 10) -> Dict:
        if not pkts:
            return {}
        timestamps = [p["timestamp"] for p in pkts]
        t_min = min(timestamps)
        t_max = max(timestamps)

        bins: List[Dict] = []
        t = t_min
        while t <= t_max:
            bins.append({
                "start": t,
                "end": t + bin_sec,
                "benign": 0,
                "malicious": 0,
                "UL": 0,
                "DL": 0,
                "UL_benign": 0,
                "UL_malicious": 0,
                "DL_benign": 0,
                "DL_malicious": 0,
                "total": 0,
            })
            t += bin_sec

        for p in pkts:
            idx = int((p["timestamp"] - t_min) / bin_sec)
            if 0 <= idx < len(bins):
                b = bins[idx]
                b["total"] += 1
                lbl = p["label"]
                if lbl in ("benign", "malicious"):
                    b[lbl] += 1
                d = p["direction"]
                if d in ("UL", "DL"):
                    b[d] += 1
                    # combined direction × label
                    combo = f"{d}_{lbl}"
                    if combo in b:
                        b[combo] += 1

        return {"bins": bins, "bin_sec": bin_sec}

    # ------------------------------------------------------------------ #
    # Top-N IPs (overall + per direction)
    # ------------------------------------------------------------------ #
    @staticmethod
    def top_ips(pkts: List[Dict], n: int = 10) -> Dict:
        src_all = Counter(p.get("src_ip", "") for p in pkts)
        dst_all = Counter(p.get("dst_ip", "") for p in pkts)

        result: Dict = {
            "top_src": src_all.most_common(n),
            "top_dst": dst_all.most_common(n),
            "per_direction": {},
        }
        for d in ("UL", "DL"):
            sub = [p for p in pkts if p["direction"] == d]
            if sub:
                result["per_direction"][d] = {
                    "top_src": Counter(p.get("src_ip", "") for p in sub).most_common(n),
                    "top_dst": Counter(p.get("dst_ip", "") for p in sub).most_common(n),
                }
        return result

    # ------------------------------------------------------------------ #
    # Packet-size distribution (binned, by label AND direction)
    # ------------------------------------------------------------------ #
    @staticmethod
    def size_distribution(pkts: List[Dict]) -> Dict:
        BIN_EDGES = [0, 64, 128, 256, 512, 1024, 1500, float("inf")]
        BIN_LABELS = ["<64", "64-127", "128-255", "256-511", "512-1023", "1024-1499", "≥1500"]

        def _bin(sizes):
            counts = {lbl: 0 for lbl in BIN_LABELS}
            for sz in sizes:
                for i in range(len(BIN_EDGES) - 1):
                    if BIN_EDGES[i] <= sz < BIN_EDGES[i + 1]:
                        counts[BIN_LABELS[i]] += 1
                        break
            return counts

        result = {}
        for group_key in ("label", "direction"):
            groups: Dict[str, List[int]] = defaultdict(list)
            for p in pkts:
                groups[p[group_key] or "(none)"].append(p["ip_len"])
            result[group_key] = {k: _bin(v) for k, v in sorted(groups.items())}
        return result

    # ------------------------------------------------------------------ #
    # Flow-level stats (by label)
    # ------------------------------------------------------------------ #
    @staticmethod
    def flow_stats(flows: List[Dict]) -> Dict:
        if not flows:
            return {}
        result = {}
        for label in ("benign", "malicious"):
            grp = [f for f in flows if f["label"] == label]
            if not grp:
                continue
            result[label] = {
                "count": len(grp),
                "duration": _descriptive([f["duration"] for f in grp]),
                "packet_count": _descriptive([float(f["packet_count"]) for f in grp]),
                "byte_count": _descriptive([float(f["byte_count"]) for f in grp]),
                "packets_per_sec": _descriptive([f["packets_per_sec"] for f in grp]),
                "bytes_per_sec": _descriptive([f["bytes_per_sec"] for f in grp]),
                "pkt_size_mean": _descriptive([f["pkt_size_mean"] for f in grp]),
                "iat_mean": _descriptive([f["iat_mean"] for f in grp]),
            }
        return result

    # ------------------------------------------------------------------ #
    # Class-imbalance metrics
    # ------------------------------------------------------------------ #
    @staticmethod
    def class_imbalance(pkts: List[Dict], flows: Optional[List[Dict]] = None) -> Dict:
        p_labels = Counter(p["label"] for p in pkts)
        b = p_labels.get("benign", 0)
        m = p_labels.get("malicious", 0)
        ratio = b / m if m else float("inf")
        ir = max(b, m) / min(b, m) if min(b, m) else float("inf")

        out: Dict = {
            "packet_counts": dict(p_labels),
            "benign_malicious_ratio": ratio,
            "imbalance_ratio": ir,
        }

        # Per-direction imbalance
        for d in ("UL", "DL"):
            sub = [p for p in pkts if p["direction"] == d]
            if sub:
                dl = Counter(p["label"] for p in sub)
                db = dl.get("benign", 0)
                dm = dl.get("malicious", 0)
                out[f"{d}_counts"] = dict(dl)
                out[f"{d}_ratio"] = db / dm if dm else float("inf")

        if flows:
            f_labels = Counter(f["label"] for f in flows)
            fb = f_labels.get("benign", 0)
            fm = f_labels.get("malicious", 0)
            out["flow_counts"] = dict(f_labels)
            out["flow_benign_malicious_ratio"] = fb / fm if fm else float("inf")
            out["flow_imbalance_ratio"] = max(fb, fm) / min(fb, fm) if min(fb, fm) else float("inf")

        return out

    # ------------------------------------------------------------------ #
    # Benign traffic — application-protocol profile
    # ------------------------------------------------------------------ #
    @staticmethod
    def benign_app_protocol_profile(pkts: List[Dict]) -> Dict:
        """Detailed breakdown of benign traffic by application-level protocol.

        Uses the ``app_protocol`` field which is derived from well-known
        port numbers in pcap_to_csv.py:
          HTTP  → TCP ports 80 / 8080 / 8000 or payload heuristics
          HTTPS → TCP port 443
          DNS   → UDP port 53
          TCP   → other TCP traffic
          UDP   → other UDP traffic
          ICMP  → ICMP packets
        """
        benign = [p for p in pkts if p["label"] == "benign"]
        if not benign:
            return {}

        total_benign = len(benign)
        total_bytes = sum(p["ip_len"] for p in benign)

        groups: Dict[str, List[Dict]] = defaultdict(list)
        for p in benign:
            groups[p["app_protocol"] or "(other)"].append(p)

        result: Dict = {"total_benign_packets": total_benign,
                        "total_benign_bytes": total_bytes,
                        "protocols": {}}

        for proto in sorted(groups.keys()):
            grp = groups[proto]
            sizes = [p["ip_len"] for p in grp]
            payloads = [p["l4_payload_len"] for p in grp]
            byte_sum = sum(sizes)
            directions = Counter(p["direction"] for p in grp)
            ports_src = Counter(p["src_port"] for p in grp)
            ports_dst = Counter(p["dst_port"] for p in grp)

            result["protocols"][proto] = {
                "count": len(grp),
                "pct_packets": _pct(len(grp), total_benign),
                "bytes": byte_sum,
                "pct_bytes": _pct(byte_sum, total_bytes),
                "directions": dict(directions),
                "size_stats": _descriptive(sizes),
                "payload_stats": _descriptive(payloads),
                "top_src_ports": ports_src.most_common(5),
                "top_dst_ports": ports_dst.most_common(5),
            }

        return result

    # ------------------------------------------------------------------ #
    # Statistical hypothesis tests
    # ------------------------------------------------------------------ #
    @staticmethod
    def statistical_tests(pkts: List[Dict]) -> Dict:
        """Run standard statistical hypothesis tests."""
        benign_sizes = [p["ip_len"] for p in pkts if p["label"] == "benign"]
        mal_sizes    = [p["ip_len"] for p in pkts if p["label"] == "malicious"]

        benign_payloads = [p["l4_payload_len"] for p in pkts if p["label"] == "benign"]
        mal_payloads    = [p["l4_payload_len"] for p in pkts if p["label"] == "malicious"]

        benign_ttl = [p["ttl"] for p in pkts if p["label"] == "benign"]
        mal_ttl    = [p["ttl"] for p in pkts if p["label"] == "malicious"]

        ul_sizes = [p["ip_len"] for p in pkts if p["direction"] == "UL"]
        dl_sizes = [p["ip_len"] for p in pkts if p["direction"] == "DL"]

        ul_benign  = [p["ip_len"] for p in pkts if p["direction"] == "UL" and p["label"] == "benign"]
        ul_mal     = [p["ip_len"] for p in pkts if p["direction"] == "UL" and p["label"] == "malicious"]
        dl_benign  = [p["ip_len"] for p in pkts if p["direction"] == "DL" and p["label"] == "benign"]
        dl_mal     = [p["ip_len"] for p in pkts if p["direction"] == "DL" and p["label"] == "malicious"]

        results: Dict = {}

        # ---- Effect sizes (always computed, no scipy needed) ----
        results["cohens_d_ip_len_benign_vs_malicious"] = _cohens_d(benign_sizes, mal_sizes)
        results["cohens_d_l4_payload_benign_vs_malicious"] = _cohens_d(benign_payloads, mal_payloads)
        results["cohens_d_ttl_benign_vs_malicious"] = _cohens_d(
            [float(x) for x in benign_ttl], [float(x) for x in mal_ttl])
        results["cohens_d_ip_len_UL_vs_DL"] = _cohens_d(
            [float(x) for x in ul_sizes], [float(x) for x in dl_sizes])
        results["cohens_d_ip_len_UL_benign_vs_malicious"] = _cohens_d(
            [float(x) for x in ul_benign], [float(x) for x in ul_mal])
        results["cohens_d_ip_len_DL_benign_vs_malicious"] = _cohens_d(
            [float(x) for x in dl_benign], [float(x) for x in dl_mal])

        # ---- Entropy ----
        results["entropy_label"] = _shannon_entropy(Counter(p["label"] for p in pkts))
        results["entropy_direction"] = _shannon_entropy(Counter(p["direction"] for p in pkts))
        results["entropy_protocol"] = _shannon_entropy(Counter(p["protocol"] for p in pkts))
        results["entropy_app_protocol"] = _shannon_entropy(Counter(p["app_protocol"] for p in pkts))
        results["entropy_attack_type"] = _shannon_entropy(
            Counter(p["attack_type"] for p in pkts if p["attack_type"]))

        if not HAS_SCIPY:
            results["note"] = "scipy not installed — parametric/non-parametric tests skipped"
            return results

        # ---- Two-sample tests helper ----
        def _two_sample(a, b, name):
            out = {}
            af = [float(x) for x in a]
            bf = [float(x) for x in b]
            if len(af) >= 2 and len(bf) >= 2:
                t_stat, t_p = sp_stats.ttest_ind(af, bf, equal_var=False)
                out[f"{name}_welch_t_stat"] = float(t_stat)
                out[f"{name}_welch_p_value"] = float(t_p)
                try:
                    u_stat, u_p = sp_stats.mannwhitneyu(af, bf, alternative="two-sided")
                    out[f"{name}_mannwhitney_U"] = float(u_stat)
                    out[f"{name}_mannwhitney_p"] = float(u_p)
                except ValueError:
                    pass
            return out

        # Benign vs Malicious (overall)
        results.update(_two_sample(benign_sizes, mal_sizes, "ip_len_benign_vs_malicious"))
        results.update(_two_sample(benign_payloads, mal_payloads, "l4_payload_benign_vs_malicious"))
        results.update(_two_sample(benign_ttl, mal_ttl, "ttl_benign_vs_malicious"))

        # UL vs DL (overall)
        results.update(_two_sample(ul_sizes, dl_sizes, "ip_len_UL_vs_DL"))

        # Benign vs Malicious within each direction
        results.update(_two_sample(ul_benign, ul_mal, "ip_len_UL_benign_vs_malicious"))
        results.update(_two_sample(dl_benign, dl_mal, "ip_len_DL_benign_vs_malicious"))

        # Kolmogorov–Smirnov: packet size distributions
        if len(benign_sizes) >= 5 and len(mal_sizes) >= 5:
            ks_stat, ks_p = sp_stats.ks_2samp(benign_sizes, mal_sizes)
            results["ks_ip_len_benign_vs_malicious_stat"] = float(ks_stat)
            results["ks_ip_len_benign_vs_malicious_p"] = float(ks_p)

        if len(ul_sizes) >= 5 and len(dl_sizes) >= 5:
            ks_stat, ks_p = sp_stats.ks_2samp(ul_sizes, dl_sizes)
            results["ks_ip_len_UL_vs_DL_stat"] = float(ks_stat)
            results["ks_ip_len_UL_vs_DL_p"] = float(ks_p)

        # Chi-square: label × direction
        dir_label_ct = StatsEngine.direction_label_crosstab(pkts)
        all_dirs = sorted(dir_label_ct.keys())
        all_labels = sorted({lbl for v in dir_label_ct.values() for lbl in v})
        if len(all_dirs) >= 2 and len(all_labels) >= 2:
            observed = []
            for d in all_dirs:
                row = [dir_label_ct[d].get(lbl, 0) for lbl in all_labels]
                observed.append(row)
            chi2, chi_p, dof, _ = sp_stats.chi2_contingency(observed)
            results["chi2_direction_label"] = float(chi2)
            results["chi2_direction_label_p"] = float(chi_p)
            results["chi2_direction_label_dof"] = int(dof)

            # Cramér's V
            n_obs = sum(sum(r) for r in observed)
            k = min(len(all_dirs), len(all_labels))
            cramers_v = math.sqrt(chi2 / (n_obs * (k - 1))) if n_obs and k > 1 else 0.0
            results["cramers_v_direction_label"] = cramers_v

        # Chi-square: protocol × label
        proto_ct = StatsEngine.protocol_label_crosstab(pkts)
        all_protos = sorted(proto_ct.keys())
        if len(all_protos) >= 2 and len(all_labels) >= 2:
            observed = []
            for pr in all_protos:
                row = [proto_ct[pr].get(lbl, 0) for lbl in all_labels]
                observed.append(row)
            chi2, chi_p, dof, _ = sp_stats.chi2_contingency(observed)
            results["chi2_protocol_label"] = float(chi2)
            results["chi2_protocol_label_p"] = float(chi_p)
            results["chi2_protocol_label_dof"] = int(dof)

        return results


# ============================================================================
# PLOTS
# ============================================================================

class Plotter:
    """Publication-quality plots (requires matplotlib)."""

    COLORS = {
        "benign": "#2ecc71",
        "malicious": "#e74c3c",
        "UL": "#3498db",
        "DL": "#e67e22",
        "(none)": "#95a5a6",
        "unlabeled": "#bdc3c7",
    }

    def __init__(self, cfg: AnalysisConfig):
        self.cfg = cfg
        if HAS_PLOT:
            try:
                plt.style.use("seaborn-v0_8-whitegrid")
            except Exception:
                pass

    def _savefig(self, fig, name: str):
        path = Path(self.cfg.plots_dir) / name
        fig.savefig(path, dpi=self.cfg.plot_dpi, bbox_inches="tight")
        plt.close(fig)
        print(f"   ✅ {path.name}")

    # -- 1. Label pie -------------------------------------------------------
    def plot_label_pie(self, overview: Dict):
        if not HAS_PLOT:
            return
        labels_dict = overview.get("labels", {})
        if not labels_dict:
            return
        keys = [k for k in ("benign", "malicious", "unlabeled") if k in labels_dict]
        values = [labels_dict[k] for k in keys]
        colors = [self.COLORS.get(k, "#bdc3c7") for k in keys]

        fig, ax = plt.subplots(figsize=(5, 5))
        wedges, texts, autotexts = ax.pie(
            values, labels=[k.capitalize() for k in keys], autopct="%1.1f%%",
            colors=colors, explode=[0.03] * len(keys), startangle=90,
            textprops={"fontsize": 11})
        for at in autotexts:
            at.set_fontweight("bold")
        ax.set_title("Packet Label Distribution", fontsize=13, fontweight="bold")
        self._savefig(fig, "label_distribution_pie.png")

    # -- 2. Direction × label grouped bar -----------------------------------
    def plot_direction_bar(self, per_dir: Dict):
        if not HAS_PLOT:
            return
        import numpy as np
        dirs = sorted(per_dir.keys())
        if not dirs:
            return
        benign_vals = [per_dir[d]["labels"].get("benign", 0) for d in dirs]
        mal_vals    = [per_dir[d]["labels"].get("malicious", 0) for d in dirs]

        x = np.arange(len(dirs))
        w = 0.35
        fig, ax = plt.subplots(figsize=(7, 5))
        bars_b = ax.bar(x - w / 2, benign_vals, w, label="Benign",    color=self.COLORS["benign"])
        bars_m = ax.bar(x + w / 2, mal_vals,    w, label="Malicious", color=self.COLORS["malicious"])
        ax.bar_label(bars_b, fmt="%d", fontsize=8, padding=2)
        ax.bar_label(bars_m, fmt="%d", fontsize=8, padding=2)
        ax.set_xticks(x)
        ax.set_xticklabels(dirs, fontsize=11)
        ax.set_ylabel("Packet Count", fontsize=11)
        ax.set_title("Label Distribution per Direction", fontsize=13, fontweight="bold")
        ax.legend(fontsize=10)
        ax.grid(axis="y", alpha=0.3)
        self._savefig(fig, "direction_label_bar.png")

    # -- 3. Timeline (label + direction, 2 panels) -------------------------
    def plot_timeline(self, tl: Dict, capture_point: str = ""):
        if not HAS_PLOT or not tl:
            return
        bins = tl["bins"]
        bin_sec = tl["bin_sec"]
        t = [b["start"] for b in bins]

        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 7), sharex=True)

        # panel 1: benign vs malicious (packets per second)
        benign_pps = [b["benign"] / bin_sec for b in bins]
        mal_pps    = [b["malicious"] / bin_sec for b in bins]
        ax1.fill_between(t, benign_pps, alpha=0.5, color=self.COLORS["benign"], label="Benign")
        ax1.fill_between(t, mal_pps,    alpha=0.5, color=self.COLORS["malicious"], label="Malicious")
        ax1.set_ylabel("Packets / sec", fontsize=11)
        title_suffix = f" ({capture_point})" if capture_point else ""
        ax1.set_title(f"Traffic Timeline — Label{title_suffix}", fontsize=13, fontweight="bold")
        ax1.legend(fontsize=10)
        ax1.grid(alpha=0.3)

        # panel 2: UL vs DL
        ul_pps = [b["UL"] / bin_sec for b in bins]
        dl_pps = [b["DL"] / bin_sec for b in bins]
        ax2.fill_between(t, ul_pps, alpha=0.5, color=self.COLORS["UL"], label="Uplink (UL)")
        ax2.fill_between(t, dl_pps, alpha=0.5, color=self.COLORS["DL"], label="Downlink (DL)")
        ax2.set_ylabel("Packets / sec", fontsize=11)
        ax2.set_xlabel("Simulation Time (s)", fontsize=11)
        ax2.set_title("Traffic Timeline — Direction", fontsize=13, fontweight="bold")
        ax2.legend(fontsize=10)
        ax2.grid(alpha=0.3)

        self._savefig(fig, "traffic_timeline.png")

    # -- 4. Timeline per direction×label (4-curve, single panel) ------------
    def plot_timeline_combined(self, tl: Dict):
        if not HAS_PLOT or not tl:
            return
        bins = tl["bins"]
        bin_sec = tl["bin_sec"]
        t = [b["start"] for b in bins]

        fig, ax = plt.subplots(figsize=(10, 5))
        combos = [
            ("UL_benign",    "UL Benign",    "#27ae60", "-"),
            ("UL_malicious", "UL Malicious", "#c0392b", "-"),
            ("DL_benign",    "DL Benign",    "#2980b9", "--"),
            ("DL_malicious", "DL Malicious", "#e67e22", "--"),
        ]
        for key, label, color, ls in combos:
            vals = [b.get(key, 0) / bin_sec for b in bins]
            if any(v > 0 for v in vals):
                ax.plot(t, vals, color=color, linestyle=ls, linewidth=1.5, label=label, alpha=0.85)

        ax.set_xlabel("Simulation Time (s)", fontsize=11)
        ax.set_ylabel("Packets / sec", fontsize=11)
        ax.set_title("Traffic Timeline — Direction × Label", fontsize=13, fontweight="bold")
        ax.legend(fontsize=9, ncol=2)
        ax.grid(alpha=0.3)
        self._savefig(fig, "traffic_timeline_combined.png")

    # -- 5. Packet-size histogram by label ----------------------------------
    def plot_size_hist_by_label(self, pkts: List[Dict]):
        if not HAS_PLOT:
            return
        benign_sizes = [p["ip_len"] for p in pkts if p["label"] == "benign"]
        mal_sizes    = [p["ip_len"] for p in pkts if p["label"] == "malicious"]
        if not benign_sizes and not mal_sizes:
            return

        fig, ax = plt.subplots(figsize=(8, 5))
        bins_edges = list(range(0, 1600, 50))
        if benign_sizes:
            ax.hist(benign_sizes, bins=bins_edges, alpha=0.6,
                    color=self.COLORS["benign"], label=f"Benign (n={len(benign_sizes):,})",
                    density=True)
        if mal_sizes:
            ax.hist(mal_sizes, bins=bins_edges, alpha=0.6,
                    color=self.COLORS["malicious"], label=f"Malicious (n={len(mal_sizes):,})",
                    density=True)
        ax.set_xlabel("IP Packet Length (bytes)", fontsize=11)
        ax.set_ylabel("Density", fontsize=11)
        ax.set_title("Packet Size Distribution by Label", fontsize=13, fontweight="bold")
        ax.legend(fontsize=10)
        ax.grid(alpha=0.3)
        self._savefig(fig, "packet_size_hist_label.png")

    # -- 6. Packet-size histogram by direction ------------------------------
    def plot_size_hist_by_direction(self, pkts: List[Dict]):
        if not HAS_PLOT:
            return
        ul_sizes = [p["ip_len"] for p in pkts if p["direction"] == "UL"]
        dl_sizes = [p["ip_len"] for p in pkts if p["direction"] == "DL"]
        if not ul_sizes and not dl_sizes:
            return

        fig, ax = plt.subplots(figsize=(8, 5))
        bins_edges = list(range(0, 1600, 50))
        if ul_sizes:
            ax.hist(ul_sizes, bins=bins_edges, alpha=0.6,
                    color=self.COLORS["UL"], label=f"UL (n={len(ul_sizes):,})", density=True)
        if dl_sizes:
            ax.hist(dl_sizes, bins=bins_edges, alpha=0.6,
                    color=self.COLORS["DL"], label=f"DL (n={len(dl_sizes):,})", density=True)
        ax.set_xlabel("IP Packet Length (bytes)", fontsize=11)
        ax.set_ylabel("Density", fontsize=11)
        ax.set_title("Packet Size Distribution by Direction", fontsize=13, fontweight="bold")
        ax.legend(fontsize=10)
        ax.grid(alpha=0.3)
        self._savefig(fig, "packet_size_hist_direction.png")

    # -- 7. Protocol × label stacked bar -----------------------------------
    def plot_protocol_label(self, proto_ct: Dict):
        if not HAS_PLOT or not proto_ct:
            return
        import numpy as np
        protos = sorted(proto_ct.keys())
        benign = [proto_ct[p].get("benign", 0) for p in protos]
        mal    = [proto_ct[p].get("malicious", 0) for p in protos]

        x = np.arange(len(protos))
        w = 0.5
        fig, ax = plt.subplots(figsize=(8, 5))
        ax.bar(x, benign, w, label="Benign",    color=self.COLORS["benign"])
        ax.bar(x, mal,    w, bottom=benign, label="Malicious", color=self.COLORS["malicious"])
        ax.set_xticks(x)
        ax.set_xticklabels(protos, fontsize=10)
        ax.set_ylabel("Packet Count", fontsize=11)
        ax.set_title("Protocol × Label", fontsize=13, fontweight="bold")
        ax.legend(fontsize=10)
        ax.grid(axis="y", alpha=0.3)
        self._savefig(fig, "protocol_label_stacked.png")

    # -- 8. Attack-type horizontal bar --------------------------------------
    def plot_attack_types(self, at_stats: Dict):
        if not HAS_PLOT:
            return
        import numpy as np
        types = [k for k in sorted(at_stats.keys()) if k != "benign"]
        if not types:
            return
        counts = [at_stats[t]["count"] for t in types]

        fig, ax = plt.subplots(figsize=(8, 4))
        x = np.arange(len(types))
        ax.barh(x, counts, color=self.COLORS["malicious"], alpha=0.85)
        ax.set_yticks(x)
        ax.set_yticklabels([t.replace("_", " ").title() for t in types], fontsize=10)
        ax.set_xlabel("Packet Count", fontsize=11)
        ax.set_title("Attack Type Breakdown", fontsize=13, fontweight="bold")
        ax.grid(axis="x", alpha=0.3)
        self._savefig(fig, "attack_type_bar.png")

    # -- 9. Direction × label heatmap (contingency) -------------------------
    def plot_direction_label_heatmap(self, ct: Dict):
        if not HAS_PLOT:
            return
        import numpy as np
        dirs = sorted(ct.keys())
        labels_order = ["benign", "malicious", "unlabeled"]
        all_labels = [lbl for lbl in labels_order if any(lbl in ct[d] for d in dirs)]
        if len(dirs) < 2 or len(all_labels) < 2:
            return

        matrix = []
        for d in dirs:
            matrix.append([ct[d].get(lbl, 0) for lbl in all_labels])
        matrix = np.array(matrix, dtype=float)

        fig, ax = plt.subplots(figsize=(6, 4))
        im = ax.imshow(matrix, cmap="YlOrRd", aspect="auto")
        ax.set_xticks(range(len(all_labels)))
        ax.set_xticklabels([lbl.capitalize() for lbl in all_labels], fontsize=10)
        ax.set_yticks(range(len(dirs)))
        ax.set_yticklabels(dirs, fontsize=10)
        for i in range(len(dirs)):
            for j in range(len(all_labels)):
                v = int(matrix[i, j])
                ax.text(j, i, f"{v:,}", ha="center", va="center", fontsize=10,
                        color="white" if matrix[i, j] > matrix.max() * 0.6 else "black")
        ax.set_title("Direction × Label Contingency", fontsize=13, fontweight="bold")
        fig.colorbar(im, ax=ax, shrink=0.8, label="Packet Count")
        self._savefig(fig, "direction_label_heatmap.png")

    # -- 10. Attack-type per direction stacked bar --------------------------
    def plot_attack_direction(self, at_stats: Dict):
        if not HAS_PLOT:
            return
        import numpy as np
        types = [k for k in sorted(at_stats.keys()) if k != "benign"]
        if not types:
            return

        ul_vals = [at_stats[t]["directions"].get("UL", 0) for t in types]
        dl_vals = [at_stats[t]["directions"].get("DL", 0) for t in types]

        x = np.arange(len(types))
        w = 0.5
        fig, ax = plt.subplots(figsize=(8, 5))
        ax.bar(x, ul_vals, w, label="UL", color=self.COLORS["UL"])
        ax.bar(x, dl_vals, w, bottom=ul_vals, label="DL", color=self.COLORS["DL"])
        ax.set_xticks(x)
        ax.set_xticklabels([t.replace("_", " ").title() for t in types], fontsize=9, rotation=25, ha="right")
        ax.set_ylabel("Packet Count", fontsize=11)
        ax.set_title("Attack Type × Direction", fontsize=13, fontweight="bold")
        ax.legend(fontsize=10)
        ax.grid(axis="y", alpha=0.3)
        self._savefig(fig, "attack_type_direction.png")


# ============================================================================
# REPORT WRITERS
# ============================================================================

class ReportWriter:
    """Markdown + JSON + LaTeX output."""

    def __init__(self, cfg: AnalysisConfig):
        self.cfg = cfg

    # ------------------------------------------------------------------ #
    # Markdown
    # ------------------------------------------------------------------ #
    def write_markdown(self, stats: Dict):
        path = Path(self.cfg.outdir) / "summary_report.md"
        with open(path, "w") as f:
            self._md(f, stats)
        print(f"   ✅ {path}")

    def _md(self, f, s):
        f.write("# Dataset Statistical Summary\n\n")
        f.write(f"_Generated by `analyze_curated_v2.py` v{__version__}_\n\n---\n\n")

        # --- 1. Overview ---
        ov = s.get("overview", {})
        f.write("## 1. Overview\n\n")
        f.write("| Metric | Value |\n|--------|-------|\n")
        f.write(f"| Total packets | {_fmt(ov.get('total_packets', 0))} |\n")
        f.write(f"| Simulation duration | {ov.get('duration_sec', 0):.1f} s |\n")
        f.write(f"| Data volume | {ov.get('total_mb', 0):.2f} MB |\n")
        for lbl in ("benign", "malicious", "unlabeled"):
            cnt = ov.get("labels", {}).get(lbl, 0)
            pct = _pct(cnt, ov.get("total_packets", 0))
            f.write(f"| {lbl.capitalize()} packets | {_fmt(cnt)} ({pct:.1f}%) |\n")
        for d in ("UL", "DL"):
            cnt = ov.get("directions", {}).get(d, 0)
            pct = _pct(cnt, ov.get("total_packets", 0))
            f.write(f"| {d} packets | {_fmt(cnt)} ({pct:.1f}%) |\n")
        f.write("\n")

        # --- Protocols ---
        f.write("### Protocols\n\n")
        protos = ov.get("protocols", {})
        f.write("| Protocol | Count | % |\n|----------|-------|---|\n")
        for pr, cnt in sorted(protos.items(), key=lambda x: -x[1]):
            f.write(f"| {pr} | {_fmt(cnt)} | {_pct(cnt, ov.get('total_packets', 0)):.1f}% |\n")
        f.write("\n")

        # --- Attack types ---
        atk = ov.get("attack_types", {})
        if atk:
            f.write("### Attack Types\n\n")
            f.write("| Type | Count |\n|------|-------|\n")
            for at, cnt in sorted(atk.items(), key=lambda x: -x[1]):
                f.write(f"| {at} | {_fmt(cnt)} |\n")
            f.write("\n")

        # --- 2. Per-direction ---
        pd = s.get("per_direction", {})
        if pd:
            f.write("## 2. Per-Direction Breakdown\n\n")
            for d, ds in sorted(pd.items()):
                f.write(f"### Direction: {d}\n\n")
                f.write(f"- Packets: {_fmt(ds['count'])}\n")
                for lbl in ("benign", "malicious"):
                    cnt = ds["labels"].get(lbl, 0)
                    f.write(f"- {lbl.capitalize()}: {_fmt(cnt)} ({_pct(cnt, ds['count']):.1f}%)\n")
                ss = ds.get("size_stats", {})
                if ss:
                    f.write(f"- Packet size — mean: {ss['mean']:.1f}, median: {ss['median']:.1f}, "
                            f"std: {ss['std']:.1f}, min: {ss['min']}, max: {ss['max']}\n")
                at_d = ds.get("attack_types", {})
                if at_d:
                    f.write(f"- Attack types: {dict(at_d)}\n")
                f.write("\n")

        # --- 3. Contingency ---
        ct = s.get("direction_label_crosstab", {})
        if ct:
            f.write("## 3. Direction × Label Contingency Table\n\n")
            all_labels = sorted({lbl for v in ct.values() for lbl in v})
            header = "| Direction | " + " | ".join(lbl.capitalize() for lbl in all_labels) + " | Total |\n"
            sep = "|" + "---|" * (len(all_labels) + 2) + "\n"
            f.write(header)
            f.write(sep)
            for d in sorted(ct.keys()):
                vals = [ct[d].get(lbl, 0) for lbl in all_labels]
                total = sum(vals)
                f.write(f"| {d} | " + " | ".join(_fmt(v) for v in vals) + f" | {_fmt(total)} |\n")
            f.write("\n")

        # --- 4. Statistical tests ---
        tests = s.get("statistical_tests", {})
        if tests:
            f.write("## 4. Statistical Tests\n\n")

            f.write("### Effect Sizes (Cohen's d)\n\n")
            f.write("| Comparison | Cohen's d | Magnitude |\n|------------|----------|----------|\n")
            for key in sorted(tests):
                if key.startswith("cohens_d_"):
                    v = tests[key]
                    label = key.replace("cohens_d_", "").replace("_", " ")
                    if v is not None:
                        mag = "negligible" if abs(v) < 0.2 else "small" if abs(v) < 0.5 else \
                              "medium" if abs(v) < 0.8 else "large"
                        f.write(f"| {label} | {v:.4f} | {mag} |\n")
                    else:
                        f.write(f"| {label} | — | — |\n")
            f.write("\n")

            f.write("### Shannon Entropy\n\n")
            f.write("| Distribution | Entropy (bits) |\n|--------------|----------------|\n")
            for key in sorted(tests):
                if key.startswith("entropy_"):
                    label = key.replace("entropy_", "").replace("_", " ").capitalize()
                    f.write(f"| {label} | {tests[key]:.4f} |\n")
            f.write("\n")

            if HAS_SCIPY:
                f.write("### Hypothesis Tests\n\n")
                f.write("| Test | Comparison | Statistic | p-value | Sig. |\n")
                f.write("|------|------------|-----------|---------|------|\n")

                def _write_test(name, comp, stat_key, p_key):
                    if stat_key in tests and p_key in tests:
                        stat_v = tests[stat_key]
                        p_v = tests[p_key]
                        sig = "***" if p_v < 0.001 else "**" if p_v < 0.01 else "*" if p_v < 0.05 else "ns"
                        f.write(f"| {name} | {comp} | {stat_v:.4f} | {p_v:.2e} | {sig} |\n")

                _write_test("Welch t", "IP len (B vs M)",
                            "ip_len_benign_vs_malicious_welch_t_stat",
                            "ip_len_benign_vs_malicious_welch_p_value")
                _write_test("Mann-Whitney U", "IP len (B vs M)",
                            "ip_len_benign_vs_malicious_mannwhitney_U",
                            "ip_len_benign_vs_malicious_mannwhitney_p")
                _write_test("Welch t", "L4 payload (B vs M)",
                            "l4_payload_benign_vs_malicious_welch_t_stat",
                            "l4_payload_benign_vs_malicious_welch_p_value")
                _write_test("Welch t", "TTL (B vs M)",
                            "ttl_benign_vs_malicious_welch_t_stat",
                            "ttl_benign_vs_malicious_welch_p_value")
                _write_test("Welch t", "IP len (UL vs DL)",
                            "ip_len_UL_vs_DL_welch_t_stat",
                            "ip_len_UL_vs_DL_welch_p_value")
                _write_test("Welch t", "IP len UL (B vs M)",
                            "ip_len_UL_benign_vs_malicious_welch_t_stat",
                            "ip_len_UL_benign_vs_malicious_welch_p_value")
                _write_test("Welch t", "IP len DL (B vs M)",
                            "ip_len_DL_benign_vs_malicious_welch_t_stat",
                            "ip_len_DL_benign_vs_malicious_welch_p_value")
                _write_test("K-S", "IP len (B vs M)",
                            "ks_ip_len_benign_vs_malicious_stat",
                            "ks_ip_len_benign_vs_malicious_p")
                _write_test("K-S", "IP len (UL vs DL)",
                            "ks_ip_len_UL_vs_DL_stat",
                            "ks_ip_len_UL_vs_DL_p")
                _write_test("χ²", "Direction × Label",
                            "chi2_direction_label",
                            "chi2_direction_label_p")
                _write_test("χ²", "Protocol × Label",
                            "chi2_protocol_label",
                            "chi2_protocol_label_p")
                f.write("\n")
                f.write("_Significance: * p<0.05, ** p<0.01, *** p<0.001, ns = not significant_\n\n")

                if "cramers_v_direction_label" in tests:
                    f.write(f"Cramér's V (direction × label): **{tests['cramers_v_direction_label']:.4f}**\n\n")

        # --- 5. Class imbalance ---
        ci = s.get("class_imbalance", {})
        if ci:
            f.write("## 5. Class Imbalance\n\n")
            f.write(f"- Benign : Malicious ratio (packets): **{ci.get('benign_malicious_ratio', 0):.2f}:1**\n")
            f.write(f"- Imbalance ratio: **{ci.get('imbalance_ratio', 0):.2f}:1**\n")
            for d in ("UL", "DL"):
                r = ci.get(f"{d}_ratio")
                if r is not None:
                    cnts = ci.get(f"{d}_counts", {})
                    f.write(f"- {d} ratio (B:M): **{r:.2f}:1** "
                            f"(B={_fmt(cnts.get('benign', 0))}, M={_fmt(cnts.get('malicious', 0))})\n")
            if "flow_benign_malicious_ratio" in ci:
                f.write(f"- Flow ratio (B:M): **{ci.get('flow_benign_malicious_ratio', 0):.2f}:1**\n")
            f.write("\n")

        # --- 6. Flow stats ---
        fs = s.get("flow_stats", {})
        if fs:
            f.write("## 6. Flow-Level Statistics\n\n")
            for label in ("benign", "malicious"):
                if label not in fs:
                    continue
                data = fs[label]
                f.write(f"### {label.capitalize()} Flows (n={data['count']})\n\n")
                f.write("| Metric | Mean | Median | Std | P25 | P75 | P95 |\n")
                f.write("|--------|------|--------|-----|-----|-----|-----|\n")
                for metric in ("duration", "packet_count", "byte_count",
                                "packets_per_sec", "bytes_per_sec",
                                "pkt_size_mean", "iat_mean"):
                    d = data.get(metric, {})
                    if d:
                        f.write(f"| {metric} | {d['mean']:.2f} | {d['median']:.2f} | "
                                f"{d['std']:.2f} | {d['p25']:.2f} | {d['p75']:.2f} | {d['p95']:.2f} |\n")
                f.write("\n")

        # --- 7. Attack types per direction ---
        at = s.get("attack_type_stats", {})
        if at:
            types = [k for k in sorted(at.keys()) if k != "benign"]
            if types:
                f.write("## 7. Attack Type Breakdown (with Direction)\n\n")
                f.write("| Attack Type | Count | UL | DL | Mean Size | Std Size |\n")
                f.write("|-------------|-------|----|----|-----------|----------|\n")
                for t in types:
                    data = at[t]
                    ss = data.get("size_stats", {})
                    ul = data.get("directions", {}).get("UL", 0)
                    dl = data.get("directions", {}).get("DL", 0)
                    f.write(f"| {t} | {_fmt(data['count'])} | {_fmt(ul)} | {_fmt(dl)} "
                            f"| {ss.get('mean', 0):.1f} | {ss.get('std', 0):.1f} |\n")
                f.write("\n")

        # --- 8. Top IPs ---
        tip = s.get("top_ips", {})
        if tip:
            f.write("## 8. Top IP Addresses\n\n")
            f.write("### Overall\n\n")
            f.write("**Source IPs**\n\n| IP | Count |\n|----|-------|\n")
            for ip, cnt in tip.get("top_src", []):
                f.write(f"| {ip} | {_fmt(cnt)} |\n")
            f.write("\n**Destination IPs**\n\n| IP | Count |\n|----|-------|\n")
            for ip, cnt in tip.get("top_dst", []):
                f.write(f"| {ip} | {_fmt(cnt)} |\n")
            f.write("\n")
            for d in ("UL", "DL"):
                per_d = tip.get("per_direction", {}).get(d)
                if per_d:
                    f.write(f"### {d}\n\n")
                    f.write("**Source IPs**\n\n| IP | Count |\n|----|-------|\n")
                    for ip, cnt in per_d.get("top_src", []):
                        f.write(f"| {ip} | {_fmt(cnt)} |\n")
                    f.write("\n**Destination IPs**\n\n| IP | Count |\n|----|-------|\n")
                    for ip, cnt in per_d.get("top_dst", []):
                        f.write(f"| {ip} | {_fmt(cnt)} |\n")
                    f.write("\n")

        # --- 9. Benign application-protocol profile ---
        bap = s.get("benign_app_protocol_profile", {})
        if bap and bap.get("protocols"):
            f.write("## 9. Benign Traffic — Application Protocol Profile\n\n")
            f.write(f"Total benign packets: **{_fmt(bap['total_benign_packets'])}** "
                    f"({bap['total_benign_bytes'] / (1024*1024):.2f} MB)\n\n")
            f.write("| App Protocol | Packets | % Pkts | Bytes | % Bytes "
                    "| UL | DL | Mean Size | Std Size |\n")
            f.write("|-------------|---------|--------|-------|--------"
                    "|----|----|-----------|----------|\n")
            for proto, pd in sorted(bap["protocols"].items(),
                                     key=lambda x: -x[1]["count"]):
                ss = pd.get("size_stats", {})
                ul = pd["directions"].get("UL", 0)
                dl = pd["directions"].get("DL", 0)
                f.write(f"| {proto} | {_fmt(pd['count'])} | {pd['pct_packets']:.1f}% "
                        f"| {_fmt(pd['bytes'])} | {pd['pct_bytes']:.1f}% "
                        f"| {_fmt(ul)} | {_fmt(dl)} "
                        f"| {ss.get('mean', 0):.1f} | {ss.get('std', 0):.1f} |\n")
            f.write("\n")
            # Top ports per protocol
            f.write("### Top Destination Ports per Benign Protocol\n\n")
            for proto, pd in sorted(bap["protocols"].items(),
                                     key=lambda x: -x[1]["count"]):
                top_dst = pd.get("top_dst_ports", [])
                if top_dst:
                    ports_str = ", ".join(f"{port} ({cnt:,})" for port, cnt in top_dst)
                    f.write(f"- **{proto}**: {ports_str}\n")
            f.write("\n")

    # ------------------------------------------------------------------ #
    # JSON
    # ------------------------------------------------------------------ #
    def write_json(self, stats: Dict):
        path = Path(self.cfg.outdir) / "metrics.json"

        def _clean(obj):
            if isinstance(obj, float):
                if math.isinf(obj) or math.isnan(obj):
                    return str(obj)
            if isinstance(obj, dict):
                return {k: _clean(v) for k, v in obj.items()}
            if isinstance(obj, (list, tuple)):
                return [_clean(v) for v in obj]
            return obj

        with open(path, "w") as f:
            json.dump(_clean(stats), f, indent=2)
        print(f"   ✅ {path}")

    # ------------------------------------------------------------------ #
    # LaTeX tables
    # ------------------------------------------------------------------ #
    def write_latex(self, stats: Dict):
        latex_dir = Path(self.cfg.latex_dir)
        self._latex_overview(latex_dir, stats)
        self._latex_direction_label(latex_dir, stats)
        self._latex_protocol_label(latex_dir, stats)
        self._latex_tests(latex_dir, stats)
        self._latex_direction_descriptive(latex_dir, stats)
        self._latex_attack_types(latex_dir, stats)
        self._latex_class_imbalance(latex_dir, stats)
        self._latex_benign_app_protocol(latex_dir, stats)
        self._latex_cross_validation(latex_dir, stats)

    # -- Table 1: Dataset overview ------------------------------------------
    def _latex_overview(self, d: Path, s: Dict):
        ov = s.get("overview", {})
        if not ov:
            return
        path = d / "table_overview.tex"
        with open(path, "w") as f:
            f.write("% Dataset Overview\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Dataset Overview}\\label{tab:overview}\n")
            f.write("\\begin{tabular}{lr}\n\\toprule\n")
            f.write("\\textbf{Metric} & \\textbf{Value} \\\\\n\\midrule\n")
            f.write(f"Total packets & {_fmt(ov.get('total_packets', 0))} \\\\\n")
            f.write(f"Duration (s) & {ov.get('duration_sec', 0):.1f} \\\\\n")
            f.write(f"Data volume (MB) & {ov.get('total_mb', 0):.2f} \\\\\n")
            for lbl in ("benign", "malicious"):
                cnt = ov.get("labels", {}).get(lbl, 0)
                pct = _pct(cnt, ov.get("total_packets", 0))
                f.write(f"{lbl.capitalize()} & {_fmt(cnt)} ({pct:.1f}\\%) \\\\\n")
            for dr in ("UL", "DL"):
                cnt = ov.get("directions", {}).get(dr, 0)
                pct = _pct(cnt, ov.get("total_packets", 0))
                f.write(f"{dr} & {_fmt(cnt)} ({pct:.1f}\\%) \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
        print(f"   ✅ {path.name}")

    # -- Table 2: Direction × label -----------------------------------------
    def _latex_direction_label(self, d: Path, s: Dict):
        ct = s.get("direction_label_crosstab", {})
        if not ct:
            return
        all_labels = sorted({lbl for v in ct.values() for lbl in v})
        path = d / "table_direction_label.tex"
        with open(path, "w") as f:
            ncols = len(all_labels) + 2
            col_spec = "l" + "r" * (ncols - 1)
            f.write("% Direction x Label Contingency\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Direction $\\times$ Label Contingency Table}\\label{tab:dir_label}\n")
            f.write(f"\\begin{{tabular}}{{{col_spec}}}\n\\toprule\n")
            header = "\\textbf{Direction} & " + \
                     " & ".join(f"\\textbf{{{lbl.capitalize()}}}" for lbl in all_labels) + \
                     " & \\textbf{Total} \\\\\n"
            f.write(header)
            f.write("\\midrule\n")
            for dr in sorted(ct.keys()):
                vals = [ct[dr].get(lbl, 0) for lbl in all_labels]
                total = sum(vals)
                f.write(f"{_tex_esc(dr)} & " + " & ".join(_fmt(v) for v in vals) + f" & {_fmt(total)} \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
        print(f"   ✅ {path.name}")

    # -- Table 3: Protocol × label ------------------------------------------
    def _latex_protocol_label(self, d: Path, s: Dict):
        ct = s.get("protocol_label_crosstab", {})
        if not ct:
            return
        all_labels = sorted({lbl for v in ct.values() for lbl in v})
        path = d / "table_protocol_label.tex"
        with open(path, "w") as f:
            ncols = len(all_labels) + 2
            col_spec = "l" + "r" * (ncols - 1)
            f.write("% Protocol x Label\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Protocol $\\times$ Label Distribution}\\label{tab:proto_label}\n")
            f.write(f"\\begin{{tabular}}{{{col_spec}}}\n\\toprule\n")
            header = "\\textbf{Protocol} & " + \
                     " & ".join(f"\\textbf{{{lbl.capitalize()}}}" for lbl in all_labels) + \
                     " & \\textbf{Total} \\\\\n"
            f.write(header)
            f.write("\\midrule\n")
            for pr in sorted(ct.keys()):
                vals = [ct[pr].get(lbl, 0) for lbl in all_labels]
                total = sum(vals)
                f.write(f"{_tex_esc(pr)} & " + " & ".join(_fmt(v) for v in vals) + f" & {_fmt(total)} \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
        print(f"   ✅ {path.name}")

    # -- Table 4: Statistical tests -----------------------------------------
    def _latex_tests(self, d: Path, s: Dict):
        tests = s.get("statistical_tests", {})
        if not tests:
            return
        path = d / "table_statistical_tests.tex"
        with open(path, "w") as f:
            # -- Hypothesis tests table --
            f.write("% Statistical Hypothesis Tests\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Statistical Hypothesis Tests}\\label{tab:tests}\n")
            f.write("\\begin{tabular}{llrl}\n\\toprule\n")
            f.write("\\textbf{Test} & \\textbf{Comparison} & \\textbf{Statistic} & \\textbf{p-value} \\\\\n")
            f.write("\\midrule\n")

            def _row(name, comp, stat_key, p_key):
                if stat_key in tests and p_key in tests:
                    sv = tests[stat_key]
                    pv = tests[p_key]
                    sig = "$^{***}$" if pv < 0.001 else "$^{**}$" if pv < 0.01 else "$^{*}$" if pv < 0.05 else ""
                    f.write(f"{name} & {_tex_esc(comp)} & {sv:.4f} & {pv:.2e}{sig} \\\\\n")

            _row("Welch $t$", "IP len (B vs M)",
                 "ip_len_benign_vs_malicious_welch_t_stat", "ip_len_benign_vs_malicious_welch_p_value")
            _row("Mann-Whitney $U$", "IP len (B vs M)",
                 "ip_len_benign_vs_malicious_mannwhitney_U", "ip_len_benign_vs_malicious_mannwhitney_p")
            _row("Welch $t$", "L4 payload (B vs M)",
                 "l4_payload_benign_vs_malicious_welch_t_stat", "l4_payload_benign_vs_malicious_welch_p_value")
            _row("Welch $t$", "TTL (B vs M)",
                 "ttl_benign_vs_malicious_welch_t_stat", "ttl_benign_vs_malicious_welch_p_value")
            _row("Welch $t$", "IP len (UL vs DL)",
                 "ip_len_UL_vs_DL_welch_t_stat", "ip_len_UL_vs_DL_welch_p_value")
            _row("Welch $t$", "IP len UL (B vs M)",
                 "ip_len_UL_benign_vs_malicious_welch_t_stat", "ip_len_UL_benign_vs_malicious_welch_p_value")
            _row("Welch $t$", "IP len DL (B vs M)",
                 "ip_len_DL_benign_vs_malicious_welch_t_stat", "ip_len_DL_benign_vs_malicious_welch_p_value")
            _row("K-S", "IP len (B vs M)",
                 "ks_ip_len_benign_vs_malicious_stat", "ks_ip_len_benign_vs_malicious_p")
            _row("K-S", "IP len (UL vs DL)",
                 "ks_ip_len_UL_vs_DL_stat", "ks_ip_len_UL_vs_DL_p")
            _row("$\\chi^2$", "Direction $\\times$ Label",
                 "chi2_direction_label", "chi2_direction_label_p")
            _row("$\\chi^2$", "Protocol $\\times$ Label",
                 "chi2_protocol_label", "chi2_protocol_label_p")

            f.write("\\bottomrule\n")
            f.write("\\multicolumn{4}{l}{\\footnotesize $^{*}p<0.05$, $^{**}p<0.01$, $^{***}p<0.001$} \\\\\n")
            f.write("\\end{tabular}\n\\end{table}\n\n")

            # -- Effect sizes --
            f.write("% Effect Sizes\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Effect Sizes (Cohen's $d$)}\\label{tab:effect_sizes}\n")
            f.write("\\begin{tabular}{lrl}\n\\toprule\n")
            f.write("\\textbf{Comparison} & \\textbf{$d$} & \\textbf{Magnitude} \\\\\n\\midrule\n")
            for key in sorted(tests):
                if key.startswith("cohens_d_"):
                    v = tests[key]
                    label = key.replace("cohens_d_", "").replace("_", " ")
                    if v is not None:
                        mag = "negligible" if abs(v) < 0.2 else "small" if abs(v) < 0.5 else \
                              "medium" if abs(v) < 0.8 else "large"
                        f.write(f"{_tex_esc(label)} & {v:.4f} & {mag} \\\\\n")
                    else:
                        f.write(f"{_tex_esc(label)} & --- & --- \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n\n")

            # -- Entropy --
            f.write("% Shannon Entropy\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Shannon Entropy of Feature Distributions}\\label{tab:entropy}\n")
            f.write("\\begin{tabular}{lr}\n\\toprule\n")
            f.write("\\textbf{Feature} & \\textbf{$H$ (bits)} \\\\\n\\midrule\n")
            for key in sorted(tests):
                if key.startswith("entropy_"):
                    label = key.replace("entropy_", "").replace("_", " ").capitalize()
                    f.write(f"{_tex_esc(label)} & {tests[key]:.4f} \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
        print(f"   ✅ {path.name}")

    # -- Table 5: Descriptive per direction ----------------------------------
    def _latex_direction_descriptive(self, d: Path, s: Dict):
        pd = s.get("per_direction", {})
        if not pd:
            return
        path = d / "table_direction_descriptive.tex"
        with open(path, "w") as f:
            f.write("% Descriptive Statistics per Direction\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Packet Size Descriptive Statistics per Direction}\\label{tab:dir_desc}\n")
            f.write("\\begin{tabular}{lrrrrrr}\n\\toprule\n")
            f.write("\\textbf{Dir.} & \\textbf{$n$} & \\textbf{Mean} & \\textbf{Median} "
                    "& \\textbf{Std} & \\textbf{Min} & \\textbf{Max} \\\\\n\\midrule\n")
            for dr in sorted(pd.keys()):
                ss = pd[dr].get("size_stats", {})
                if ss:
                    f.write(f"{_tex_esc(dr)} & {_fmt(ss['n'])} & {ss['mean']:.1f} & {ss['median']:.1f} "
                            f"& {ss['std']:.1f} & {ss['min']} & {ss['max']} \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
        print(f"   ✅ {path.name}")

    # -- Table 6: Attack types (with direction) -----------------------------
    def _latex_attack_types(self, d: Path, s: Dict):
        at = s.get("attack_type_stats", {})
        if not at:
            return
        types = [k for k in sorted(at.keys()) if k != "benign"]
        if not types:
            return
        path = d / "table_attack_types.tex"
        with open(path, "w") as f:
            f.write("% Attack Type Breakdown\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Attack Type Breakdown}\\label{tab:attack_types}\n")
            f.write("\\begin{tabular}{lrrrrr}\n\\toprule\n")
            f.write("\\textbf{Attack Type} & \\textbf{Count} & \\textbf{UL} & \\textbf{DL} "
                    "& \\textbf{Mean (B)} & \\textbf{Std (B)} \\\\\n\\midrule\n")
            for t in types:
                data = at[t]
                ss = data.get("size_stats", {})
                ul = data.get("directions", {}).get("UL", 0)
                dl = data.get("directions", {}).get("DL", 0)
                f.write(f"{_tex_esc(t)} & {_fmt(data['count'])} & {_fmt(ul)} & {_fmt(dl)} "
                        f"& {ss.get('mean', 0):.1f} & {ss.get('std', 0):.1f} \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
        print(f"   ✅ {path.name}")

    # -- Table 7: Class imbalance -------------------------------------------
    def _latex_class_imbalance(self, d: Path, s: Dict):
        ci = s.get("class_imbalance", {})
        if not ci:
            return
        path = d / "table_class_imbalance.tex"
        with open(path, "w") as f:
            f.write("% Class Imbalance\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Class Imbalance Analysis}\\label{tab:imbalance}\n")
            f.write("\\begin{tabular}{lrrr}\n\\toprule\n")
            f.write("\\textbf{Scope} & \\textbf{Benign} & \\textbf{Malicious} & \\textbf{Ratio (B:M)} \\\\\n")
            f.write("\\midrule\n")
            pc = ci.get("packet_counts", {})
            f.write(f"All packets & {_fmt(pc.get('benign', 0))} & {_fmt(pc.get('malicious', 0))} "
                    f"& {ci.get('benign_malicious_ratio', 0):.2f}:1 \\\\\n")
            for dr in ("UL", "DL"):
                dc = ci.get(f"{dr}_counts", {})
                if dc:
                    ratio = ci.get(f"{dr}_ratio", 0)
                    r_str = f"{ratio:.2f}:1" if not math.isinf(ratio) else "$\\infty$"
                    f.write(f"{dr} & {_fmt(dc.get('benign', 0))} & {_fmt(dc.get('malicious', 0))} "
                            f"& {r_str} \\\\\n")
            fc = ci.get("flow_counts", {})
            if fc:
                fr = ci.get("flow_benign_malicious_ratio", 0)
                fr_str = f"{fr:.2f}:1" if not math.isinf(fr) else "$\\infty$"
                f.write(f"Flows & {_fmt(fc.get('benign', 0))} & {_fmt(fc.get('malicious', 0))} "
                        f"& {fr_str} \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
        print(f"   ✅ {path.name}")


    # -- Table 8: Benign app-protocol profile ------------------------------
    def _latex_benign_app_protocol(self, d: Path, s: Dict):
        bap = s.get("benign_app_protocol_profile", {})
        if not bap or not bap.get("protocols"):
            return
        protos = bap["protocols"]
        path = d / "table_benign_app_protocol.tex"
        with open(path, "w") as f:
            f.write("% Benign Traffic Application Protocol Profile\n")
            f.write("\\begin{table}[htbp]\n\\centering\n")
            f.write("\\caption{Benign Traffic Application Protocol Profile}\\label{tab:benign_app_proto}\n")
            f.write("\\begin{tabular}{lrrrrrr}\n\\toprule\n")
            f.write("\\textbf{App Protocol} & \\textbf{Packets} & \\textbf{\\%} "
                    "& \\textbf{Bytes} & \\textbf{\\%} "
                    "& \\textbf{UL} & \\textbf{DL} \\\\\n")
            f.write("\\midrule\n")
            for proto in sorted(protos.keys(), key=lambda k: -protos[k]["count"]):
                pd = protos[proto]
                ul = pd["directions"].get("UL", 0)
                dl = pd["directions"].get("DL", 0)
                f.write(f"{_tex_esc(proto)} & {_fmt(pd['count'])} & {pd['pct_packets']:.1f}\\% "
                        f"& {_fmt(pd['bytes'])} & {pd['pct_bytes']:.1f}\\% "
                        f"& {_fmt(ul)} & {_fmt(dl)} \\\\\n")
            # Totals row
            f.write("\\midrule\n")
            f.write(f"\\textbf{{Total}} & {_fmt(bap['total_benign_packets'])} & 100.0\\% "
                    f"& {_fmt(bap['total_benign_bytes'])} & 100.0\\% "
                    f"& & \\\\\n")
            f.write("\\bottomrule\n\\end{tabular}\n")
            f.write("\\vspace{2mm}\n")
            f.write("\\footnotesize{Protocol classification based on standard port numbers: "
                    "HTTP (80/8080/8000), HTTPS (443), DNS (53).}\n")
            f.write("\\end{table}\n")

            # Second table: descriptive statistics per app-protocol
            path2 = d / "table_benign_app_protocol_stats.tex"
            with open(path2, "w") as f2:
                f2.write("% Benign Traffic — Packet Size Statistics per Application Protocol\n")
                f2.write("\\begin{table}[htbp]\n\\centering\n")
                f2.write("\\caption{Benign Packet Size Statistics per Application Protocol}"
                         "\\label{tab:benign_app_proto_stats}\n")
                f2.write("\\begin{tabular}{lrrrrrrr}\n\\toprule\n")
                f2.write("\\textbf{App Protocol} & \\textbf{$n$} & \\textbf{Mean} "
                         "& \\textbf{Median} & \\textbf{Std} "
                         "& \\textbf{Min} & \\textbf{Max} & \\textbf{P95} \\\\\n")
                f2.write("\\midrule\n")
                for proto in sorted(protos.keys(), key=lambda k: -protos[k]["count"]):
                    ss = protos[proto].get("size_stats", {})
                    if ss:
                        f2.write(f"{_tex_esc(proto)} & {_fmt(ss['n'])} & {ss['mean']:.1f} "
                                 f"& {ss['median']:.1f} & {ss['std']:.1f} "
                                 f"& {ss['min']} & {ss['max']} & {ss['p95']:.0f} \\\\\n")
                f2.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
            print(f"   ✅ {path2.name}")
        print(f"   ✅ {path.name}")

    # -- Cross-validation tables -------------------------------------------
    def _latex_cross_validation(self, d: Path, s: Dict):
        cv = s.get("cross_validation", {})
        if not cv:
            return

        # (i) Count parity — PCAP total vs label total by label class
        cp = cv.get("count_parity")
        if cp:
            path = d / "table_cv_count_parity.tex"
            with open(path, "w") as f:
                f.write("% Cross-Validation — Count Parity (all PCAP packets vs label records)\n")
                f.write("\\begin{table}[htbp]\n\\centering\n")
                f.write("\\caption{Cross-Validation: Packet Count Parity "
                        "(PCAP vs Labels per Class)}"
                        "\\label{tab:cv_count_parity}\n")
                # 8 cols: Label | PCAP total | App | Overhead | Compared† | Labels | Diff | %Diff
                f.write("\\begin{tabular}{lrrrrrrrr}\n\\toprule\n")
                f.write("\\multicolumn{1}{l}{} & "
                        "\\multicolumn{3}{c}{\\textbf{PCAP}} & "
                        "\\multicolumn{1}{c}{} & "
                        "\\multicolumn{1}{c}{} & "
                        "\\multicolumn{2}{c}{} \\\\\n")
                f.write("\\cmidrule(lr){2-4}\n")
                f.write("\\textbf{Label} & "
                        "\\textbf{Total} & \\textbf{App} & \\textbf{Overhead} & "
                        "\\textbf{Compared$^\\dagger$} & "
                        "\\textbf{Labels} & "
                        "\\textbf{Diff} & \\textbf{\\%\\ Diff} \\\\\n")
                f.write("\\midrule\n")
                for lbl, v in sorted(cp.get("per_class", {}).items()):
                    pct  = f"{v['pct_diff']:.1f}\\%" if v["pct_diff"] is not None else "---"
                    mark = "" if v["within_tolerance"] else " \\textbf{*}"
                    f.write(f"{_tex_esc(lbl)} & "
                            f"{_fmt(v['pcap_count'])} & "
                            f"{_fmt(v['pcap_app_count'])} & "
                            f"{_fmt(v['pcap_overhead_count'])} & "
                            f"\\textbf{{{_fmt(v['pcap_compared'])}}} & "
                            f"{_fmt(v['label_count'])} & "
                            f"{v['diff']:+d} & {pct}{mark} \\\\\n")
                f.write("\\midrule\n")
                verdict = "PASS" if cp["pass"] else "FAIL"
                f.write(f"\\multicolumn{{6}}{{l}}{{\\textbf{{Overall discrepancy: "
                        f"{cp['overall_discrepancy_pct']:.1f}\\%\\ — {verdict}}}}} & & \\\\\n")
                f.write("\\bottomrule\n\\end{tabular}\n")
                f.write("\\vspace{2mm}\n")
                f.write("\\footnotesize{$^\\dagger$\\textit{Compared}: column used for Diff "
                        "and \\% Diff. Benign uses \\textit{App} packets ($l4\\_payload\\_len > 0$) "
                        "since label CSVs record one application-layer event per burst, not "
                        "individual TCP control frames. Malicious uses \\textit{Total} since "
                        "attack packets (e.g.\\ SYN flood) legitimately carry no payload.}\n")
                f.write("\\end{table}\n")
            print(f"   ✅ {path.name}")

        # (ii) Dual-vantage
        dv = cv.get("dual_vantage")
        if dv:
            path = d / "table_cv_dual_vantage.tex"
            with open(path, "w") as f:
                f.write("% Cross-Validation — Dual-Vantage Consistency\n")
                f.write("\\begin{table}[htbp]\n\\centering\n")
                per_type = dv.get("per_type", {})
                # determine column headers from first entry
                vantage_names = []
                if per_type:
                    sample = next(iter(per_type.values()))
                    vantage_names = [k for k in sample.keys() if k != "ratio"]
                na = _tex_esc(vantage_names[0]) if len(vantage_names) > 0 else "A"
                nb = _tex_esc(vantage_names[1]) if len(vantage_names) > 1 else "B"
                f.write("\\caption{Cross-Validation: Dual-Vantage Attack-Type Consistency "
                        f"({na} vs {nb})}}\\label{{tab:cv_dual_vantage}}\n")
                f.write(f"\\begin{{tabular}}{{lrrrr}}\n\\toprule\n")
                f.write(f"\\textbf{{Attack Type}} & \\textbf{{{na}}} & "
                        f"\\textbf{{{nb}}} & \\textbf{{Ratio}} \\\\\n")
                f.write("\\midrule\n")
                for atype, v in sorted(per_type.items()):
                    ca = v.get(vantage_names[0], 0) if vantage_names else 0
                    cb = v.get(vantage_names[1], 0) if len(vantage_names) > 1 else 0
                    ratio = f"{v['ratio']:.3f}" if v["ratio"] is not None else "---"
                    f.write(f"{_tex_esc(atype)} & {_fmt(ca)} & {_fmt(cb)} & {ratio} \\\\\n")
                f.write("\\midrule\n")
                cosine = dv.get("attack_type_cosine_sim", 0.0)
                count_ratio = dv.get("count_ratio", None)
                verdict = "PASS" if dv["pass"] else "FAIL"
                f.write(f"\\multicolumn{{4}}{{l}}{{\\textbf{{Cosine similarity: "
                        f"{cosine:.4f}")
                if count_ratio is not None:
                    f.write(f", count ratio: {count_ratio:.3f}")
                f.write(f" — {verdict}}}}} \\\\\n")
                f.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")
            print(f"   ✅ {path.name}")


# ============================================================================
# CROSS-VALIDATOR
# ============================================================================

class CrossValidator:
    """
    Cross-validation between PCAP-derived packet CSV and simulation label CSVs.

    Check (i)   Count parity  — UL PCAP packet counts vs TX application-event
                                counts in label CSVs, grouped by label.
    Check (ii)  Dual-vantage  — Attack-type distribution cosine similarity and
                                malicious-count ratio between two capture CSVs
                                (e.g., gNodeB and UPF).
    """

    COUNT_TOL_PCT: float = 10.0   # acceptable overall count discrepancy (%)
    COSINE_SIM_THR: float = 0.95  # minimum cosine similarity for dual-vantage pass

    # ------------------------------------------------------------------ #
    # Label CSV loader
    # ------------------------------------------------------------------ #
    @staticmethod
    def load_label_records(labels_dir: str) -> List[Dict]:
        """Load every *.csv under *labels_dir* into plain dicts with coerced types."""
        records: List[Dict] = []
        for csv_file in Path(labels_dir).rglob("*.csv"):
            try:
                with open(csv_file, "r", newline="") as fh:
                    reader = csv.DictReader(fh)
                    for row in reader:
                        row["timestamp"]    = _safe_float(row.get("timestamp"))
                        row["packet_size"]  = _safe_int(row.get("packet_size"))
                        row["label"]        = normalize_label(row.get("label", ""))
                        row["direction"]    = (row.get("direction") or "").strip().upper()
                        row["traffic_type"] = _normalize_type(row.get("traffic_type") or "")
                        row["protocol"]     = (row.get("protocol") or "").upper()
                        row["src_ip"]       = (row.get("src_ip") or "").strip()
                        row["src_port"]     = _safe_int(row.get("src_port"))
                        row["dest_ip"]      = (row.get("dest_ip") or "").strip()
                        row["dest_port"]    = _safe_int(row.get("dest_port"))
                        records.append(row)
            except Exception as exc:
                print(f"   ⚠️  Skipping {Path(csv_file).name}: {exc}")
        return records

    # ------------------------------------------------------------------ #
    # (i) Count parity — PCAP UL vs label TX, grouped by label
    # ------------------------------------------------------------------ #
    @staticmethod
    def check_count_parity(pkts: List[Dict], label_records: List[Dict]) -> Dict:
        """
        Compare PCAP packet counts to label-CSV event counts, grouped by
        label (benign / malicious).

        Filtering strategy (symmetric comparison):
          PCAP  → UL only  (packets sent by UEs)
          Labels→ TX only  (events logged when apps send)

        This avoids counting DL server-reply segmentation (one
        reassembled message logged as 1 label entry but N TCP segments
        in PCAP) and TCP back-scatter.

        App-level classification (both classes):
          - "app" packet: l4_payload_len > 0  **or**  TCP SYN flag set
            (SYN-flood packets are zero-payload but ARE app-generated)
          - "overhead":  pure TCP ACKs, FIN-ACKs, RSTs generated by
            the INET TCP stack (not logged by any app)
        """
        TCP_SYN_FLAG = 0x02

        def _is_app_packet(p: Dict) -> bool:
            """True when the packet was created by an application.

            Packets NOT created by applications (overhead / artifacts):
              - TCP pure ACKs, FIN-ACKs, RSTs (INET TCP state machine)
              - ICMP errors (INET IP stack — port unreachable, etc.)
              - IP fragments without L4 header (offset > 0)

            Packets that ARE application-generated:
              - Any packet with l4_payload_len > 0 (app sent data)
              - TCP SYN (SYN-flood sends raw SYNs via attack app)
              - UDP with payload > 0 (normal app traffic)
              - UDP with payload = 0 BUT src_port != 0 (app sent it,
                just zero-byte payload e.g. keepalive)

            With IP fragmentation fixed (all UDP packets ≤ 1400 B),
            we should no longer see zero-payload UDP fragments.
            """
            proto = (p.get("protocol") or "").upper()

            # ICMP is never app-generated; it's INET stack noise
            if proto == "ICMP":
                return False

            # UDP: require payload > 0 OR valid src_port (not a fragment)
            if proto == "UDP":
                if (p.get("l4_payload_len") or 0) > 0:
                    return True
                # Zero-payload with valid ports = app keepalive
                # Zero-payload with port 0 = IP fragment (should not happen anymore)
                try:
                    return int(p.get("src_port") or 0) != 0
                except (ValueError, TypeError):
                    return False

            # TCP: require payload > 0 OR SYN flag
            if proto == "TCP":
                if (p.get("l4_payload_len") or 0) > 0:
                    return True
                flags = p.get("tcp_flags")
                if flags is not None:
                    try:
                        return int(flags) & TCP_SYN_FLAG != 0
                    except (ValueError, TypeError):
                        pass
                return False

            # Other protocols: conservatively count as app
            return (p.get("l4_payload_len") or 0) > 0

        # ---- filter to UL / TX ----
        ul_pkts   = [p for p in pkts if p.get("direction") == "UL"]
        tx_labels = [r for r in label_records
                     if (r.get("direction") or "").upper() == "TX"]

        pcap_counts: Dict    = defaultdict(int)
        pcap_app: Dict       = defaultdict(int)
        pcap_overhead: Dict  = defaultdict(int)
        for p in ul_pkts:
            lbl = p["label"]
            pcap_counts[lbl] += 1
            if _is_app_packet(p):
                pcap_app[lbl] += 1
            else:
                pcap_overhead[lbl] += 1

        label_counts: Dict = defaultdict(int)
        for r in tx_labels:
            label_counts[r["label"]] += 1

        all_labels = sorted(set(pcap_counts) | set(label_counts))
        per_class: Dict = {}
        total_label = total_discrepancy = 0

        for lbl in all_labels:
            pc       = pcap_counts.get(lbl, 0)
            pc_app   = pcap_app.get(lbl, 0)
            pc_ovhd  = pcap_overhead.get(lbl, 0)
            lc       = label_counts.get(lbl, 0)
            # Compare app-level packets only (both classes). This
            # excludes TCP stack overhead (pure ACKs, FIN, RST) which
            # is never logged by any application.
            pc_cmp   = pc_app
            diff     = pc_cmp - lc
            pct_diff = 100.0 * abs(diff) / lc if lc else None
            within_tol = pct_diff is not None and pct_diff <= CrossValidator.COUNT_TOL_PCT
            per_class[lbl] = {
                "pcap_count":          pc,
                "pcap_app_count":      pc_app,
                "pcap_overhead_count": pc_ovhd,
                "pcap_compared":       pc_cmp,   # what's actually diffed vs labels
                "label_count":         lc,
                "diff":                diff,
                "pct_diff":            round(pct_diff, 2) if pct_diff is not None else None,
                "within_tolerance":    within_tol,
            }
            total_label       += lc
            total_discrepancy += abs(diff)

        overall_pct = 100.0 * total_discrepancy / total_label if total_label else 0.0
        return {
            "per_class":               per_class,
            "overall_discrepancy_pct": round(overall_pct, 3),
            "tolerance_pct":           CrossValidator.COUNT_TOL_PCT,
            "pass":                    overall_pct <= CrossValidator.COUNT_TOL_PCT,
        }

    # ------------------------------------------------------------------ #
    # (ii) Dual-vantage semantic consistency
    # ------------------------------------------------------------------ #
    @staticmethod
    def check_dual_vantage(pkts_a: List[Dict], pkts_b: List[Dict],
                            name_a: str = "primary", name_b: str = "secondary") -> Dict:
        """
        Compare attack-type distributions between two capture vantage points
        (e.g., gNodeB vs UPF).  Uses cosine similarity of count vectors and
        per-type count ratios.  Volume differences due to GTP-U encapsulation
        overhead are expected; semantic agreement should be near-perfect.
        """
        def _dist(pkts: List[Dict]) -> Counter:
            return Counter(_normalize_type(p.get("attack_type") or "")
                           for p in pkts if p["label"] == "malicious")

        dist_a = _dist(pkts_a)
        dist_b = _dist(pkts_b)
        all_types = sorted(set(dist_a) | set(dist_b))

        per_type: Dict = {}
        for t in all_types:
            ca, cb = dist_a.get(t, 0), dist_b.get(t, 0)
            per_type[t] = {
                name_a:  ca,
                name_b:  cb,
                "ratio": round(ca / cb, 4) if cb else None,
            }

        vec_a = [dist_a.get(t, 0) for t in all_types]
        vec_b = [dist_b.get(t, 0) for t in all_types]
        dot   = sum(a * b for a, b in zip(vec_a, vec_b))
        mag_a = math.sqrt(sum(a ** 2 for a in vec_a))
        mag_b = math.sqrt(sum(b ** 2 for b in vec_b))
        cosine_sim = dot / (mag_a * mag_b) if mag_a and mag_b else 0.0

        total_a = sum(dist_a.values())
        total_b = sum(dist_b.values())
        count_ratio = round(total_a / total_b, 4) if total_b else None
        return {
            "vantage_a":                    name_a,
            "vantage_b":                    name_b,
            f"{name_a}_malicious_total":    total_a,
            f"{name_b}_malicious_total":    total_b,
            "count_ratio":                  count_ratio,
            "attack_type_cosine_sim":       round(cosine_sim, 4),
            "per_type":                     per_type,
            "pass":                         (cosine_sim >= CrossValidator.COSINE_SIM_THR
                                             and count_ratio is not None
                                             and 0.85 <= count_ratio <= 1.15),
        }


# ============================================================================
# MAIN
# ============================================================================

def main():
    p = argparse.ArgumentParser(
        description="Statistical analysis of pcap_to_csv.py outputs (publication-grade)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("--packets", required=True, help="Packet CSV (e.g. upf.csv)")
    p.add_argument("--flows", help="Flow CSV (time-window or application). Optional.")
    p.add_argument("--labels", help="Simulation label CSV directory (for cross-validation).")
    p.add_argument("--packets2", help="Second vantage-point packet CSV (e.g. gnb.csv) for dual-vantage check.")
    p.add_argument("--vantage-names", default="UPF,gNodeB", help="Comma-separated names for --packets / --packets2 (default: UPF,gNodeB).")
    p.add_argument("--outdir", default="analysis_curated", help="Output directory")
    p.add_argument("--time-bin", type=int, default=10, help="Timeline bin size (seconds)")
    p.add_argument("--top-n", type=int, default=10, help="Number of top IPs to show")
    p.add_argument("--no-plots", action="store_true", help="Skip plot generation")
    p.add_argument("--no-latex", action="store_true", help="Skip LaTeX table generation")
    p.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    args = p.parse_args()

    cfg = AnalysisConfig(outdir=args.outdir, time_bin_sec=args.time_bin, top_n=args.top_n)

    print()
    print("=" * 65)
    print(f"  analyze_curated_v2.py v{__version__}")
    print("=" * 65)
    print(f"  Packets : {args.packets}")
    print(f"  Flows   : {args.flows or '(none)'}")
    print(f"  Labels  : {args.labels or '(none — cross-validation skipped)'}")
    print(f"  Packets2: {args.packets2 or '(none — dual-vantage skipped)'}")
    print(f"  Output  : {args.outdir}/")
    print(f"  scipy   : {'✅' if HAS_SCIPY else '❌ (statistical tests limited)'}")
    print(f"  matplotlib: {'✅' if HAS_PLOT else '❌ (plots skipped)'}")
    print("=" * 65)

    # ── Load data ──────────────────────────────────────────────────────
    print("\n📂 Loading data...")
    pkt_rows = load_csv(args.packets)
    pkts = [coerce_packet(r) for r in pkt_rows]
    print(f"   {len(pkts):,} packets loaded")

    flows = None
    if args.flows:
        flow_rows = load_csv(args.flows)
        flows = [coerce_flow(r) for r in flow_rows]
        print(f"   {len(flows):,} flows loaded")

    label_records: List[Dict] = []
    if args.labels:
        print(f"   Loading label CSVs from {args.labels} ...")
        label_records = CrossValidator.load_label_records(args.labels)
        print(f"   {len(label_records):,} label records loaded")

    pkts2: List[Dict] = []
    if args.packets2:
        pkt2_rows = load_csv(args.packets2)
        pkts2 = [coerce_packet(r) for r in pkt2_rows]
        print(f"   {len(pkts2):,} secondary packets loaded ({args.packets2})")

    # Check direction coverage
    dir_counts = Counter(p["direction"] for p in pkts)
    has_direction = bool(dir_counts.get("UL", 0) or dir_counts.get("DL", 0))
    if has_direction:
        print(f"   Direction data: UL={dir_counts.get('UL', 0):,}, DL={dir_counts.get('DL', 0):,}")
    else:
        print("   ⚠️  No direction data — direction breakdowns will be empty")

    # ── Compute statistics ─────────────────────────────────────────────
    print("\n📊 Computing statistics...")
    se = StatsEngine

    all_stats: Dict = {
        "version": __version__,
        "input_packets": args.packets,
        "input_flows": args.flows,
    }

    all_stats["overview"]                 = se.overview(pkts)
    all_stats["per_direction"]            = se.per_direction(pkts)
    all_stats["direction_label_crosstab"] = se.direction_label_crosstab(pkts)
    all_stats["protocol_label_crosstab"]  = se.protocol_label_crosstab(pkts)
    all_stats["attack_type_stats"]        = se.attack_type_stats(pkts)
    all_stats["size_distribution"]        = se.size_distribution(pkts)
    all_stats["temporal_timeline"]        = se.temporal_timeline(pkts, cfg.time_bin_sec)
    all_stats["top_ips"]                  = se.top_ips(pkts, cfg.top_n)
    all_stats["class_imbalance"]          = se.class_imbalance(pkts, flows)
    all_stats["benign_app_protocol_profile"] = se.benign_app_protocol_profile(pkts)
    all_stats["statistical_tests"]        = se.statistical_tests(pkts)

    if flows:
        all_stats["flow_stats"] = se.flow_stats(flows)

    # ── Cross-validation (requires --labels and/or --packets2) ────────
    cv_results: Dict = {}
    if label_records:
        print("   Running cross-validation checks...")
        cv_results["count_parity"] = CrossValidator.check_count_parity(pkts, label_records)
        cp = cv_results["count_parity"]
        print(f"   (i)  Count parity:       {'✅ PASS' if cp['pass'] else '⚠️  FAIL'} "
              f"  (discrepancy {cp['overall_discrepancy_pct']:.1f}%)")
    if pkts2:
        vantage_names = [v.strip() for v in args.vantage_names.split(",", 1)]
        name_a = vantage_names[0] if len(vantage_names) > 0 else "primary"
        name_b = vantage_names[1] if len(vantage_names) > 1 else "secondary"
        cv_results["dual_vantage"] = CrossValidator.check_dual_vantage(
            pkts, pkts2, name_a=name_a, name_b=name_b)
        dv_pass = cv_results["dual_vantage"]["pass"]
        print(f"   (ii)  Dual-vantage:      {'✅ PASS' if dv_pass else '⚠️  FAIL'} "
              f"  (cosine {cv_results['dual_vantage']['attack_type_cosine_sim']:.4f}, "
              f"ratio {cv_results['dual_vantage'].get('count_ratio', 'N/A')})")
    if cv_results:
        all_stats["cross_validation"] = cv_results

    print("   ✅ Done")

    # ── Write reports ──────────────────────────────────────────────────
    print("\n💾 Writing reports...")
    rw = ReportWriter(cfg)
    rw.write_markdown(all_stats)
    rw.write_json(all_stats)
    if not args.no_latex:
        rw.write_latex(all_stats)

    # ── Plots ──────────────────────────────────────────────────────────
    if not args.no_plots and HAS_PLOT:
        print("\n📈 Generating plots...")
        plotter = Plotter(cfg)
        capture_point = pkts[0].get("capture_point", "") if pkts else ""
        plotter.plot_label_pie(all_stats["overview"])
        plotter.plot_direction_bar(all_stats["per_direction"])
        plotter.plot_timeline(all_stats["temporal_timeline"], capture_point)
        plotter.plot_timeline_combined(all_stats["temporal_timeline"])
        plotter.plot_size_hist_by_label(pkts)
        plotter.plot_size_hist_by_direction(pkts)
        plotter.plot_protocol_label(all_stats["protocol_label_crosstab"])
        plotter.plot_attack_types(all_stats["attack_type_stats"])
        plotter.plot_direction_label_heatmap(all_stats["direction_label_crosstab"])
        plotter.plot_attack_direction(all_stats["attack_type_stats"])

    print("\n" + "=" * 65)
    print("✅ Analysis complete!")
    print(f"   📄 Report  : {cfg.outdir}/summary_report.md")
    print(f"   📊 Metrics : {cfg.outdir}/metrics.json")
    if not args.no_latex:
        print(f"   📐 LaTeX   : {cfg.latex_dir}/")
    if not args.no_plots and HAS_PLOT:
        print(f"   🖼️  Plots   : {cfg.plots_dir}/")
    print("=" * 65)
    print()


if __name__ == "__main__":
    main()
