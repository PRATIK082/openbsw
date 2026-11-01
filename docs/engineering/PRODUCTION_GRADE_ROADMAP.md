# Production-Grade Feature Roadmap — Eclipse OpenBSW

Companion to the high-level phase roadmap in `README.md`. This document is the **gap analysis**
between the current openbsw stack and a production-grade automotive BSW (AUTOSAR Classic 4.x
concepts + Adaptive + SDV), prioritised for a Tier-1 / OEM delivery. Status legend:

- ✅ Implemented and on `main`
- 🟡 Partial / in progress
- ❌ Missing (not started)

Priority: 🔴 Critical (safety/compliance), 🟠 High (production blocker), 🟡 Medium, 🟢 Nice-to-have.

---

## 1. MCAL / ECU Abstraction (Hardware Drivers)

| Feature | Status | Priority | Notes |
|---|---|---|---|
| CAN driver (Can) + transceiver | ✅ | — | bxCAN, FDCAN, SocketCAN, MCP2515 |
| LIN driver (Lin) | 🟡 | 🟠 | commgateway has LinChannel; needs LinIf driver |
| Ethernet (Eth) driver | 🟡 | 🟠 | lwipSocket; needs EthIf + PHY mgmt |
| FlexRay (Fr) driver | ❌ | 🟢 | optional, high-end chassis |
| ADC driver | 🟡 | 🟠 | sensors/actuators; needs full ADC driver |
| PWM / OCU driver | 🟡 | 🟠 | actuator control |
| DIO / PORT driver | 🟡 | 🟠 | GPIO abstraction |
| ICU (input capture) | ❌ | 🟠 | RPM, speed, crank sensors |
| SPI / I2C / UART drivers | ❌ | 🟠 | sensor/actuator buses |
| Watchdog (Wdg) driver | ❌ | 🔴 | window/Q&A watchdog HW |
| GPT / MCU / PLL / clock | 🟡 | 🟠 | clock mgmt, sleep modes |
| Flash / EEPROM driver (Fls/Eep) | 🟡 | 🔴 | persistence uses RAM emulation today |
| NVM (MemIf/MemAcc) abstraction | 🟡 | 🟠 | unify Flash/EEPROM access |
| Temperature / battery monitor | ❌ | 🟡 | thermal/power safety |
| PMIC control | ❌ | 🟡 | power sequencing |

## 2. Services Layer (BSW Services)

| Feature | Status | Priority | Notes |
|---|---|---|---|
| ECU State Manager (EcuM) | 🟡 | 🔴 | ecumode has NORMAL/STANDBY/SLEEP/DIAG; needs startup/shutdown seq, wakeup sources, run requests |
| BSW Mode Manager (BswM) | ❌ | 🟠 | mode arbitration, rules, actions |
| Communication Manager (ComM) | ❌ | 🔴 | channel mgmt, user requests, full/partial/silent/no comm |
| PDU Router (PduR) | 🟡 | 🔴 | framegateway routes; needs full Com/CanIf/LinIf/SoAd routing |
| CAN Interface (CanIf) | 🟡 | 🟠 | canstack; needs full CanIf API |
| LIN Interface (LinIf) | ❌ | 🟠 | |
| Ethernet Interface (EthIf) | ❌ | 🟠 | |
| CAN Transport Protocol (CanTp) | 🟡 | 🟠 | docan; ISO-TP TX/RX |
| Socket Adapter (SoAd) | ❌ | 🟠 | TCP/UDP, SOME/IP |
| I-PDU Multiplexer (IpduM) | ❌ | 🟡 | |
| NVRAM Manager (NvM) | ✅ | — | persistence module |
| Diagnostic Event Manager (Dem) | ✅ | — | faultmanager module |
| Diagnostic Communication Mgr (Dcm) | 🟡 | 🔴 | UDS 0x10-0x3E partial; needs full set + security access |
| Function Inhibition Manager (FiM) | ❌ | 🟡 | inhibit on DTC |
| Crypto Service Manager (Csm) | ❌ | 🔴 | crypto abstraction |
| Key Manager (KeyM) | ❌ | 🔴 | key/cert storage |
| SecOC (Secure Onboard Comm) | ❌ | 🔴 | MAC + freshness per PDU |
| E2E (Transformer) | ✅ | — | integrity module (profiles 1/2/4/5/11) |
| Intrusion Detection (IdsM) | ❌ | 🟠 | CAN/Eth anomaly detection |
| Time Sync (StbM) | ❌ | 🟠 | global time, gPTP |
| Generic NM (Nm) | ❌ | 🔴 | CanNm, LinNm, EthNm, UdpNm |
| Diagnostic Log & Trace (Dlt) | 🟡 | 🟡 | logger; needs DLT protocol |
| Memory stack (MemIf/MemAcc) | ❌ | 🟠 | |

## 3. OS / RTE

| Feature | Status | Priority | Notes |
|---|---|---|---|
| RTOS (FreeRTOS/ThreadX) | ✅ | — | async module |
| AUTOSAR OS (tasks, ISRs, alarms, events, resources) | ❌ | 🔴 | |
| Schedule tables | ❌ | 🟠 | |
| Multi-core OS (spinlocks, task affinity) | ❌ | 🔴 | Phase 10 |
| Memory protection (MPU/MMU, partitions) | ❌ | 🔴 | spatial isolation |
| Timing protection (execution time monitoring) | ❌ | 🔴 | temporal isolation |
| RTE (sender/receiver, client/server, mode switch) | ❌ | 🟠 | middleware is IPC framework, not AUTOSAR RTE |
| Inter-runnable variables / exclusive areas | ❌ | 🟡 | |
| Calibration parameters (RTE) | ❌ | 🟡 | |

## 4. Communication Stacks

| Feature | Status | Priority | Notes |
|---|---|---|---|
| CAN stack (signal-based, multi-channel) | ✅ | — | canstack |
| LIN (master/slave, scheduler) | 🟡 | 🟠 | commgateway |
| Ethernet (TCP/UDP) | ✅ | — | lwipSocket |
| Frame gateway (PDU routing, ISO-TP, DoIP, XCP) | ✅ | — | framegateway |
| DoIP (ISO 13400) | ✅ | — | doip |
| XCP (calibration) | 🟡 | 🟠 | XcpServer present |
| SOME/IP (service discovery, RPC, events) | 🟡 | 🟠 | cpp2someip; needs full SoAd + SD |
| DDS (pub/sub, QoS, discovery) | ❌ | 🟡 | SDV backbone |
| TSN (802.1Qbv/Qbu/Qcc/1AS/1CB) | ❌ | 🟡 | deterministic Ethernet |
| NM (CAN/LIN/Eth) | ❌ | 🔴 | sleep/wake coordination |
| Partial networking | ❌ | 🟠 | selective wake-up |
| FlexRay | ❌ | 🟢 | |
| MQTT / AMQP (cloud) | ❌ | 🟡 | |
| V2X (DSRC/C-V2X) | ❌ | 🟢 | |

## 5. Diagnostics & OBD

| Feature | Status | Priority | Notes |
|---|---|---|---|
| UDS core (0x10, 0x11, 0x14, 0x19, 0x22, 0x27, 0x2E, 0x31, 0x34, 0x36, 0x37, 0x3E, 0x85) | 🟡 | 🔴 | needs full service set |
| OBD-II (SAE J1979) modes 01-0A | ❌ | 🟠 | |
| WWH-OBD (ISO 27145) | ❌ | 🟠 | |
| Readiness monitors (I/M) | ❌ | 🟠 | |
| Permanent DTCs | 🟡 | 🟠 | faultmanager |
| Flash programming via UDS (0x34/0x36/0x37) | ❌ | 🔴 | EOL/reprogramming |
| Security access (0x27 seed/key) | ❌ | 🔴 | |
| Tester present (0x3E) | 🟡 | 🟠 | |
| Response on event (0x86) | ❌ | 🟡 | |
| Diagnostic over IP (DoIP) | ✅ | — | doip |

## 6. Safety (ISO 26262)

| Feature | Status | Priority | Notes |
|---|---|---|---|
| E2E protection (CRC+counter+data-ID) | ✅ | — | integrity |
| Window / Q&A watchdog (WdgM) | ❌ | 🔴 | |
| Program flow monitoring (PFM/CFI) | ❌ | 🔴 | control flow integrity |
| Safe state handling | 🟡 | 🔴 | ecumode |
| ASIL decomposition (multi-core) | ❌ | 🟠 | |
| Fault injection framework | ❌ | 🟠 | testing |
| FMEDA tooling / safety manual | ❌ | 🟡 | |
| Freedom from interference (spatial/temporal) | ❌ | 🔴 | |

## 7. Security (ISO 21434 / UN R155)

| Feature | Status | Priority | Notes |
|---|---|---|---|
| SecOC (MAC per PDU, freshness) | ❌ | 🔴 | |
| TLS / DTLS (mbedTLS) | ❌ | 🟠 | HTTPS, DoIP security |
| Secure boot (signature verify) | ❌ | 🔴 | |
| Key management (HSM integration) | ❌ | 🔴 | |
| Certificate management (X.509) | ❌ | 🟠 | |
| Intrusion detection (IdsM) | ❌ | 🟠 | |
| Secure diagnostics (0x27, 0x29) | ❌ | 🔴 | |
| CAN FD encryption (optional) | ❌ | 🟢 | |

## 8. OTA / FOTA

| Feature | Status | Priority | Notes |
|---|---|---|---|
| Dual-bank (A/B) flash update | ❌ | 🟠 | |
| Delta updates (bsdiff) | ❌ | 🟡 | |
| Rollback on failure | ❌ | 🟠 | |
| Campaign management | ❌ | 🟡 | |
| Signed updates (RSA/ECDSA) | ❌ | 🔴 | |
| Progress reporting (UDS 0x36) | ❌ | 🟠 | |
| Post-update validation (self-test) | ❌ | 🟠 | |

## 9. Manufacturing / EOL

| Feature | Status | Priority | Notes |
|---|---|---|---|
| End-of-line programming (flash/cal) | ❌ | 🟠 | |
| Variant coding (VIN, config) | ❌ | 🟠 | persistence can store |
| Traceability (serial/part numbers) | ❌ | 🟡 | |
| Calibration download (XCP) | 🟡 | 🟠 | XcpServer |

## 10. Bootloader

| Feature | Status | Priority | Notes |
|---|---|---|---|
| Secure bootloader | ❌ | 🔴 | |
| Flash programming (UDS) | ❌ | 🔴 | |
| Version management / rollback | ❌ | 🟠 | |

## 11. Power Management

| Feature | Status | Priority | Notes |
|---|---|---|---|
| Sleep / stop / standby modes | 🟡 | 🟠 | ecumode |
| Wakeup sources (CAN, LIN, IO) | ❌ | 🟠 | |
| PMIC control | ❌ | 🟡 | |
| Coordinated sleep across network (NM) | ❌ | 🔴 | |

## 12. Observability / Telemetry

| Feature | Status | Priority | Notes |
|---|---|---|---|
| Logging (per-component) | ✅ | — | logger |
| DLT (Diagnostic Log and Trace) protocol | ❌ | 🟡 | |
| Tracing (ITM/SWO, RTT) | ❌ | 🟡 | |
| Performance counters / profiling | ❌ | 🟡 | |
| Console / commands | ✅ | — | asyncConsole |

## 13. Tooling & CI

| Feature | Status | Priority | Notes |
|---|---|---|---|
| Configuration (YAML → blob/code) | 🟡 | 🟠 | blob tool |
| Code generators (DBC → C++, ARXML) | 🟡 | 🟠 | dbc_to_openbsw.py |
| Simulation (POSIX socketcan) | ✅ | — | |
| Unit/integration testing (GTest, pytest) | ✅ | — | |
| Static analysis (clang-tidy, cppcheck) | ✅ | — | |
| Coverage (gcov/lcov) | ✅ | — | |
| HIL testing (pytest) | ✅ | — | |
| Documentation (Sphinx/Doxygen) | ✅ | — | |
| Safety tooling (Polyspace, QAC) | ❌ | 🟡 | |
| MISRA checking | ❌ | 🟡 | |

## 14. AUTOSAR Adaptive (HPC / SDV)

| Feature | Status | Priority | Notes |
|---|---|---|---|
| ara::com (SOME/IP) | ❌ | 🟡 | |
| ara::exec (execution mgmt) | ❌ | 🟡 | |
| ara::diag (diagnostics) | ❌ | 🟡 | |
| ara::ucm (update & config) | ❌ | 🟡 | |
| ara::iam (identity & access) | ❌ | 🟡 | |
| ara::per (persistency) | ❌ | 🟡 | |
| ara::tsync (time sync) | ❌ | 🟡 | |

---

## Recommended Next Phases (after 6-8)

1. **Phase 9 — Network Management (NM)**: CanNm/LinNm/EthNm, partial networking, sleep/wake coordination (🔴 critical).
2. **Phase 10 — Multi-core & Partitioning**: OS multi-core, MPU, timing protection (🔴 critical).
3. **Phase 13 — Security & Crypto**: SecOC, secure boot, key mgmt, TLS (🔴 critical).
4. **Phase 11 — SOME/IP & Service Discovery**: full SoAd + SD (🟠 high).
5. **Phase 12 — DDS**: SDV backbone (🟠 high).
6. **Phase 14 — OTA/FOTA**: dual-bank, signed updates, rollback (🟠 high).
7. **Phase 16 — Advanced Diagnostics**: full UDS service set, OBD-II, WWH-OBD (🔴 critical).
8. **Phase 19 — Functional Safety**: WdgM, PFM, FMEDA, safety manual (🔴 critical).

## Summary

openbsw already delivers a strong **communication + diagnostics + persistence + safety (E2E)** core.
The largest gaps for production-grade delivery are: **NM, multi-core safety, security (SecOC/crypto),
watchdog manager, full UDS/Dcm, OTA, and manufacturing/EOL tooling**. These should be prioritised
before any customer-facing release.
