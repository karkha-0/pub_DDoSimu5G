#!/usr/bin/env python3
"""
pcap_to_csv.py
----------------
Simple, self-contained PCAP -> CSV exporter for UPF and gNB captures.

Features:
- Streaming parsing (dpkt) to keep memory usage low
- UPF mode: parse IPv4 packets and export IP/L4 fields
- gNB mode: parse outer IPv4, detect GTP-U (UDP/2152) and extract inner IPv4
- Label packets by DSCP/TOS markers (attack types) and fallback IPID marker for UDP
- Direction detection (UL/DL) from ground-truth label CSVs
- Chunked CSV writes

Input modes:
  --input-dir <run_folder>   Accept entire run folder (with labels/ and PCAPs)
  --input <file.pcap>        Single PCAP file (no direction detection)

Usage:
  # Folder mode (direction-aware):
  python3 pcap_to_csv.py --type upf --input-dir run_results/pcaps/Config/20260225-19:14:00/

  # Single PCAP mode (backward compatible):
  python3 pcap_to_csv.py --type upf --input input.pcap output.csv
"""

import argparse
import csv
import socket
import sys
from pathlib import Path
from typing import List, Optional, Set
import os
from datetime import datetime

try:
    import dpkt
    import dpkt.dns
except Exception:
    print("dpkt is required (pip install dpkt)")
    raise

# Full TOS byte markers (from AttackMarkers.h)
ATTACK_TOS_MAP = {
    0xD0: 'udp_flood',
    0xD2: 'tcp_syn_flood',
    0xD4: 'http_flood',
    0xD8: 'dns_amplification',
}


# ---------------------------------------------------------------------------
# Ground-truth label helpers
# ---------------------------------------------------------------------------

def load_ue_ips(labels_dir: str) -> Set[str]:
    """Scan label CSVs under *labels_dir* and return the set of all UE IPs.

    UE IP is identified as:
      - ``src_ip``  in rows where ``direction == 'TX'``  (UE transmitted)
      - ``dest_ip`` in rows where ``direction == 'RX'``  (UE received)

    Expected directory layout::

        labels/
        ├── ue0/
        │   ├── app0.csv
        │   └── ...
        └── ue1/
            └── ...
    """
    labels_path = Path(labels_dir)
    if not labels_path.is_dir():
        return set()

    ue_ips: Set[str] = set()
    csv_files = list(labels_path.rglob("*.csv"))
    if not csv_files:
        return ue_ips

    for csv_file in csv_files:
        try:
            with open(csv_file, "r") as fh:
                reader = csv.DictReader(fh)
                for row in reader:
                    direction = row.get("direction", "").strip().upper()
                    if direction == "TX":
                        ip = row.get("src_ip", "").strip()
                        if ip:
                            ue_ips.add(ip)
                            break  # one TX row per file is enough
                    elif direction == "RX":
                        ip = row.get("dest_ip", "").strip()
                        if ip:
                            ue_ips.add(ip)
                            break  # one RX row per file is enough
        except Exception:
            continue  # skip broken / empty files

    return ue_ips


def classify_direction(src_ip: str, dst_ip: str, ue_ips: Optional[Set[str]]) -> str:
    """Return ``'UL'``, ``'DL'``, or ``''`` based on UE IP membership.

    * ``src_ip ∈ ue_ips`` → **UL** (uplink – UE is sending)
    * ``dst_ip ∈ ue_ips`` → **DL** (downlink – UE is receiving)
    * Both or neither    → ``''``
    """
    if not ue_ips:
        return ""
    src_is_ue = src_ip in ue_ips
    dst_is_ue = dst_ip in ue_ips
    if src_is_ue and not dst_is_ue:
        return "UL"
    if dst_is_ue and not src_is_ue:
        return "DL"
    return ""


def discover_pcap(input_dir: str, capture_type: str) -> Optional[str]:
    """Auto-discover a PCAP file inside *input_dir* for the given capture type.

    For ``upf``  → look for ``upf.pcap`` / ``upf.pcapng``
    For ``gnb``  → look for ``gnb*.pcap`` / ``gnb*.pcapng`` (returns first match)
    """
    d = Path(input_dir)
    if capture_type == "upf":
        for ext in ("pcap", "pcapng"):
            candidate = d / f"upf.{ext}"
            if candidate.is_file():
                return str(candidate)
    else:
        # gnb: match gnb1.pcap, gnb2.pcap, etc.
        for ext in ("pcap", "pcapng"):
            matches = sorted(d.glob(f"gnb*.{ext}"))
            if matches:
                return str(matches[0])
    return None


def discover_gnb_pcaps(input_dir: str) -> List[str]:
    """Return a sorted list of all gnb*.pcap / gnb*.pcapng files in *input_dir*.

    Used to merge multiple gNodeB capture files into a single gnb.csv.
    """
    d = Path(input_dir)
    results: List[str] = []
    for ext in ("pcap", "pcapng"):
        results.extend(str(p) for p in sorted(d.glob(f"gnb*.{ext}")))
    # de-dup while preserving sort order
    seen: Set[str] = set()
    deduped: List[str] = []
    for r in results:
        if r not in seen:
            seen.add(r)
            deduped.append(r)
    return deduped


def extract_ipv4_packet(buf):
    """Try extracting an IPv4 packet from a raw frame/buffer.
    Handles Ethernet and some common link-layer offsets used by OMNeT++ captures.
    Returns a dpkt.ip.IP instance or None.
    """
    # Ethernet
    try:
        eth = dpkt.ethernet.Ethernet(buf)
        if isinstance(eth.data, dpkt.ip.IP):
            return eth.data
    except Exception:
        pass

    # Raw IPv4
    try:
        if len(buf) >= 20 and (buf[0] >> 4) == 4:
            return dpkt.ip.IP(buf)
    except Exception:
        pass

    # PPP-like or framed variants (common in OMNeT++ exports)
    candidate_offsets = []
    if len(buf) >= 6 and buf[:5] == b'\x7e\xff\x03\x00\x21':
        candidate_offsets.append(5)
    if len(buf) >= 5 and buf[:4] == b'\xff\x03\x00\x21':
        candidate_offsets.append(4)
    if len(buf) >= 3 and buf[:2] == b'\x00\x21':
        candidate_offsets.append(2)
    candidate_offsets.extend([14, 16])

    for off in candidate_offsets:
        if len(buf) <= off + 20:
            continue
        try:
            if (buf[off] >> 4) != 4:
                continue
        except Exception:
            continue
        try:
            return dpkt.ip.IP(buf[off:])
        except Exception:
            continue

    return None




def parse_gtp_inner_ip_from_bytes(payload: bytes) -> Optional[dict]:
    """Parse inner IPv4 packet from a GTP-U payload bytes (robust).
    Returns dict with keys: src_ip,dst_ip,src_port,dst_port,protocol,ip_len,tcp_flags
    """
    try:
        if not payload or not isinstance(payload, (bytes, bytearray)):
            return None

        L = len(payload)
        # Need at least GTP header (8) + IP header (20)
        if L < 8 + 20:
            return None

        # Skip GTP header (8 bytes)
        inner = payload[8:]
        if len(inner) < 20:
            return None

        # First 20 bytes of IP header
        ip_bytes = inner[:20]
        version_ihl = ip_bytes[0]
        version = version_ihl >> 4
        if version != 4:
            return None
        ihl = (version_ihl & 0x0F) * 4
        inner_ip_len = int.from_bytes(ip_bytes[2:4], 'big')
        protocol = ip_bytes[9]
        src_ip = '.'.join(str(b) for b in ip_bytes[12:16])
        dst_ip = '.'.join(str(b) for b in ip_bytes[16:20])

        src_port = 0
        dst_port = 0
        tcp_flags_str = ""

        # TOS / DSCP / ECN
        tos = ip_bytes[1]
        dscp = tos >> 2
        ecn = tos & 0x03

        # IP ID
        ip_id = int.from_bytes(ip_bytes[4:6], 'big')

        # TTL
        ttl = ip_bytes[8]

        # Ports: need at least ip header + 4 bytes
        if protocol in (6, 17):
            if len(inner) >= ihl + 4:
                port_bytes = inner[ihl:ihl+4]
                if len(port_bytes) >= 4:
                    src_port = int.from_bytes(port_bytes[0:2], 'big')
                    dst_port = int.from_bytes(port_bytes[2:4], 'big')

        # TCP flags
        if protocol == 6:
            # flags byte is at offset ihl + 13 within inner
            flags_idx = ihl + 13
            if len(inner) > flags_idx:
                flags_byte = inner[flags_idx]
                tcp_flags_int = int(flags_byte)

        return {
            'src_ip': src_ip,
            'dst_ip': dst_ip,
            'src_port': src_port,
            'dst_port': dst_port,
            'protocol': protocol,
            'ip_len': inner_ip_len,
            'tcp_flags': tcp_flags_int if 'tcp_flags_int' in locals() else 0,
            'tos': tos,
            'dscp': dscp,
            'ecn': ecn,
            'ip_id': ip_id,
            'ttl': ttl,
        }
    except Exception:
        return None


def classify_attack(ip):
    """Return attack label (string) or 'benign'."""
    # Use the full TOS byte exclusively for labeling (AttackMarkers.h).
    # Return the attack type string (e.g., 'tcp_syn_flood') or empty string when none.
    tos = getattr(ip, 'tos', 0)
    return ATTACK_TOS_MAP.get(tos, '')

def process_upf(pcap_path, out_csv, chunk=10000, progress_interval=100000,
                ue_ips: Optional[Set[str]] = None):
    """Process a UPF (decapsulated) PCAP and write CSV rows with per-packet fields."""
    out_path = Path(out_csv)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        'frame_number', 'timestamp', 'src_ip', 'dst_ip', 'src_port', 'dst_port',
        'protocol', 'packet_size', 'ip_len', 'l4_payload_len', 'ttl', 'tos_hex',
        'dscp', 'ecn', 'ip_id_hex', 'tcp_flags', 'tcp_seq', 'tcp_ack',
        'app_protocol', 'label', 'attack_type', 'capture_point', 'direction'
    ]

    file_size = os.path.getsize(pcap_path)
    if file_size == 0:
        print(f"   ⚠️  {Path(pcap_path).name}: empty file (0 bytes) — skipped")
        return

    f = open(pcap_path, 'rb')
    try:
        pcap = None
        try:
            pcap = dpkt.pcap.Reader(f)
        except Exception:
            pass
        if pcap is None:
            try:
                f.seek(0)
                pcap = dpkt.pcapng.Reader(f)
            except Exception:
                pass
        if pcap is None:
            print(f"   ⚠️  {Path(pcap_path).name}: not a valid pcap/pcapng "
                  f"(size {file_size} B) — skipped")
            return

        writer = None
        buf_rows = []
        frame_no = 0

        for ts, buf in pcap:
            frame_no += 1
            ip = extract_ipv4_packet(buf)
            if ip is None:
                continue

            protocol = {6: 'TCP', 17: 'UDP', 1: 'ICMP'}.get(ip.p, str(ip.p))
            packet_size = len(buf)
            ip_len = getattr(ip, 'len', 0)
            ttl = getattr(ip, 'ttl', 0)
            tos = getattr(ip, 'tos', 0)
            dscp = tos >> 2
            ecn = tos & 0x03
            ip_id = getattr(ip, 'id', 0)

            src_port = dst_port = 0
            l4_payload_len = 0
            tcp_flags = ''
            tcp_seq = tcp_ack = 0
            app_proto = ''

            if isinstance(ip.data, dpkt.tcp.TCP):
                tcp = ip.data
                src_port = tcp.sport
                dst_port = tcp.dport
                l4_payload_len = max(0, len(tcp.data))
                tcp_flags = tcp.flags
                tcp_seq = getattr(tcp, 'seq', 0)
                tcp_ack = getattr(tcp, 'ack', 0)
                # Detect HTTP over TCP by simple payload heuristics or common ports
                try:
                    payload = bytes(tcp.data) if tcp.data else b''
                    http_methods = [b'GET ', b'POST ', b'HEAD ', b'PUT ', b'DELETE ', b'OPTIONS ', b'PATCH ', b'CONNECT ']
                    if any(payload.startswith(m) for m in http_methods) or payload.startswith(b'HTTP/'):
                        app_proto = 'HTTP'
                    elif dst_port in (80, 8080, 8000) or src_port in (80, 8080, 8000):
                        app_proto = 'HTTP'
                    elif dst_port == 443 or src_port == 443:
                        app_proto = 'HTTPS'
                    else:
                        app_proto = 'TCP'
                except Exception:
                    app_proto = 'TCP'
            elif isinstance(ip.data, dpkt.udp.UDP):
                udp = ip.data
                src_port = udp.sport
                dst_port = udp.dport
                l4_payload_len = max(0, len(udp.data))
                # Detect DNS over UDP by port or by parsing
                try:
                    payload = bytes(udp.data) if udp.data else b''
                    if src_port == 53 or dst_port == 53:
                        # quick parse attempt
                        try:
                            dns = dpkt.dns.DNS(payload)
                            app_proto = 'DNS'
                        except Exception:
                            app_proto = 'DNS'
                    else:
                        app_proto = 'UDP'
                except Exception:
                    app_proto = 'UDP'
            else:
                app_proto = protocol

            # Determine attack_type from TOS mapping; empty string means no attack
            attack_type = classify_attack(ip)
            label_field = 'malicious' if attack_type else 'benign'

            src_ip_str = socket.inet_ntoa(ip.src)
            dst_ip_str = socket.inet_ntoa(ip.dst)

            row = {
                'frame_number': frame_no,
                'timestamp': ts,
                'src_ip': src_ip_str,
                'dst_ip': dst_ip_str,
                'src_port': src_port,
                'dst_port': dst_port,
                'protocol': protocol,
                'app_protocol': app_proto,
                'packet_size': packet_size,
                'ip_len': ip_len,
                'l4_payload_len': l4_payload_len,
                'ttl': ttl,
                'tos_hex': f"0x{tos:02x}",
                'dscp': dscp,
                'ecn': ecn,
                'ip_id_hex': f"0x{ip_id:04x}",
                'tcp_flags': tcp_flags,
                'tcp_seq': tcp_seq,
                'tcp_ack': tcp_ack,
                'label': label_field,
                'attack_type': attack_type if attack_type != 'normal' else '',
                'capture_point': 'upf',
                'direction': classify_direction(src_ip_str, dst_ip_str, ue_ips),
            }

            buf_rows.append(row)

            if len(buf_rows) >= chunk:
                if writer is None:
                    out_f = open(out_csv, 'w', newline='')
                    writer = csv.DictWriter(out_f, fieldnames=fieldnames)
                    writer.writeheader()
                for r in buf_rows:
                    writer.writerow(r)
                buf_rows = []
                if frame_no % progress_interval == 0:
                    print(f"  Processed {frame_no:,} packets...")

        # final flush
        if buf_rows:
            if writer is None:
                out_f = open(out_csv, 'w', newline='')
                writer = csv.DictWriter(out_f, fieldnames=fieldnames)
                writer.writeheader()
            for r in buf_rows:
                writer.writerow(r)
        print(f"✅ Finished processing UPF: wrote {frame_no:,} frames to {out_csv}")

        if writer is not None:
            out_f.close()

    finally:
        f.close()


def process_gnb(pcap_path, out_csv, chunk=10000, progress_interval=100000, debug=False,
                ue_ips: Optional[Set[str]] = None):
    """Process a gNB (GTP-U encapsulated) PCAP and write CSV with outer/inner fields."""
    out_path = Path(out_csv)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        'frame_number', 'timestamp',
        # outer
        'outer_src_ip', 'outer_dst_ip', 'outer_src_port', 'outer_dst_port', 'outer_proto',
        # inner
        'src_ip', 'dst_ip', 'src_port', 'dst_port', 'protocol',
        'packet_size', 'ip_len', 'inner_ip_len', 'l4_payload_len',
        'ttl', 'tos_hex', 'dscp', 'ecn', 'ip_id_hex', 'tcp_flags', 'tcp_seq', 'tcp_ack',
        'app_protocol', 'label', 'attack_type', 'capture_point', 'direction'
    ]

    file_size = os.path.getsize(pcap_path)
    if file_size == 0:
        print(f"   ⚠️  {Path(pcap_path).name}: empty file (0 bytes) — skipped")
        return

    f = open(pcap_path, 'rb')
    try:
        pcap = None
        try:
            pcap = dpkt.pcap.Reader(f)
        except Exception:
            pass
        if pcap is None:
            try:
                f.seek(0)
                pcap = dpkt.pcapng.Reader(f)
            except Exception:
                pass
        if pcap is None:
            print(f"   ⚠️  {Path(pcap_path).name}: not a valid pcap/pcapng "
                  f"(size {file_size} B) — skipped")
            return

        writer = None
        buf_rows = []
        frame_no = 0

        for ts, buf in pcap:
            frame_no += 1
            outer = extract_ipv4_packet(buf)
            if outer is None:
                continue

            outer_proto = {6: 'TCP', 17: 'UDP', 1: 'ICMP'}.get(outer.p, str(outer.p))
            outer_src = socket.inet_ntoa(outer.src)
            outer_dst = socket.inet_ntoa(outer.dst)

            # Initialize inner/default values
            inner = None
            src_ip = dst_ip = ''
            src_port = dst_port = 0
            protocol = ''
            ip_len = getattr(outer, 'len', 0)
            inner_ip_len = 0
            packet_size = len(buf)
            l4_payload_len = 0
            ttl = getattr(outer, 'ttl', 0)
            tos = getattr(outer, 'tos', 0)
            dscp = tos >> 2
            ecn = tos & 0x03
            ip_id = getattr(outer, 'id', 0)
            tcp_flags = ''
            tcp_seq = tcp_ack = 0
            app_proto = ''

            # Try to find inner IP via GTP-U: UDP port 2152
            parsed_inner = None
            inner = None
            try:
                if isinstance(outer.data, dpkt.udp.UDP):
                    udp = outer.data
                    payload = bytes(udp.data)
                    # Try robust GTP parser on any UDP payload (avoid relying on port parsing)
                    parsed_inner = parse_gtp_inner_ip_from_bytes(payload)
                    if debug:
                        try:
                            sport = getattr(udp, 'sport', None)
                            dport = getattr(udp, 'dport', None)
                            short_hex = payload[:64].hex()
                        except Exception:
                            sport = dport = None
                            short_hex = ''
                        print(f"[DEBUG] frame={frame_no} outer={outer_src}->{outer_dst} udp_ports={sport}/{dport} payload_len={len(payload)} payload_hex={short_hex} parsed_inner={parsed_inner}")
            except Exception:
                parsed_inner = None
                inner = None

            # If the robust parser returned a dict, prefer its values
            if parsed_inner is not None:
                src_ip = parsed_inner.get('src_ip', '')
                dst_ip = parsed_inner.get('dst_ip', '')
                proto_num = parsed_inner.get('protocol', 0)
                protocol = {6: 'TCP', 17: 'UDP', 1: 'ICMP'}.get(proto_num, str(proto_num))
                inner_ip_len = parsed_inner.get('ip_len', 0)
                src_port = parsed_inner.get('src_port', 0)
                dst_port = parsed_inner.get('dst_port', 0)
                l4_payload_len = 0
                tcp_flags = parsed_inner.get('tcp_flags', '')
                tcp_seq = tcp_ack = 0
                # attempt app-level detection from parsed inner
                try:
                    if proto_num == 6:
                        # TCP: check common HTTP ports
                        if src_port in (80, 8080, 8000) or dst_port in (80, 8080, 8000):
                            app_proto = 'HTTP'
                        elif src_port == 443 or dst_port == 443:
                            app_proto = 'HTTPS'
                        else:
                            app_proto = 'TCP'
                    elif proto_num == 17:
                        if src_port == 53 or dst_port == 53:
                            app_proto = 'DNS'
                        else:
                            app_proto = 'UDP'
                    else:
                        app_proto = protocol
                except Exception:
                    app_proto = protocol
                # Use inner's TOS/IPID/TTL for labeling when available
                tos = parsed_inner.get('tos', tos)
                dscp = parsed_inner.get('dscp', dscp)
                ecn = parsed_inner.get('ecn', ecn)
                ip_id = parsed_inner.get('ip_id', ip_id)
                ttl = parsed_inner.get('ttl', ttl)
            elif inner is not None and isinstance(inner, dpkt.ip.IP):
                src_ip = socket.inet_ntoa(inner.src)
                dst_ip = socket.inet_ntoa(inner.dst)
                protocol = {6: 'TCP', 17: 'UDP', 1: 'ICMP'}.get(inner.p, str(inner.p))
                inner_ip_len = getattr(inner, 'len', 0)
                if isinstance(inner.data, dpkt.tcp.TCP):
                    tcp = inner.data
                    src_port = tcp.sport
                    dst_port = tcp.dport
                    l4_payload_len = max(0, len(tcp.data))
                    tcp_flags = tcp.flags
                    tcp_seq = getattr(tcp, 'seq', 0)
                    tcp_ack = getattr(tcp, 'ack', 0)
                elif isinstance(inner.data, dpkt.udp.UDP):
                    udp2 = inner.data
                    src_port = udp2.sport
                    dst_port = udp2.dport
                    l4_payload_len = max(0, len(udp2.data))

                # app-protocol heuristics for inner
                try:
                    if protocol == 'TCP':
                        if src_port in (80, 8080, 8000) or dst_port in (80, 8080, 8000):
                            app_proto = 'HTTP'
                        elif src_port == 443 or dst_port == 443:
                            app_proto = 'HTTPS'
                        else:
                            # inspect payload briefly
                            if isinstance(inner.data, dpkt.tcp.TCP):
                                pay = bytes(inner.data.data) if inner.data.data else b''
                                if pay.startswith((b'GET ', b'POST ', b'HTTP/')):
                                    app_proto = 'HTTP'
                                else:
                                    app_proto = 'TCP'
                            else:
                                app_proto = 'TCP'
                    elif protocol == 'UDP':
                        if src_port == 53 or dst_port == 53:
                            app_proto = 'DNS'
                        else:
                            app_proto = 'UDP'
                    else:
                        app_proto = protocol
                except Exception:
                    app_proto = protocol

                # Use inner's IP fields for labeling (DSCP / IPID)
                tos = getattr(inner, 'tos', tos)
                dscp = tos >> 2
                ecn = tos & 0x03
                ip_id = getattr(inner, 'id', ip_id)
                ttl = getattr(inner, 'ttl', ttl)

            # Determine attack_type: prefer parsed_inner (dict), else inner dpkt.IP, else outer
            if parsed_inner is not None:
                tos_inner = parsed_inner.get('tos', None)
                attack_type = ATTACK_TOS_MAP.get(tos_inner, '') if tos_inner is not None else ''
            elif inner is not None:
                attack_type = classify_attack(inner)
            else:
                attack_type = classify_attack(outer)
            label_field = 'malicious' if attack_type else 'benign'

            row = {
                'frame_number': frame_no,
                'timestamp': ts,
                'outer_src_ip': outer_src,
                'outer_dst_ip': outer_dst,
                'outer_src_port': getattr(outer.data, 'sport', 0) if hasattr(outer, 'data') else 0,
                'outer_dst_port': getattr(outer.data, 'dport', 0) if hasattr(outer, 'data') else 0,
                'outer_proto': outer_proto,
                'src_ip': src_ip,
                'dst_ip': dst_ip,
                'src_port': src_port,
                'dst_port': dst_port,
                'protocol': protocol,
                'app_protocol': app_proto,
                'packet_size': packet_size,
                'ip_len': ip_len,
                'inner_ip_len': inner_ip_len,
                'l4_payload_len': l4_payload_len,
                'ttl': ttl,
                'tos_hex': f"0x{tos:02x}",
                'dscp': dscp,
                'ecn': ecn,
                'ip_id_hex': f"0x{ip_id:04x}",
                'tcp_flags': tcp_flags,
                'tcp_seq': tcp_seq,
                'tcp_ack': tcp_ack,
                'label': label_field,
                'attack_type': attack_type,
                'capture_point': 'gnb',
                'direction': classify_direction(src_ip, dst_ip, ue_ips),
            }

            buf_rows.append(row)

            if len(buf_rows) >= chunk:
                if writer is None:
                    out_f = open(out_csv, 'w', newline='')
                    writer = csv.DictWriter(out_f, fieldnames=fieldnames)
                    writer.writeheader()
                for r in buf_rows:
                    writer.writerow(r)
                buf_rows = []
                if frame_no % progress_interval == 0:
                    print(f"  Processed {frame_no:,} packets...")

        # final flush
        if buf_rows:
            if writer is None:
                out_f = open(out_csv, 'w', newline='')
                writer = csv.DictWriter(out_f, fieldnames=fieldnames)
                writer.writeheader()
            for r in buf_rows:
                writer.writerow(r)
        print(f"✅ Finished processing gNB: wrote {frame_no:,} frames to {out_csv}")
        if writer is not None:
            out_f.close()

    finally:
        f.close()


def process_gnb_multi(pcap_paths, out_csv, chunk=10000, progress_interval=100000, debug=False,
                      ue_ips: Optional[Set[str]] = None):
    """Process one or more gNB PCAPs, writing all frames into a single merged CSV.

    When a single path is given the call is delegated directly to process_gnb.
    When multiple paths are given each is processed to a temporary CSV, then all
    data rows are concatenated under one shared header into *out_csv*.
    """
    if isinstance(pcap_paths, str):
        pcap_paths = [pcap_paths]
    if not pcap_paths:
        print("\u26a0\ufe0f  process_gnb_multi: no PCAP paths provided")
        return

    n = len(pcap_paths)
    if n == 1:
        process_gnb(pcap_paths[0], out_csv, chunk=chunk,
                    progress_interval=progress_interval, debug=debug, ue_ips=ue_ips)
        return

    # Multiple files: write each to a temp CSV, then merge into one output file
    import tempfile
    import shutil
    tmp_paths: List[str] = []
    try:
        for i, pcap_path in enumerate(pcap_paths, 1):
            print(f"  📡 [{i}/{n}] {Path(pcap_path).name} ...")
            tmp_fd, tmp_path = tempfile.mkstemp(suffix='.csv')
            os.close(tmp_fd)
            tmp_paths.append(tmp_path)
            try:
                process_gnb(pcap_path, tmp_path, chunk=chunk,
                            progress_interval=progress_interval, debug=debug, ue_ips=ue_ips)
            except Exception as exc:
                print(f"   ⚠️  {Path(pcap_path).name}: error ({exc}) — skipped")

        # Only merge temp files that actually contain data (process_gnb wrote rows)
        valid_tmps = [tp for tp in tmp_paths if Path(tp).stat().st_size > 0]
        n_valid = len(valid_tmps)
        n_skipped = n - n_valid
        
        if not valid_tmps:
            print(f"   ⚠️  All {n} gNB PCAPs were empty or invalid — no gnb.csv written")
            return

        Path(out_csv).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(valid_tmps[0], out_csv)
        with open(out_csv, 'a', newline='') as dst:
            dst_writer = csv.writer(dst)
            for tmp_path in valid_tmps[1:]:
                with open(tmp_path, 'r', newline='') as src:
                    src_reader = csv.reader(src)
                    next(src_reader, None)  # skip header
                    for row in src_reader:
                        dst_writer.writerow(row)
        print(f"✅ Merged {n_valid}/{n} gNB PCAPs → {out_csv}"
              + (f" ({n_skipped} skipped)" if n_skipped else ""))
    finally:
        for tmp_path in tmp_paths:
            try:
                Path(tmp_path).unlink()
            except Exception:
                pass


def sanitize_tos_csv(src_csv: str, dst_csv: str) -> None:
    """Create a copy of *src_csv* with the ``tos_hex`` and ``dscp`` columns
    zeroed-out and ``ecn`` set to ``0``.

    The TOS byte is used by pcap_to_csv as a ground-truth labelling
    marker (from AttackMarkers.h) and MUST stay in the original CSV
    for the analysis / parity pipeline.  However, including it in an
    ML feature matrix would leak ground-truth information directly
    into the model.

    This function produces a second "ML-ready" CSV where those columns
    are neutralised (set to ``0x00`` / ``0`` / ``0``).
    """
    with open(src_csv, 'r', newline='') as fin, \
         open(dst_csv, 'w', newline='') as fout:
        reader = csv.DictReader(fin)
        if reader.fieldnames is None:
            return
        writer = csv.DictWriter(fout, fieldnames=reader.fieldnames)
        writer.writeheader()
        for row in reader:
            row['tos_hex']  = '0x00'
            row['dscp']     = '0'
            row['ecn']      = '0'
            writer.writerow(row)
    print(f"🧹 TOS-sanitized CSV → {dst_csv}")


def main():
    p = argparse.ArgumentParser(description='PCAP \u2192 CSV (UPF / gNB)')
    # --- Input modes (mutually exclusive) ---
    input_grp = p.add_mutually_exclusive_group()
    input_grp.add_argument('--input-dir', help=(
        'Run folder containing PCAPs and labels/ subdirectory. '
        'Processes both UPF and all gNB PCAPs by default; restrict with --type.'))
    input_grp.add_argument('--input', help='Input PCAP file (single-file mode, requires --type)')
    p.add_argument('input_pos', nargs='?', help='Input PCAP file (positional, single-file mode)')
    p.add_argument('--output', help='Output CSV file or directory (optional)')
    p.add_argument('output_pos', nargs='?', help='Output CSV file (positional, optional)')
    p.add_argument('--type', choices=['upf', 'gnb'], default=None,
                   help='Capture type to process (default: both upf and gnb in folder mode)')
    p.add_argument('--chunk', type=int, default=10000, help='CSV chunk flush size')
    p.add_argument('--progress-interval', type=int, default=100000,
                   help='Print progress every N processed packets')
    p.add_argument('--make-flows', action='store_true',
                   help='Also generate flow tables (time-window and application) from the CSV')
    p.add_argument('--debug', action='store_true', help='Enable debug printing for gNB parsing')
    args = p.parse_args()

    ue_ips: Optional[Set[str]] = None
    output_spec = args.output or args.output_pos

    if args.input_dir:
        # ── Folder mode ───────────────────────────────────────────────
        run_dir = Path(args.input_dir)
        if not run_dir.is_dir():
            print(f"Error: --input-dir '{args.input_dir}' is not a directory")
            sys.exit(1)
        print(f"\U0001f4e6 Run folder mode: {run_dir}")

        # Determine output directory
        if output_spec:
            out_dir = Path(output_spec)
        else:
            ts_str = datetime.now().strftime('%Y%m%d_%H%M%S')
            out_dir = Path(f"output_{ts_str}")
        out_dir.mkdir(parents=True, exist_ok=True)

        # Load UE IPs from labels/ subfolder
        labels_dir = run_dir / "labels"
        if labels_dir.is_dir():
            ue_ips = load_ue_ips(str(labels_dir))
            if ue_ips:
                print(f"   Loaded {len(ue_ips)} UE IPs from labels \u2192 direction detection enabled")
                for ip in sorted(ue_ips):
                    print(f"      \u2022 {ip}")
            else:
                print(f"   \u26a0\ufe0f  No UE IPs found in {labels_dir} \u2014 direction will be empty")
        else:
            print(f"   \u26a0\ufe0f  No labels/ subfolder in {run_dir} \u2014 direction will be empty")

        # Process both types by default, or the single type specified via --type
        types_to_process = [args.type] if args.type else ['upf', 'gnb']

        for capture_type in types_to_process:
            output_csv = str(out_dir / f"{capture_type}.csv")
            if capture_type == 'upf':
                upf_pcap = discover_pcap(str(run_dir), 'upf')
                if upf_pcap is None:
                    print(f"   \u26a0\ufe0f  No upf.pcap found in '{run_dir}' \u2014 skipping UPF")
                    continue
                print(f"   UPF PCAP : {Path(upf_pcap).name}  \u2192  {output_csv}")
                process_upf(upf_pcap, output_csv, chunk=args.chunk,
                            progress_interval=args.progress_interval, ue_ips=ue_ips)
            else:  # gnb
                gnb_pcaps = discover_gnb_pcaps(str(run_dir))
                if not gnb_pcaps:
                    print(f"   \u26a0\ufe0f  No gnb*.pcap found in '{run_dir}' \u2014 skipping gNB")
                    continue
                names = ', '.join(Path(pp).name for pp in gnb_pcaps)
                print(f"   gNB PCAPs: {names}  \u2192  {output_csv}")
                process_gnb_multi(gnb_pcaps, output_csv, chunk=args.chunk,
                                  progress_interval=args.progress_interval,
                                  debug=args.debug, ue_ips=ue_ips)

            # Always produce a TOS-sanitized "ML-ready" copy
            sanitized_csv = str(Path(output_csv).with_stem(
                Path(output_csv).stem + '_ml'))
            sanitize_tos_csv(output_csv, sanitized_csv)

            if args.make_flows:
                base = str(Path(output_csv).with_suffix(''))
                generate_time_window_flows(output_csv, base + '_flows_timewindow.csv', timeout=60)
                generate_application_flows(output_csv, base + '_flows_application.csv')

    else:
        # ── Single-file mode ──────────────────────────────────────────
        input_path = args.input or args.input_pos
        if not input_path:
            print('Error: no input specified (use --input-dir, --input, or positional input)')
            sys.exit(1)
        if not args.type:
            print('Error: --type {upf,gnb} is required in single-file mode')
            sys.exit(1)

        # Resolve output path
        if output_spec:
            out_path = Path(output_spec)
            if out_path.is_dir() or (not out_path.exists() and output_spec.endswith(os.sep)):
                out_dir = out_path
                out_dir.mkdir(parents=True, exist_ok=True)
                output_csv = str(out_dir / f"{args.type}.csv")
            else:
                out_dir = out_path.parent
                out_dir.mkdir(parents=True, exist_ok=True)
                output_csv = str(out_path)
        else:
            ts_str = datetime.now().strftime('%Y%m%d_%H%M%S')
            out_dir = Path(f"output_{ts_str}")
            out_dir.mkdir(parents=True, exist_ok=True)
            output_csv = str(out_dir / f"{args.type}.csv")

        if args.type == 'upf':
            process_upf(input_path, output_csv, chunk=args.chunk,
                        progress_interval=args.progress_interval, ue_ips=ue_ips)
        else:
            process_gnb_multi([input_path], output_csv, chunk=args.chunk,
                              progress_interval=args.progress_interval,
                              debug=args.debug, ue_ips=ue_ips)

        # Always produce a TOS-sanitized "ML-ready" copy
        sanitized_csv = str(Path(output_csv).with_stem(
            Path(output_csv).stem + '_ml'))
        sanitize_tos_csv(output_csv, sanitized_csv)

        if args.make_flows:
            base = str(Path(output_csv).with_suffix(''))
            generate_time_window_flows(output_csv, base + '_flows_timewindow.csv', timeout=60)
            generate_application_flows(output_csv, base + '_flows_application.csv')


def _row_to_5tuple(row):
    """Return canonical 5-tuple (src,dst,src_port,dst_port,proto) from CSV row."""
    try:
        src = row.get('src_ip', '')
        dst = row.get('dst_ip', '')
        sport = int(row.get('src_port') or 0)
        dport = int(row.get('dst_port') or 0)
        proto = (row.get('protocol') or '').upper()
        return (src, dst, sport, dport, proto)
    except Exception:
        return ('', '', 0, 0, '')


def generate_time_window_flows(packet_csv, out_csv, timeout=60):
    """Aggregate packets into time-window flows (split when gap > timeout seconds).

    Writes a CSV with one flow per line.
    """
    timeout = float(timeout)
    # read packets
    pkts_by_5t = {}
    with open(packet_csv, 'r', newline='') as f:
        r = csv.DictReader(f)
        for row in r:
            key = _row_to_5tuple(row)
            if not key[0]:
                continue
            pkts_by_5t.setdefault(key, []).append(row)

    # prepare writer
    fieldnames = [
        'flow_id', 'src_ip', 'dst_ip', 'src_port', 'dst_port', 'protocol',
        'start_time', 'end_time', 'duration', 'packet_count', 'byte_count',
        'packets_per_sec', 'bytes_per_sec',
        # per-direction
        'pkts_src_to_dst', 'pkts_dst_to_src', 'bytes_src_to_dst', 'bytes_dst_to_src',
        # statistical features
        'pkt_size_mean', 'pkt_size_std', 'pkt_size_min', 'pkt_size_max',
        'iat_mean', 'iat_std',
        # TCP flags fractions
        'frac_syn', 'frac_ack', 'frac_fin', 'frac_rst',
        # percentiles + entropy + port-common
        'pkt_p25', 'pkt_p50', 'pkt_p75', 'pkt_p90', 'pkt_size_entropy',
        'is_src_port_common', 'is_dst_port_common',
        'label', 'capture_point'
    ]

    out_f = open(out_csv, 'w', newline='')
    w = csv.DictWriter(out_f, fieldnames=fieldnames)
    w.writeheader()

    flow_counter = 0
    for key, rows in pkts_by_5t.items():
        # sort rows by timestamp
        rows.sort(key=lambda r: float(r.get('timestamp') or 0.0))
        cur_flow = []
        last_ts = None
        for row in rows:
            ts = float(row.get('timestamp') or 0.0)
            if last_ts is None:
                cur_flow = [row]
                last_ts = ts
                continue
            if ts - last_ts > timeout:
                # flush current
                flow_counter += 1
                _write_flow(w, flow_counter, cur_flow)
                cur_flow = [row]
            else:
                cur_flow.append(row)
            last_ts = ts

        if cur_flow:
            flow_counter += 1
            _write_flow(w, flow_counter, cur_flow)

    out_f.close()


def generate_application_flows(packet_csv, out_csv):
    """Aggregate packets into application flows by 5-tuple (single flow per 5-tuple).

    This treats the entire capture for a 5-tuple as one application flow.
    """
    pkts_by_5t = {}
    with open(packet_csv, 'r', newline='') as f:
        r = csv.DictReader(f)
        for row in r:
            key = _row_to_5tuple(row)
            if not key[0]:
                continue
            pkts_by_5t.setdefault(key, []).append(row)

    fieldnames = [
        'flow_id', 'src_ip', 'dst_ip', 'src_port', 'dst_port', 'protocol',
        'start_time', 'end_time', 'duration', 'packet_count', 'byte_count',
        'packets_per_sec', 'bytes_per_sec',
        'pkts_src_to_dst', 'pkts_dst_to_src', 'bytes_src_to_dst', 'bytes_dst_to_src',
        'pkt_size_mean', 'pkt_size_std', 'pkt_size_min', 'pkt_size_max',
        'iat_mean', 'iat_std', 'frac_syn', 'frac_ack', 'frac_fin', 'frac_rst',
        'pkt_p25', 'pkt_p50', 'pkt_p75', 'pkt_p90', 'pkt_size_entropy',
        'is_src_port_common', 'is_dst_port_common',
        'label', 'capture_point'
    ]

    out_f = open(out_csv, 'w', newline='')
    w = csv.DictWriter(out_f, fieldnames=fieldnames)
    w.writeheader()

    flow_counter = 0
    for key, rows in pkts_by_5t.items():
        flow_counter += 1
        _write_flow(w, flow_counter, rows)

    out_f.close()


def _write_flow(writer, flow_id_num, rows):
    # compute stats
    times = [float(r.get('timestamp') or 0.0) for r in rows]
    start = min(times) if times else 0.0
    end = max(times) if times else 0.0
    duration = end - start if end > start else 0.001
    packet_count = len(rows)
    byte_count = 0
    labels = {}
    capture_point = rows[0].get('capture_point', '') if rows else ''
    pkt_sizes = []
    iats = []
    last_ts = None
    pkts_fwd = pkts_bwd = 0
    bytes_fwd = bytes_bwd = 0
    syn = ack = fin = rst = 0

    src, dst, sport, dport, proto = _row_to_5tuple(rows[0])

    for r in rows:
        try:
            bs = int(r.get('ip_len') or r.get('packet_size') or 0)
        except Exception:
            bs = 0
        byte_count += bs
        pkt_sizes.append(bs)

        ts = float(r.get('timestamp') or 0.0)
        if last_ts is not None:
            iats.append(ts - last_ts)
        last_ts = ts

        # directionality
        r_src = r.get('src_ip', '')
        if r_src == src:
            pkts_fwd += 1
            bytes_fwd += bs
        else:
            pkts_bwd += 1
            bytes_bwd += bs

        # tcp flags
        try:
            flags = int(r.get('tcp_flags') or 0)
        except Exception:
            flags = 0
        if flags & 0x02:
            syn += 1
        if flags & 0x10:
            ack += 1
        if flags & 0x01:
            fin += 1
        if flags & 0x04:
            rst += 1

        labels[r.get('label', 'normal')] = labels.get(r.get('label', 'normal'), 0) + 1

    packets_per_sec = packet_count / duration if duration > 0 else 0
    bytes_per_sec = byte_count / duration if duration > 0 else 0

    # size stats
    import statistics

    if pkt_sizes:
        pkt_mean = statistics.mean(pkt_sizes)
        pkt_min = min(pkt_sizes)
        pkt_max = max(pkt_sizes)
        try:
            pkt_std = statistics.stdev(pkt_sizes) if len(pkt_sizes) > 1 else 0.0
        except Exception:
            pkt_std = 0.0
        # percentiles
        pkt_sizes_sorted = sorted(pkt_sizes)
        def pct(p):
            if not pkt_sizes_sorted:
                return 0
            k = (len(pkt_sizes_sorted)-1) * (p/100.0)
            f = int(k)
            c = min(f+1, len(pkt_sizes_sorted)-1)
            if f == c:
                return pkt_sizes_sorted[f]
            d0 = pkt_sizes_sorted[f] * (c-k)
            d1 = pkt_sizes_sorted[c] * (k-f)
            return d0 + d1
        pkt_p25 = pct(25)
        pkt_p50 = pct(50)
        pkt_p75 = pct(75)
        pkt_p90 = pct(90)
        # entropy
        from collections import Counter
        cnt = Counter(pkt_sizes)
        total = sum(cnt.values())
        import math
        ent = 0.0
        for v in cnt.values():
            p = v / total
            ent -= p * math.log2(p)
        pkt_entropy = ent
    else:
        pkt_mean = pkt_std = pkt_min = pkt_max = 0
        pkt_p25 = pkt_p50 = pkt_p75 = pkt_p90 = 0
        pkt_entropy = 0.0

    if iats:
        iat_mean = statistics.mean(iats)
        try:
            iat_std = statistics.stdev(iats) if len(iats) > 1 else 0.0
        except Exception:
            iat_std = 0.0
    else:
        iat_mean = iat_std = 0.0

    frac_syn = syn / packet_count if packet_count else 0.0
    frac_ack = ack / packet_count if packet_count else 0.0
    frac_fin = fin / packet_count if packet_count else 0.0
    frac_rst = rst / packet_count if packet_count else 0.0

    # common port flags
    COMMON_PORTS = {80, 443, 53, 123, 22, 25, 3389, 8080}
    try:
        is_src_port_common = int(sport in COMMON_PORTS)
    except Exception:
        is_src_port_common = 0
    try:
        is_dst_port_common = int(dport in COMMON_PORTS)
    except Exception:
        is_dst_port_common = 0

    maj_label = max(labels.items(), key=lambda kv: kv[1])[0] if labels else 'normal'

    flow_row = {
        'flow_id': f'flow_{flow_id_num:07d}',
        'src_ip': src,
        'dst_ip': dst,
        'src_port': sport,
        'dst_port': dport,
        'protocol': proto,
        'start_time': start,
        'end_time': end,
        'duration': duration,
        'packet_count': packet_count,
        'byte_count': byte_count,
        'packets_per_sec': packets_per_sec,
        'bytes_per_sec': bytes_per_sec,
        'pkts_src_to_dst': pkts_fwd,
        'pkts_dst_to_src': pkts_bwd,
        'bytes_src_to_dst': bytes_fwd,
        'bytes_dst_to_src': bytes_bwd,
        'pkt_size_mean': pkt_mean,
        'pkt_size_std': pkt_std,
        'pkt_size_min': pkt_min,
        'pkt_size_max': pkt_max,
        'iat_mean': iat_mean,
        'iat_std': iat_std,
        'frac_syn': frac_syn,
        'frac_ack': frac_ack,
        'frac_fin': frac_fin,
        'frac_rst': frac_rst,
        'pkt_p25': pkt_p25,
        'pkt_p50': pkt_p50,
        'pkt_p75': pkt_p75,
        'pkt_p90': pkt_p90,
        'pkt_size_entropy': pkt_entropy,
        'is_src_port_common': is_src_port_common,
        'is_dst_port_common': is_dst_port_common,
        'label': maj_label,
        'capture_point': capture_point,
    }

    writer.writerow(flow_row)


if __name__ == '__main__':
    main()
