# DDoSimu5G Technical Reference

**Version:** 1.0  
**Author:** Karim Khalil, PhD — EIT, Lund University

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Dynamic Traffic Architecture](#2-dynamic-traffic-architecture)
3. [Attack Framework](#3-attack-framework)
4. [Traffic Labeling](#4-traffic-labeling)
5. [PCAP Capture](#5-pcap-capture)
6. [Configuration Reference](#6-configuration-reference)
7. [IP Spoofing](#7-ip-spoofing)
8. [Extending the Framework](#8-extending-the-framework)

---

## 1. System Overview

DDoSimu5G operates as a set of OMNeT++ modules layered on top of INET 4.5 and Simu5G 1.2.2. The core components are:

```
┌─────────────────────────────────────────────────────────────┐
│ DataTrafficController (Orchestrator)                        │
│ ├─ Reads infection timeline JSON                            │
│ ├─ Schedules infection events at timestamps                 │
│ ├─ Instantiates attack apps via AttackRegistry              │
│ └─ Coordinates benign (slots 0-9) + attack (slots 10-19)   │
└─────────────────────────────────────────────────────────────┘
            │                             │
            ▼                             ▼
┌──────────────────────────┐  ┌──────────────────────────────┐
│ DynamicTrafficSender     │  │ BaseAttackApp → Subclasses   │
│ (Benign traffic)         │  │ (Adversarial traffic)        │
│ Protocol handlers:       │  │ UdpFlood, TcpSynFlood,      │
│ UDP, TCP, DNS, HTTP      │  │ DnsAmplification, HttpFlood  │
│ Slots 0-9                │  │ Slots 10-19                  │
└──────────────────────────┘  └──────────────────────────────┘
            │                             │
            └──────────────┬──────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ TrafficLabeler (Ground Truth Generator)                     │
│ ├─ Logs: timestamp, module, protocol, 5-tuple, size         │
│ ├─ Labels: benign vs malicious                              │
│ └─ Includes spoofing metadata if enabled                    │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Dual-Vantage PCAP Capture                                   │
│ ├─ gNodeB: GTP-U encapsulated (per base station)           │
│ └─ UPF: Decapsulated IP (aggregated, Wireshark-ready)      │
└─────────────────────────────────────────────────────────────┘
```

### Source Code Layout

```
src/ddosimu5g/
├── apps/
│   ├── dynamic/                   # Benign traffic generation
│   │   ├── DynamicTrafficSender   # Multi-protocol sender orchestrator
│   │   ├── DynamicTrafficReceiver # Server-side receiver orchestrator
│   │   ├── protocols/             # Sender-side protocol handlers
│   │   │   ├── ProtocolHandler    # Abstract base
│   │   │   ├── UdpProtocolHandler
│   │   │   ├── TcpProtocolHandler
│   │   │   ├── DnsProtocolHandler
│   │   │   ├── HttpProtocolHandler
│   │   │   └── receiver/          # Server-side handlers
│   │   │       ├── ReceiverProtocolHandler
│   │   │       ├── UdpReceiverHandler
│   │   │       ├── TcpReceiverHandler
│   │   │       ├── DnsReceiverHandler
│   │   │       └── HttpReceiverHandler
│   │   └── trafficlabel/          # Ground-truth labeling
│   │       ├── TrafficLabeler
│   │       └── LabelEntry
│   └── adversarialApps/           # Attack traffic generation
│       ├── BaseAttackApp          # Abstract base for all attacks
│       └── ddos/
│           ├── UdpFloodAttack
│           ├── TcpSynFloodAttack
│           ├── DnsAmplificationAttack
│           ├── HttpFloodAttack
│           ├── AttackMarkers      # IPv4 TOS/DSCP markers per attack type
│           ├── AttackTracker      # Singleton: tracks active spoofed bots
│           ├── CoAPFloodAttack    # Planned
│           ├── MqttFloodAttack    # Planned
│           └── LwM2MFloodAttack   # Planned
├── common/
│   ├── SilentTcp                  # Drops TCP RST back-scatter on closed ports
│   └── RotatingPcapRecorder       # PCAP file rotation by size or packet count
├── trafficcontroller/
│   ├── DataTrafficController      # Infection timing orchestrator
│   └── AttackRegistry             # Factory pattern for attack instantiation
└── simcommands/
    └── PostSimulationCommand      # Post-simulation processing
```

---

## 2. Dynamic Traffic Architecture

### Design

A single `DynamicTrafficSender` instance hosts multiple concurrent **protocol handlers**, each managing its own sockets, timers, and state. Device behavior is defined in JSON profiles — no C++ changes needed to model new device types.

```
DynamicTrafficSender (single app instance)
    ├─ UdpProtocolHandler    (UDP sockets, send timers)
    ├─ TcpProtocolHandler    (TCP connections, data transfers)
    ├─ DnsProtocolHandler    (DNS queries/responses)
    └─ HttpProtocolHandler   (HTTP sessions over TCP)
```

### Application Slot Model

Each UE has a fixed pool of application slots:

| Slots | Purpose |
|-------|---------|
| 0–9 | Benign traffic (`DynamicTrafficSender` instances) |
| 10–19 | Attack applications (placeholders, activated at infection time) |

All 10 sender apps read the same JSON profile. Each processes only schedule entries matching its `app_id`, enabling concurrent multi-protocol execution.

### Protocol Handler Interface

```cpp
class ProtocolHandler {
    virtual void initialize(cModule* parent, int appId) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void handleMessage(cMessage* msg) = 0;
    virtual void handlePacket(Packet* pkt) = 0;
};
```

### Available Traffic Profiles

| Profile | Device Type | Protocols | 5G Use Case |
|---------|-------------|-----------|-------------|
| `smartphone_user.json` | Smartphone | TCP, HTTP, UDP, DNS | eMBB |
| `video_streamer.json` | Video device | TCP, HTTP, UDP, DNS | eMBB |
| `iot_wearable_health_monitor.json` | Health tracker | UDP, TCP, HTTP, DNS | mMTC |
| `iot_smart_meter.json` | Utility meter | UDP, HTTP, DNS | mMTC |
| `iot_connected_vehicle.json` | Connected car | UDP, TCP, HTTP, DNS | V2X |
| `iot_drone_control.json` | Drone | UDP, TCP | URLLC |
| `iot_industrial_sensor.json` | Industrial sensor | UDP, DNS | mMTC |
| `iot_asset_tracker.json` | Asset tracker | UDP, DNS | mMTC |

---

## 3. Attack Framework

### Class Hierarchy

```
ApplicationBase (INET)
  └─ BaseAttackApp (abstract)
      ├─ UdpFloodAttack
      ├─ TcpSynFloodAttack
      ├─ DnsAmplificationAttack
      ├─ HttpFloodAttack
      ├─ CoAPFloodAttack     (planned)
      ├─ MqttFloodAttack     (planned)
      └─ LwM2MFloodAttack    (planned)
```

### Attack Lifecycle

1. **Placeholder phase** — `BaseAttackApp` modules pre-allocated with `startTime = -1` (inactive)
2. **Infection event** — `DataTrafficController` reads infection timeline, finds placeholder, calls `BaseAttackApp::createFromProfile()`
3. **Instantiation** — `AttackRegistry` maps `attackType` string to concrete module type; placeholder is replaced
4. **Dormant phase** — Optional delay (`dormantDuration`) before attack begins
5. **Active phase** — Attack runs with configured rate pattern (constant/ramp/burst)
6. **Termination** — `stopAttackTimer` fires after `attackDuration`

### Attack Styles

| Style | Rate Pattern | Description |
|-------|-------------|-------------|
| **intense** | Constant high rate | Maximum impact flooding |
| **stealthy** | Slow linear ramp | Evade rate-based detection |
| **pulsing** | ON/OFF cycles | Intermittent bursts |
| **ramping** | Fast linear ramp | Aggressive escalation |
| **slowrate** | Constant low rate | Under-the-radar persistent |

### Behavior Modes

Controls interaction between attack and benign traffic on the same UE:

| Mode | Effect on Benign Traffic |
|------|------------------------|
| **coexistence** | Continues unchanged (realistic botnet) |
| **replace** | Stopped completely (device takeover) |
| **hybrid** | Rate reduced by 50% (partial hijacking) |

### Self-Registration

Attack classes register with `AttackRegistry` using static initializers:

```cpp
namespace {
    struct UdpFloodRegistrar {
        UdpFloodRegistrar() {
            AttackRegistry::registerAttack("udp_flood",
                "ddosimu5g.apps.adversarialApps.ddos.UdpFloodAttack");
        }
    };
    static UdpFloodRegistrar registrar;
}
```

### IPv4 Attack Markers

`AttackMarkers` embeds distinguishing values in the IPv4 TOS byte of every attack packet. These markers survive GTP-U tunneling and are visible in PCAPs for offline traffic classification:

| Attack Type | TOS | DSCP | Purpose |
|-------------|------|------|---------|
| UDP Flood | `0xD0` | 52 | Identifies UDP flood traffic in captures |
| TCP SYN Flood | `0xD2` | 52 | Distinguishes SYN flood from benign TCP |
| HTTP Flood | `0xD4` | 53 | Tags HTTP-layer attack traffic |
| DNS Amplification | `0xD8` | 54 | Marks amplification queries |

Each attack's `sendAttackPacket()` sets TOS via `Ipv4Header::setTypeOfService()` before sending.

### Attack Tracker

`AttackTracker` is a singleton that maintains global state across the simulation. Currently used by DNS amplification attacks to register bots so the DNS receiver can suppress responses (simulating amplified replies redirected to the victim). See [IP Spoofing](#7-ip-spoofing) for details.

### SilentTcp — RST Back-Scatter Suppression

When a TCP SYN flood targets a port with no listener, INET's standard `Tcp` module creates a temporary connection that replies with RST. At flood rates this generates thousands of downlink RST packets that inflate PCAPs and pollute traffic labels.

`SilentTcp` extends `inet::tcp::Tcp` and overrides `segmentArrivalWhileClosed()` to silently discard the segment instead of replying with RST:

```ini
# Apply to any server receiving SYN flood traffic
*.generalServer.tcp.typename = "ddosimu5g.common.SilentTcp"
```

This is essential for realistic SYN flood simulation — without it, the RST back-scatter creates a 1:1 downlink response for every attack SYN, which does not occur in real networks where spoofed source IPs prevent RST delivery.

---

## 4. Traffic Labeling

### CSV Label Structure

Every packet (benign and malicious) is logged to CSV with ground-truth labels:

```csv
timestamp,packet_num,src_module,src_ip,src_port,direction,traffic_type,protocol,dest_ip,dest_port,packet_size,attack_type,label,spoofing_enabled,spoofed_src_ip
```

**Label values:**
- `benign` — Normal traffic from `DynamicTrafficSender`
- `malicious` — Attack traffic from `BaseAttackApp` subclasses

### Label File Organization

Each application instance writes to a unique file:

```
labels/ue{ueId}/app{appId}.csv
```

Labels are synchronized with PCAP timestamps, enabling direct correlation between ground-truth and network captures.

---

## 5. PCAP Capture

### Dual-Vantage Recording

Traffic is captured at two network points simultaneously:

| Capture Point | Format | File | Use Case |
|--------------|--------|------|----------|
| **gNodeB** | GTP-U encapsulated | `gnb{N}.pcap` (per base station) | Cell-level traffic distribution, RAN analysis |
| **UPF** | Decapsulated IP | `upf.pcap` (aggregated) | Protocol analysis, DDoS detection research |

### gNodeB Encapsulation

```
┌──────────────────────┐
│ Custom Header (4B)   │  ff 03 00 21
├──────────────────────┤
│ Outer IP             │  gNodeB ↔ iUpf addressing
├──────────────────────┤
│ UDP (port 31→31)     │
├──────────────────────┤
│ Tunnel Header (8B)   │
├──────────────────────┤
│ Inner IP             │  UE's actual IP packet
├──────────────────────┤
│ Application Data     │  TCP/UDP/DNS/HTTP payload
└──────────────────────┘
```

### Configuration

```ini
# gNodeB recording
*.gnb1.pppIf.numPcapRecorders = 1
*.gnb1.pppIf.pcapRecorder[0].pcapFile = "run_results/pcaps/${configname}/${datetime}/gnb1.pcap"
*.gnb1.pppIf.pcapRecorder[0].alwaysFlush = true

# UPF recording
*.upf.pppIf.numPcapRecorders = 1
*.upf.pppIf.pcapRecorder[0].pcapFile = "run_results/pcaps/${configname}/${datetime}/upf.pcap"
*.upf.pppIf.pcapRecorder[0].alwaysFlush = true
```

For most analysis and ML dataset generation, the **UPF PCAP** is recommended due to its standard IP format compatible with Wireshark, tshark, and Zeek.

### RotatingPcapRecorder

`RotatingPcapRecorder` extends INET's `PcapRecorder` with automatic file rotation when a packet-count or file-size threshold is reached. Rotated files are named `originalname_001.pcapng`, `originalname_002.pcapng`, etc.

```ini
*.upf.pppIf.pcapRecorder[0].typename = "ddosimu5g.common.RotatingPcapRecorder"
*.upf.pppIf.pcapRecorder[0].enableFileRotation = true
*.upf.pppIf.pcapRecorder[0].maxPacketsPerFile = 100000
*.upf.pppIf.pcapRecorder[0].maxMegabytesPerFile = 100
```

This prevents multi-gigabyte single PCAPs in long simulation runs.

---

## 6. Configuration Reference

### File Structure

```
Test-cases-002/
├── config/
│   ├── infectionTraces/                    # WHEN infections occur
│   │   └── infection_*.json
│   ├── trafficSchedules/                   # WHAT traffic to generate
│   │   ├── attack_*.json                   # Attack profiles
│   │   ├── iot_*.json                      # IoT device profiles
│   │   ├── smartphone_user.json
│   │   └── video_streamer.json
│   └── network/                            # Network topology configs
│       ├── hosts_net_config.xml
│       └── internetcloud_config_dynamic.xml
├── scenarios/                              # OMNeT++ INI configurations
│   ├── TC-002-baseline-stationary.ini
│   └── TC-002-dynamic-traffic.ini
└── scripts/                                # Run and analysis scripts
```

### Traffic Profile JSON

```json
{
  "deviceType": "smart_meter",
  "description": "Smart meter with UDP telemetry and HTTP updates",
  "schedule": [
    {
      "app_id": 0,
      "type": "udp",
      "sendInterval": 60.0,
      "packetSize": 96,
      "destAddress": "iotServer",
      "destPort": 5000,
      "startDelay": 5.0,
      "duration": 3600.0
    },
    {
      "app_id": 1,
      "type": "dns",
      "queryInterval": 120.0,
      "queryDomain": "iotServer",
      "dnsServer": "8.8.8.8",
      "startDelay": 0.0
    }
  ]
}
```

**Supported traffic types:** `udp`, `tcp`, `http`, `dns`, `idle`

### Attack Profile JSON

```json
{
  "attackProfile": {
    "attackType": "udp_flood",
    "attackStyle": "intense",
    "behaviorMode": "coexistence"
  },
  "timing": {
    "dormantDuration": 0.0,
    "attackDuration": 60.0
  },
  "attackParameters": {
    "initialRate": 100,
    "peakRate": 1000,
    "rampDuration": 20.0,
    "burstOnDuration": 5.0,
    "burstOffDuration": 3.0,
    "packetSize": 1024,
    "enableSpoofing": false
  },
  "targetConfiguration": {
    "targets": ["generalServer"],
    "targetPort": 9999
  }
}
```

**Available attack types:** `udp_flood`, `tcp_syn_flood`, `dns_amplification`, `http_flood`

### Infection Timeline JSON

```json
{
  "infectionData": [
    {"node_id": 0, "malware_active_time": 10.0},
    {"node_id": 1, "malware_active_time": 25.0}
  ]
}
```

### INI Configuration

```ini
# Benign traffic (slots 0-9)
*.dynamicUe[*].numApps = 11
*.dynamicUe[*].app[0..9].typename = "DynamicTrafficSender"
*.dynamicUe[*].app[0..9].trafficScheduleFile = "config/trafficSchedules/iot_smart_meter.json"

# Attack placeholder (slot 10)
*.dynamicUe[*].app[10].typename = "BaseAttackApp"
*.dynamicUe[*].app[10].startTime = -1
*.dynamicUe[*].app[10].attackProfileFile = "config/trafficSchedules/attack_udp_intense_coexist.json"

# Controller
*.dataTrafficController.enableAttackApps = true
*.dataTrafficController.infectionDataFile = "config/infectionTraces/infection_single_ue.json"
```

### Parameter Precedence

1. **JSON profile** (highest priority)
2. **NED parameters** (fallback)
3. **Code defaults** (`AttackConfig` constructor)

---

## 7. IP Spoofing

Real IP spoofing is not possible in OMNeT++/INET (network stack validates source addresses) and is blocked in real 5G networks by Source Address Validation (BCP 38 / RFC 2827). DDoSimu5G uses two alternative approaches:

### UDP Flood — Metadata Labels

Packets send with the real source IP. A random spoofed IP is generated per packet and recorded in the CSV label as `spoofed_src_ip`. This enriches the dataset for ML models that need to detect spoofing patterns.

### TCP SYN Flood — SilentTcp

SYN floods use spoofed source IPs in reality, so the target never receives valid RST replies. To replicate this in simulation, `SilentTcp` replaces INET's standard TCP on target servers, silently dropping SYN segments to closed ports instead of generating RST back-scatter. See [SilentTcp](#silenttcp--rst-back-scatter-suppression) in Section 3.

### DNS Amplification — Response Suppression

1. Bot registers its IP in the `AttackTracker` singleton when attack starts
2. DNS server checks `AttackTracker` before sending responses
3. Registered bot → response suppressed (simulates amplified response going to victim)
4. Bot unregisters when attack stops

The victim IP is recorded in labels as `spoofed_src_ip` for dataset enrichment.

---

## 8. Extending the Framework

### Adding a New Attack Type

1. Create a subclass of `BaseAttackApp`:

```cpp
class NewAttack : public BaseAttackApp {
protected:
    void sendAttackPacket() override {
        // Protocol-specific packet generation
    }
};
```

2. Register with `AttackRegistry`:

```cpp
namespace {
    struct NewAttackRegistrar {
        NewAttackRegistrar() {
            AttackRegistry::registerAttack("new_attack",
                "ddosimu5g.apps.adversarialApps.ddos.NewAttack");
        }
    };
    static NewAttackRegistrar registrar;
}
```

3. Define NED module type
4. Create JSON profile in `config/trafficSchedules/`

### Adding a New Protocol Handler

1. Create a subclass of `ProtocolHandler`:

```cpp
class MqttProtocolHandler : public ProtocolHandler {
    void initialize(cModule* parent, int appId) override { /* ... */ }
    void start() override { /* ... */ }
    void handleMessage(cMessage* msg) override { /* ... */ }
};
```

2. Register the type string (e.g., `"mqtt"`) in `DynamicTrafficSender::createProtocolHandler()`
3. Add corresponding `MqttReceiverHandler` for server-side handling
4. Create JSON traffic profiles using the new type

### Design Patterns

| Pattern | Component | Purpose |
|---------|-----------|---------|
| **Factory** | `AttackRegistry` | Decouples attack instantiation from orchestration |
| **Strategy** | `ProtocolHandler` hierarchy | Protocol-specific behavior behind common interface |
| **Singleton** | `AttackTracker` | Global DNS amplification bot registry |
| **Template Method** | `BaseAttackApp` | Attack lifecycle skeleton with subclass hooks |
| **Observer** | `TrafficLabeler` | Decoupled packet logging from traffic generation |
| **Placeholder** | App slots 10–19 | Pre-allocated modules activated at infection time |

---

**Document Version:** 1.0  
**Last Updated:** March 2026
