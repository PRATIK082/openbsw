# Production-Grade Implementation Plan — Eclipse OpenBSW

Companion to `PRODUCTION_GRADE_ROADMAP.md` (gap analysis) and `PHASE1_8_DETAILED_GAP_PLAN.md`
(Phases 1-8 verification). This document converts the gap analysis into an **actionable,
prioritised plan**: a feature-requirements list with acceptance criteria, dependencies, and a
sequenced execution order. Status legend: ✅ Done · 🟡 Partial · ❌ Not started · 🚧 In progress.

Priority: 🔴 Critical · 🟠 High · 🟡 Medium · 🟢 Nice-to-have.

## 0. Guiding principles

1. **Reuse before creating** — extend existing modules (`ecumode`, `canstack`, `commgateway`,
   `framegateway`, `persistence`, `faultmanager`, `integrity`) rather than duplicating.
2. **Smallest complete change** — each phase delivers one testable module with unit tests before
   moving on; no speculative generalisation.
3. **POSIX-first** — every module must build and test on the `tests-posix-debug` preset; HW
   binding is a follow-up behind an interface (e.g. `IFlashDriver`).
4. **No regression** — all existing tests stay green; new modules add regression tests.
5. **Verification gate** — a phase is `VERIFIED` only after focused + broader tests pass and the
   guard-audit is clean.

## 1. Feature requirements list (prioritised)

### P0 — Remaining gaps in Phases 1-8 (finish before new phases)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| R1 | Real HAL binding for `FlashWearStore` via `IFlashDriver` | 🔴 | 🟡 | `FlashWearStore::setFlashDriver()` wired in reference app; erase/write/read delegate to driver; POSIX mock test |
| R2 | UDS routing to Dem/Persistence/Security (0x14/0x19/0x27/0x85/0x34/0x36/0x37/0x86) | 🔴 | 🟡 | End-to-end UDS request→Dem/Persistence round trip on POSIX |
| R3 | CAN-FD DLC handling + bus-off recovery verification | 🟠 | 🟡 | Tests for FD DLC in all HW adapters; bus-off recovery test |
| R4 | ISO-TP (ISO 15765-2) STmin/BS/CF timing verification | 🟠 | 🟡 | Timing tests pass on POSIX |
| R5 | Wakeup source registry integration with EcuMode | 🟠 | 🟡 | Wakeup reason propagated to EcuMode; test |
| R6 | Consolidate legacy `CRCCalculator` with `persistence::Crc`/`integrity::Crc` | 🟢 | 🟡 | Single CRC implementation; legacy removed or deprecated |
| R7 | LinIf / EthIf real driver binding | 🟠 | 🟡 | Bind to UART/LIN transceiver and Eth PHY |

### Phase A — Network Management (recommended #1, 🔴 critical)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| A1 | CanNm state machine (BusSleep/PrepareBusSleep/RepeatMessage/NormalOperation/ReadySleep) | 🔴 | ❌ | State transitions per AUTOSAR NM; unit tests |
| A2 | NM PDU (node ID, control bit vector, user data) encode/decode | 🔴 | ❌ | Round-trip test |
| A3 | Passive/active NM, node detection, alive tracking | 🔴 | ❌ | Tests |
| A4 | Sleep/wake coordination callbacks to EcuMode | 🔴 | ❌ | Integration test |
| A5 | LinNm (LIN master/slave scheduler) | 🟠 | ❌ | Tests |
| A6 | EthNm / UdpNm (Ethernet/UDP NM) | 🟠 | ❌ | Tests |
| A7 | Partial networking (selective wake-up) | 🟠 | ❌ | Tests |
| A8 | Coordinated sleep across network | 🔴 | ❌ | End-to-end test |

### Phase B — Communication Manager (ComM) (🔴 critical, prerequisite for NM)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| B1 | ComM channel model + user request API | 🔴 | ❌ | Full/partial/silent/no comm modes |
| B2 | Channel mode management (COMM_FULL_COMMUNICATION, COMM_NO_COMMUNICATION, COMM_SILENT_COMMUNICATION) | 🔴 | ❌ | Tests |
| B3 | User request aggregation per channel | 🔴 | ❌ | Tests |
| B4 | Mode-dependent communication tie-in to EcuMode | 🟠 | ❌ | Integration test |

### Phase C — BSW Mode Manager (BswM) (🟠 high)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| C1 | Mode arbitration (rules + actions) | 🟠 | ❌ | Rule evaluation tests |
| C2 | Mode request API | 🟠 | ❌ | Tests |
| C3 | Action execution on mode change | 🟠 | ❌ | Tests |

### Phase D — Multi-core & Partitioning (recommended #2, 🔴 critical)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| D1 | Multi-core OS (spinlocks, task affinity) | 🔴 | ❌ | Tests on POSIX sim |
| D2 | MPU/MMU memory protection (partitions) | 🔴 | ❌ | Tests |
| D3 | Timing protection (execution time monitoring) | 🔴 | ❌ | Tests |
| D4 | ASIL decomposition support | 🟠 | ❌ | Doc + tests |

### Phase E — Security & Crypto (recommended #3, 🔴 critical)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| E1 | Crypto Service Manager (Csm) abstraction | 🔴 | ❌ | AES/SHA/HMAC/MAC/RSA/ECDSA interface |
| E2 | Key Manager (KeyM) + key/cert storage | 🔴 | ❌ | Tests |
| E3 | SecOC (MAC + freshness per PDU) | 🔴 | ❌ | Tests |
| E4 | Secure boot (signature verify) | 🔴 | ❌ | Tests |
| E5 | TLS/DTLS (mbedTLS) for HTTPS/DoIP security | 🟠 | ❌ | Tests |
| E6 | Certificate management (X.509) | 🟠 | ❌ | Tests |
| E7 | Intrusion Detection (IdsM) | 🟠 | ❌ | Tests |
| E8 | Secure diagnostics (0x27 seed/key, 0x29) | 🔴 | ❌ | Tests |

### Phase F — SOME/IP & Service Discovery (recommended #4, 🟠 high)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| F1 | Full SoAd (TCP/UDP, SOME/IP) | 🟠 | ❌ | Tests |
| F2 | Service Discovery (SD) | 🟠 | ❌ | Tests |
| F3 | ara::com (Adaptive) basic | 🟡 | ❌ | Tests |

### Phase G — DDS (recommended #5, 🟠 high)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| G1 | DDS pub/sub + QoS + discovery | 🟡 | ❌ | Tests |

### Phase H — OTA/FOTA (recommended #6, 🟠 high)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| H1 | Dual-bank (A/B) flash update | 🟠 | ❌ | Tests |
| H2 | Signed updates (RSA/ECDSA) | 🔴 | ❌ | Tests |
| H3 | Rollback on failure | 🟠 | ❌ | Tests |
| H4 | Progress reporting (UDS 0x36) | 🟠 | ❌ | Tests |
| H5 | Post-update validation (self-test) | 🟠 | ❌ | Tests |
| H6 | Delta updates (bsdiff) | 🟡 | ❌ | Tests |
| H7 | Campaign management | 🟡 | ❌ | Tests |

### Phase I — Advanced Diagnostics (recommended #7, 🔴 critical)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| I1 | Full UDS service set (0x11 reset types, 0x34/0x36/0x37, 0x86) | 🔴 | ❌ | Tests |
| I2 | OBD-II (SAE J1979) modes 01-0A | 🟠 | ❌ | Tests |
| I3 | WWH-OBD (ISO 27145) | 🟠 | ❌ | Tests |
| I4 | Readiness monitors (I/M) | 🟠 | ❌ | Tests (partially added) |
| I5 | Permanent DTCs | 🟠 | 🟡 | Already in faultmanager |
| I6 | Flash programming via UDS | 🔴 | ❌ | Tests |

### Phase J — Functional Safety (recommended #8, 🔴 critical)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| J1 | Watchdog Manager (WdgM) — window + Q&A + alive supervision | 🔴 | ❌ | Tests |
| J2 | Program flow monitoring (PFM/CFI) | 🔴 | ❌ | Tests |
| J3 | Safe state handling (tie-in to EcuMode) | 🔴 | 🟡 | Tests |
| J4 | Fault injection framework | 🟠 | ❌ | Tests |
| J5 | FMEDA tooling / safety manual | 🟡 | ❌ | Doc |
| J6 | Freedom from interference (spatial/temporal) | 🔴 | ❌ | Tests |

### Phase K — Other (MCAL / manufacturing / bootloader / power / observability / tooling)

| ID | Feature | Priority | Status | Req / acceptance |
|---|---|---|---|---|
| K1 | Watchdog (Wdg) driver (HW) | 🔴 | ❌ | HW binding |
| K2 | ADC/PWM/DIO/ICU/SPI/I2C/UART drivers | 🟠 | ❌ | HW binding |
| K3 | Temperature/battery monitor | 🟡 | ❌ | Tests |
| K4 | PMIC control | 🟡 | ❌ | Tests |
| K5 | GPT/MCU/PLL/clock mgmt | 🟠 | ❌ | Tests |
| K6 | NVM (MemIf/MemAcc) abstraction | 🟠 | ❌ | Tests |
| K7 | End-of-line programming / variant coding | 🟠 | ❌ | Tests |
| K8 | Traceability (serial/part numbers) | 🟡 | ❌ | Tests |
| K9 | Secure bootloader | 🔴 | ❌ | Tests |
| K10 | Version management / rollback (bootloader) | 🟠 | ❌ | Tests |
| K11 | Sleep/stop/standby + wakeup sources | 🟠 | 🟡 | Tests |
| K12 | PMIC control (power) | 🟡 | ❌ | Tests |
| K13 | DLT protocol | 🟡 | ❌ | Tests |
| K14 | Tracing (ITM/SWO, RTT) | 🟡 | ❌ | Tests |
| K15 | Performance counters / profiling | 🟡 | ❌ | Tests |
| K16 | Safety tooling (Polyspace/QAC), MISRA checking | 🟡 | ❌ | CI |
| K17 | AUTOSAR Adaptive (ara::com/exec/diag/ucm/iam/per/tsync) | 🟡 | ❌ | Tests |
| K18 | TSN (802.1Qbv/Qbu/Qcc/1AS/1CB) | 🟡 | ❌ | Tests |
| K19 | MQTT/AMQP (cloud), V2X | 🟢 | ❌ | Tests |
| K20 | FlexRay driver | 🟢 | ❌ | Tests |

## 2. Execution order (phases)

1. **Finish P0** (R1-R7) — close remaining 1-8 gaps.
2. **Phase A — NM** (A1-A8) — 🔴 critical, #1 recommended.
3. **Phase B — ComM** (B1-B4) — 🔴 critical, prerequisite for NM.
4. **Phase C — BswM** (C1-C3).
5. **Phase D — Multi-core & partitioning**.
6. **Phase E — Security & crypto**.
7. **Phase F — SOME/IP & SD**.
8. **Phase G — DDS**.
9. **Phase H — OTA/FOTA**.
10. **Phase I — Advanced diagnostics**.
11. **Phase J — Functional safety** (WdgM, PFM).
12. **Phase K — MCAL/manufacturing/bootloader/power/observability/tooling/Adaptive**.

> Note: Phase B (ComM) is a prerequisite for Phase A (NM); implement ComM channel model first, then
> CanNm. Phase J (WdgM) is independent and can run in parallel with Phase A.

## 3. Started work (this session)

- 🚧 **Phase A, slice 1 — CanNm (CAN Network Management) module** (`libs/bsw/nm`):
  - NM PDU encode/decode (A2)
  - NM state machine BusSleep/PrepareBusSleep/RepeatMessage/NormalOperation/ReadySleep (A1)
  - Passive/active NM + node tracking (A3)
  - Tick-driven `mainFunction()` (caller-provided tick, like IntegrityGuard)
  - Sleep/wake coordination callbacks (A4, hook)
  - Unit tests on POSIX

## 4. Verification

Each phase: focused unit tests + broader build (`tests-posix-debug`) + guard-audit clean before push.
Mark phase `VERIFIED` only with evidence.
