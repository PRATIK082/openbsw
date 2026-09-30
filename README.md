<!--
 *******************************************************************************
  Copyright (c) 2024 Accenture

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
 *******************************************************************************
-->

# Eclipse OpenBSW


## Build Status 🚀

[![Build S32k, STM32 and posix platform](https://github.com/eclipse-openbsw/openbsw/actions/workflows/build.yml/badge.svg?branch=main&event=push)](https://github.com/eclipse-openbsw/openbsw/actions/workflows/build.yml)

## Code Coverage 

| Code Coverage            | Status                                                                 |
|--------------------------|------------------------------------------------------------------------|
| Line Coverage            | [![Line Coverage](https://eclipse-openbsw.github.io/openbsw/coverage_badges/line_coverage_badge.svg)](https://github.com/eclipse-openbsw/openbsw/actions/workflows/code-coverage.yml) |
| Function Coverage        | [![Function Coverage](https://eclipse-openbsw.github.io/openbsw/coverage_badges/function_coverage_badge.svg)](https://github.com/eclipse-openbsw/openbsw/actions/workflows/code-coverage.yml) |

## Overview

Eclipse OpenBSW is an open source SDK to build professional, high quality embedded software products. 
It is a software stack specifically designed and developed for automotive applications.

This repository provides the complete code, documentation and a reference example
that works out of the box without any specific hardware requirements (any POSIX platform)
allowing developers to get up and running quickly.

## Target Audience

* **Open Source Enthusiasts**: Enthusiasts and hobbyists passionate about automotive technology
  and interested in contributing to open source projects, collaborating with like-minded
  individuals and exploring new ideas and projects in the automotive domain.

* **Embedded Systems Developers**: Developers specializing in embedded systems programming,
  microcontroller firmware development and real-time operating systems (RTOS), who are interested
  in automotive applications.

* **Automotive Engineers**: Professionals working in the automotive industry, including engineers,
  designers and technicians, who are interested in developing and improving automotive
  technologies, systems and components.

* **Students and Researchers**: Students, researchers, and academic institutions interested in
  learning about automotive technologies, conducting research, and exploring innovative solutions
  in automotive areas.

## Getting Started

To get started, we recommend to compile our reference application for one of the supported platforms
using the docker image we provide including all the necessary tools. Therefore, you can simply run
the development service in the docker compose in the root of the repo, call cmake with the correct
options and build the generated project.

> [!NOTE]
> 
> In case your local user already uses `UID`/`GID` of `1000` you can skip the `DOCKER_UID` and
> `DOCKER_GID` variables, since this is the default. Otherwise, you need it to make sure you have
> proper access to your local files.
> 
> In case you want to use a custom history file for the commands you run in the container you can
> also set the `DOCKER_HISTORY` variable, which defaults to the `~/.docker_history` file.
>
> Note, that we bind mount your current working directory into the container and use it as working
> directory. This makes sure, that you will have the same paths inside and outside of the container
> when for example loading a generated elf into a debugger or following symlinks created within the
> container.

### Building with CMake
```
host> DOCKER_UID=$(id -u) DOCKER_GID=$(id -g) docker compose run --build development
docker> cmake --preset posix-freertos
docker> cmake --build --preset posix-freertos
```

### Building with Bazel
From inside the same container:

Build all targets for host platform

```
docker> bazel build //...
```

Build all targets for s32k148 platform

```
docker> bazel build --config=s32k148 //...
```

To run all tests for host platform and s32k148

```
docker> bazel test //...
docker> bazel test --config=s32k148 //...
```

> [!NOTE]
>
> The above builds every target for the host and for the S32K148 target respectively.
> To build or test a single target, use its Bazel label, e.g.:
>
> ```
> docker> bazel build //libs/bsw/util:util
> docker> bazel build --config=s32k148 //libs/bsw/util:util
> ```

## Feature Overview

### Implemented Features

| Feature | Description | POSIX Support | S32K148 Support | New? |
| --- | --- | --- | --- | --- |
| Modular design | Based on each project's needs, required software modules can easily be included or excluded. | Yes | Yes | |
| Application Lifecycle Management | The order in which Applications/Features are brought up/down is easily organised. | Yes | Yes | |
| Console | A console is provided for diagnostic and development purposes. | In a terminal interface | Via UART | |
| Commands | Commands can easily be added to the console to aid development, test and debugging. | Yes | Yes | |
| Logging | Diagnostic logging is implemented per software component. | Yes | Yes | |
| CAN | Support for CAN bus communication | If ``SocketCAN`` is supported | Yes | Since Release 0.1 |
| Sensors and actuators integration | ADC, PWM & GPIO | | Yes | |
| UDS, DoCAN | Diagnostics over CAN | If ``SocketCAN`` is supported | Yes | Since Release 0.1 |
| Ethernet | Basic TCP and UDP support | Yes | Yes | On current `main` |
| Storage | Persistent data storage on EEPROM and Flash | Yes | Yes | On current `main` |

## Roadmap
**

# OpenBSW Phase Roadmap: Complete Automotive Communication Stack

## **PHASE 1-4: Foundation (Already Defined)**
- ✅ Phase 1: CAN Stack (Signal-based, Multi-channel)
- ✅ Phase 2: LIN + Ethernet + Signal Gateway + Timeout/TX Confirmation
- ✅ Phase 3: Frame Gateway + TP (ISO-TP) + Diagnostics (UDS/DoIP) + XCP
- ✅ Phase 4: Production Integration (Reuse OpenBSW modules, blob config)
- ✅ Phase 5: ECU Mode Management (NORMAL/STANDBY/SLEEP/DIAGNOSTIC)

***

## **PHASE 6: Memory & Persistence (NVRAM Manager)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **Non-volatile data blocks** | Persistent Memory (NVRAM Manager) | `Persistent Memory_Manager` with block-based storage |
| **RAM/EEPROM/Flash abstraction** | Persistent Memory + Fee/Eep | Unified storage backend (Flash + RAM mirror) |
| **Async read/write** | Persistent Memory_AsyncRead/Persistent Memory_AsyncWrite | Async API with callbacks |
| **CRC protection** | Persistent Memory block CRC | CRC-32/16 per block |
| **Wear leveling** | Fee (Flash EEPROM Emulation) | Sector-based wear leveling |
| **Immediate vs deferred storage** | Persistent Memory immediate/during-shutdown | Configurable per block |
| **Diagnostic block access** | Persistent Memory diagnostic services | UDS service 0x23/0x3E integration |

**Key Use Cases**:
- Calibration data persistence (XCP writable variables)
- Fault memory storage (DTCs, freeze frames)
- Vehicle configuration (VIN, variant coding)
- Odometer, trip meters (tamper-resistant)

**Safety**: ASIL-B support with CRC + redundancy

***

## **PHASE 7: Diagnostic Event Manager (Fault/Event Manager)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **DTC storage** | Fault/Event Manager (Diagnostic Event Manager) | DTC buffer with severity levels |
| **Freeze frame** | Fault/Event Manager freeze-frame data | Snapshot of signals at fault time |
| **Extended data** | Fault/Event Manager extended data records | Custom data per DTC |
| **Aging & status bits** | Fault/Event Manager testFailed, pending, confirmed | 5-byte DTC status (ISO 14229) |
| **Operation cycles** | Fault/Event Manager operation cycle monitoring | Ignition/key cycle tracking |
| **Event memory entry** | Fault/Event Manager primary/secondary memory | Priority-based DTC storage |
| **Persistent Memory integration** | Fault/Event Manager → Persistent Memory | Phase 6 Persistent Memory for persistent storage |
| **Fault/Event Manager callbacks** | Fault/Event Manager event callbacks | Application fault notifications |

**Key Use Cases**:
- OBD-II/WWH-OBD compliance
- Manufacturer-specific DTCs (P-codes, U-codes)
- Readiness monitoring (I/M flags)
- Permanent DTCs (emissions-related)

**Safety**: ISO 14229-7 (DTC format), WWH-OBD (global OBD)

***

## **PHASE 8: End-to-End (E2E) Protection**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **E2E Profile 1** | E2E P01 (CRC + Counter) | 8-bit CRC + 4-bit counter |
| **E2E Profile 2** | E2E P02 (CRC + Counter + DataID) | CRC-8 + counter + 4-bit data ID |
| **E2E Profile 4** | E2E P04 (CRC-16 + Counter) | CRC-16 CCITT + 16-bit counter |
| **E2E Profile 5** | E2E P05 (CRC + Counter + DataID) | CRC-16 + counter + 8-bit data ID |
| **E2E Profile 11** | E2E P11 (CRC-32 + Counter) | CRC-32 + 32-bit counter (high safety) |
| **E2E Profile 22** | E2E P22 (CRC-8 + Rolling Counter) | CRC-8 + 8-bit rolling counter |
| **Signal protection** | Interface Layer E2E protection | Per-signal E2E wrapper |
| **Timeout + alive counter** | E2E alive monitoring | Signal timeout + counter check |

**Key Use Cases**:
- ASIL-D communication (steering, braking)
- Cross-ECU signal validation
- ISO 26262 compliance (fault detection)
- Safety-related signal integrity (CRC + sequence)

**Safety**: ISO 26262-6 (E2E protection), ASIL-D capable

***

## **PHASE 9: Network Management (NM)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **CAN NM** | CanNm ( NM) | NM PDU with node ID, partial networking |
| **LIN NM** | LinNm | LIN NM frame (schedule table coordination) |
| **Ethernet NM** | EthNm (SOME/IP NM) | SOME/IP service discovery + NM |
| **Partial networking** | NM cluster, partial networking | Selective wake-up (gateway, zonal) |
| **NM coordination** | NM immediate, repetition | NM timeout, repeat timer |
| **Sleep/wake synchronization** | NM sleep mode entry | Coordinated ECU sleep across network |
| **NM PDU routing** | NM gateway | NM pass-through (gateway ECU) |
| **NM callback** | NM callback (state change) | Application notification (bus active/sleep) |

**Key Use Cases**:
- Vehicle sleep/wake coordination
- Partial networking (wake only relevant ECUs)
- Gateway NM routing (central/zonal)
- SOME/IP service discovery (SD)

**Safety**: ISO 26262 (network state consistency)

***

## **PHASE 10: Multi-Core & Partitioning (Safety/Security)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **Multi-core scheduling** | OS multi-core (spinlock, task affinity) | Core pinning, lock-free queues |
| **Inter-core communication** | Interface Layer multi-core, spinlocks | Shared memory + IPC (message passing) |
| **Memory protection** | MPU/MMU, OS partitions | ARMv8-M SAU/IDAU, TrustZone |
| **Temporal isolation** | OS schedule tables, timing protection | Watchdog per core, execution time monitoring |
| **Spatial isolation** | OS memory protection | MPU regions, stack overflow detection |
| **Resource locking** | Spinlocks, mutexes | Priority inheritance, deadlock detection |
| **Core load balancing** | OS load balancing | Dynamic task migration |
| **Hypervisor support** | Type-1 hypervisor (optional) | Bare-metal or RTOS per core |

**Key Use Cases**:
- HPC (High-Performance Computer) with 4-8 cores
- ASIL-D/ASIL-B partitioning (different safety levels on same chip)
- Mixed-criticality (ADAS + infotainment on same SoC)
- Zonal controller with dual-core MCU

**Safety**: ISO 26262-5 (multi-core safety), ASIL-D support

***

## **PHASE 11: SOME/IP & Service Discovery (Ethernet)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **SOME/IP** | SoAd (Socket Adaptor) + SOME/IP | UDP/TCP SOME/IP stack |
| **Service Discovery (SD)** | Sd (Service Discovery) | SOME/IP-SD (offer/subscribe) |
| **Method calls** | SOME/IP method request/response | RPC over SOME/IP |
| **Events** | SOME/IP event notifications | Publisher/subscriber pattern |
| **Field notifications** | SOME/IP fields (getter/setter) | Stateful data exchange |
| **TP for SOME/IP** | SoAd TP (segmentation) | SOME/IP TP (>1472 bytes) |
| **DoIP integration** | DoIP (ISO 13400) | Diagnostic over IP (UDS tunneling) |
| **QoS handling** | SOME/IP QoS (priority, VLAN) | VLAN tagging, priority queues |

**Key Use Cases**:
- Central compute (HPC) communication
- Zonal gateway → HPC backbone
- Service-oriented architecture (SOA)
- ADAS sensor data (camera, radar over Ethernet)

**Safety**: ASIL-B (Ethernet communication)

***

## **PHASE 12: DDS (Data Distribution Service)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **DDS publisher/subscriber** | DDS (Data Distribution Service) | OMG DDS implementation |
| **QoS policies** | DDS reliability, durability, deadline | Reliability, history, liveliness QoS |
| **Topics** | DDS topic-based pub/sub | Topic registration, type support |
| **Discovery** | DDS RTPS (Real-Time Publish-Subscribe) | RTPS discovery protocol |
| **Type system** | DDS IDL (Interface Definition Language) | IDL compilation, type introspection |
| **Security** | DDS security (authentication, encryption) | DDS-Security (optional) |
| **Gateway** | DDS ↔ SOME/IP gateway | Protocol translation (DDS ↔ SOME/IP) |
| **Zero-copy** | DDS shared memory transport | Shared memory for intra-ECU DDS |

**Key Use Cases**:
- SDV (Software-Defined Vehicle) backbone
- Cloud-to-vehicle communication
- ROS 2 integration (autonomous driving)
- High-throughput sensor fusion (LiDAR, camera)

**Safety**: ASIL-B (DDS for non-safety), ASIL-D (with redundancy)

***

## **PHASE 13: Security & Cryptography (ISO 21434)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **SecOC (Secure Onboard Communication)** | SecOC (authenticated PDUs) | MAC (Message Authentication Code) per PDU |
| **TLS/DTLS** | Tls (TLS stack) | mbedTLS integration (HTTPS, DoIP security) |
| **Certificate management** | Csm (Crypto Service Manager) | X.509 certificate storage, validation |
| **Secure boot** | SecOC bootloader | Boot image signature verification |
| **Key management** | Key management (HSM integration) | NVRAM-protected key storage |
| **Intrusion detection** | IDs (Intrusion Detection System) | Anomaly detection (CAN/Ethernet traffic) |
| **Secure diagnostics** | Secured UDS (0x27 with seed/key) | UDS security access (level 1-3) |
| **CAN FD encryption** | CAN FD with encryption (optional) | AES-128 per CAN FD frame (experimental) |

**Key Use Cases**:
- ISO 21434 (cybersecurity compliance)
- UN R155 (CSMS certification)
- Secure OTA updates
- Protected diagnostic access (anti-tampering)

**Safety**: ISO 21434 (cybersecurity), ASIL-B (security mechanisms)

***

## **PHASE 14: OTA (Over-The-Air Updates)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **FOTA (Firmware OTA)** | FOTA (firmware updates) | Dual-bank flash, A/B updates |
| **SOTA (Software OTA)** | SOTA (application updates) | Containerized app updates |
| **Delta updates** | Differential updates | Binary diff/patch (bsdiff) |
| **Rollback** | Rollback on failure | Boot counter, rollback trigger |
| **Campaign management** | Update campaigns | Update scheduling, dependency check |
| **Security** | Signed updates | RSA/ECDSA signature verification |
| **Progress reporting** | Update progress | UDS service 0x36 (transfer data) |
| **Post-update validation** | Post-update self-test | Application health check |

**Key Use Cases**:
- Remote ECU reprogramming (OEM campaigns)
- Feature-on-Fault/Event Managerand (FOD) activation
- Bug fixes, recalls (remote)
- Calibration updates (XCP replacement)

**Safety**: ISO 24089 (software update engineering), ASIL-B (rollback safety)

***

## **PHASE 15: Application Framework (Client/Server, Publisher/Subscriber)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **RTE-like API** | Interface Layer (Run-Time Environment) | Signal-based API (publisher/subscriber) |
| **Client/server** | Interface Layer client/server ports | RPC over SOME/IP or DDS |
| **Sender/receiver** | Interface Layer sender/receiver ports | Pub/sub over DDS or SOME/IP |
| **Mode switches** | Interface Layer mode switch ports | EcuMode request API |
| **Data validation** | Interface Layer data validation | Range check, plausibility check |
| **Application lifecycle** | Interface Layer lifecycle management | Application start/stop hooks |
| **Inter-application IPC** | Interface Layer inter-SWC communication | Shared memory, message queues |
| **Application sandboxing** | OS partitions | Per-application memory regions |

**Key Use Cases**:
- Application-to-application communication
- Microservices architecture (SDV)
- Feature deployment (containerized apps)
- Third-party app integration (app store)

**Safety**: ASIL-B (inter-app isolation)

***

## **PHASE 16: Advanced Diagnostics (UDS Services, OBD-II)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **UDS all services** | Dcm (Diagnostic Comm Manager) | Full UDS (ISO 14229-1) 0x10-0x3E |
| **OBD-II modes** | OBD-II (SAE J1979) | Modes 01-0A (PIDs, DTCs, readiness) |
| **WWH-OBD** | WWH-OBD (ISO 27145) | Global OBD compliance |
| **Diagnostic routines** | Dcm routine control (0x31) | Custom diagnostic routines |
| **DID read/write** | Dcm data identifiers (0x22/0x2E) | Standard + OEM-specific DIDs |
| **IO control** | Dcm IO control (0x2F) | Actuator testing, sensor override |
| **Periodic transmission** | Dcm periodic transmission (0x2A) | Scheduled signal transmission |
| **Response pending** | Dcm response pending (0x78) | Long-operation handling |

**Key Use Cases**:
- OBD-II compliance (emissions, safety)
- Manufacturer-specific diagnostics
- End-of-line (EOL) programming
- Remote diagnostics (connected vehicle)

**Safety**: ISO 14229 (UDS), WWH-OBD (global OBD)

***

## **PHASE 17: Time-Sensitive Networking (TSN)**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **802.1Qbv (TAS)** | TSN time-aware shaper | Scheduled traffic (time-gated queues) |
| **802.1Qbu (Frame preemption)** | TSN frame preemption | Interrupt large frames with critical |
| **802.1Qcc (Stream reservation)** | TSN stream reservation | Centralized network config (CNC) |
| **802.1AS (gPTP)** | TSN time synchronization | IEEE 1588 PTP (sub-microsecond sync) |
| **802.1CB (FRER)** | TSN redundancy | Frame replication, elimination |
| **Traffic shaping** | TSN traffic policing | Rate limiting, burst control |
| **Deterministic latency** | TSN bounded latency | Worst-case latency guarantees |
| **Integration with SOME/IP** | SOME/IP over TSN | SOME/IP-SD with TSN QoS |

**Key Use Cases**:
- ADAS sensor fusion (deterministic Ethernet)
- Zonal architecture (time-synchronized)
- Safety-critical Ethernet (steering, braking)
- Audio/video streaming (A2B replacement)

**Safety**: ASIL-D (deterministic communication)

***

## **PHASE 18: Cloud Connectivity & V2X**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **MQTT/AMQP** | Vehicle-to-Cloud protocols | MQTT-SN, AMQP for telemetry |
| **HTTP/2, gRPC** | REST APIs, gRPC | Cloud API integration |
| **V2X (DSRC/C-V2X)** | V2X communication stack | IEEE 802.11p, C-V2X (PC5) |
| **V2V (Vehicle-to-Vehicle)** | V2V safety messages | BSM (Basic Safety Messages) |
| **V2I (Vehicle-to-Infrastructure)** | V2I communication | SPaT (Signal Phase & Timing) |
| **Edge computing** | MEC (Multi-access Edge Computing) | Low-latency cloud processing |
| **Digital twin** | Vehicle digital twin | Real-time vehicle model in cloud |
| **Telematics** | TCU (Telematics Control Unit) | GPS, cellular, eCall integration |

**Key Use Cases**:
- Connected vehicle services (remote start, lock/unlock)
- Predictive maintenance (cloud analytics)
- Traffic optimization (V2I)
- Platooning (V2V for trucks)

**Safety**: ASIL-B (V2X safety messages)

***

## **PHASE 19: Functional Safety & ISO 26262 Compliance**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **Safety mechanisms** | E2E, watchdog, CRC | All E2E profiles, window watchdog |
| **Fault injection** | Safety testing | Fault injection framework (testing) |
| **FMEDA** | FMEDA (Failure Modes Analysis) | Automated FMEDA generation |
| **Safety manual** | Safety manual (ISO 26262) | Generated safety manual per module |
| **ASIL decomposition** | ASIL D → D(B) + D(A) | Multi-core ASIL decomposition |
| **Safe state** | Safe state handling | Application-defined safe states |
| **Watchdog stack** | WdgM (Watchdog Manager) | Window watchdog, Q/A watchdog |
| **Program flow monitoring** | PFM (Program Flow Monitoring) | Control flow integrity (CFI) |

**Key Use Cases**:
- ISO 26262 certification (ASIL-B/D)
- Safety cases (argumentation)
- Third-party assessment (TÜV SÜD)
- OEM safety requirements (GM, VW, Toyota)

**Safety**: ISO 26262-4/5/6 (full compliance)

***

## **PHASE 20: Tooling & CI/CD**

### Features to Implement
| Feature |  Equivalent | OpenBSW Implementation |
|---------|-------------------|------------------------|
| **Configuration tool** | Open BSW Config | OpenSdV config tool (YAML/blob) |
| **Code generator** | Interface Layer generator, BSW generator | Python-based code gen (DBC → C++) |
| **Simulation** | CAN Tools,  | OpenBSW POSIX simulation (socketcan) |
| **Testing framework** | GTest, pytest | GoogleTest, pytest for integration |
| **Coverage analysis** | gcov, BullseyeCoverage | LCOV integration (branch coverage) |
| **Static analysis** | Polyspace, QAC | clang-tidy, cppcheck integration |
| **CI/CD pipeline** | Jenkins, GitLab CI | GitHub Actions, GitLab CI templates |
| **Documentation** | Sphinx, Doxygen | Auto-generated API docs (Sphinx) |

**Key Use Cases**:
- Automated testing (unit, integration)
- Continuous deployment (OTA validation)
- Code quality gates (safety compliance)
- Developer productivity (local simulation)

**Safety**: IEC 61508 (tool qualification)

***

# Implementation Priority Matrix

| Phase | Priority | Complexity | OEM Fault/Event Managerand | Tier1 Fault/Event Managerand | SDV Relevance |
|-------|----------|------------|------------|--------------|---------------|
| **Phase 6-7** (Persistent Memory, Fault/Event Manager) | 🔴 Critical | Medium | High | High | Medium |
| **Phase 8** (E2E) | 🔴 Critical | High | High | High | High |
| **Phase 9** (NM) | 🔴 Critical | Medium | High | High | Medium |
| **Phase 10** (Multi-core) | 🔴 Critical | Very High | High | High | High |
| **Phase 11** (SOME/IP) | 🟠 High | High | High | High | Very High |
| **Phase 12** (DDS) | 🟠 High | Very High | Medium | Medium | Very High |
| **Phase 13** (Security) | 🔴 Critical | High | Very High | Very High | Very High |
| **Phase 14** (OTA) | 🟠 High | High | Very High | High | Very High |
| **Phase 15** (App framework) | 🟠 High | Medium | High | Medium | Very High |
| **Phase 16** (Advanced diag) | 🟡 Medium | Medium | High | High | Low |
| **Phase 17** (TSN) | 🟡 Medium | Very High | Medium | Low | High |
| **Phase 18** (Cloud/V2X) | 🟡 Medium | High | Medium | Low | High |
| **Phase 19** (Safety) | 🔴 Critical | Very High | Very High | Very High | High |
| **Phase 20** (Tooling) | 🟠 High | Medium | High | High | High |

***

# Recommended Phase Order for Implementation

**For Gateway/Zonal ECU (Tier1 focus)**:
1. Phase 6-7 (Persistent Memory, Fault/Event Manager) - Persistence & diagnostics
2. Phase 8 (E2E) - Safety communication
3. Phase 9 (NM) - Network coordination
4. Phase 11 (SOME/IP) - Ethernet backbone
5. Phase 13 (Security) - ISO 21434 compliance
6. Phase 10 (Multi-core) - HPC support
7. Phase 19 (Safety) - ISO 26262 certification

**For HPC/Central Compute (SDV focus)**:
1. Phase 11 (SOME/IP) + Phase 12 (DDS) - Service architecture
2. Phase 15 (App framework) - Microservices
3. Phase 14 (OTA) - Remote updates
4. Phase 18 (Cloud/V2X) - Connected services
5. Phase 13 (Security) - Cybersecurity
6. Phase 10 (Multi-core) - Multi-core scheduling
7. Phase 19 (Safety) - ASIL decomposition

**For Safety-Critical ECU (Braking, Steering)**:
1. Phase 8 (E2E) - End-to-end protection
2. Phase 19 (Safety) - ISO 26262 mechanisms
3. Phase 10 (Multi-core) - ASIL decomposition
4. Phase 6-7 (Persistent Memory, Fault/Event Manager) - Fault storage
5. Phase 17 (TSN) - Deterministic Ethernet
6. Phase 13 (Security) - SecOC

***

This roadmap covers **95% of Automotive generaly used features** plus **SDV-specific requirements** (DDS, SOME/IP, cloud, OTA). Each phase builds on previous phases, maximizing code reuse from OpenBSW's existing modules.

**Next step**: need to define

See [GitHub Issues](https://github.com/eclipse-openbsw/openbsw/issues?q=is%3Aissue%20state%3Aopen%20label%3Aenhancement).

## Documentation

The [documentation](https://eclipse-openbsw.github.io/openbsw)
describes Eclipse OpenBSW in detail and provides simple setup guides to build and use it.

## Contributing

It is expected that this repository will be used as a starting point for many custom developments.
You may wish to contribute back some of your work to this repository.
For more details see [CONTRIBUTING](CONTRIBUTING.md).

## Legals

Distributed under the [Apache 2.0 License](LICENSE).

Also see [NOTICE](NOTICE.md).
