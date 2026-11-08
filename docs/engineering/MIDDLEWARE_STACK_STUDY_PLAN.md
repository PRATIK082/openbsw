# Automotive Embedded Middleware Stack — Study Plan & Mission Backlog

**Project:** Eclipse OpenBSW (`openbsw`)  
**Branch:** `study/update-plan-latest-main`  
**Baseline commit:** `4b74e28c` — merge of PR #8 (study plan) + latest main updates (2026-11-08)  
**Companion docs:** `PRODUCTION_GRADE_ROADMAP.md`, `PRODUCTION_GRADE_PLAN.md`, `NM_USE_CASE_ANALYSIS.md`  
**Status legend:** ✅ Done · 🟡 Partial · ❌ Not started · 🚧 In progress  
**Priority:** 🔴 Critical · 🟠 High · 🟡 Medium · 🟢 Nice-to-have

---

## 0. Purpose

This document converts the *production-grade gap analysis* into a **study-driven execution plan** for the
automotive embedded middleware stack currently delivered by OpenBSW. It enumerates everything that can
be added to the plan, defines missions with acceptance criteria, and provides a to-do backlog that can be
worked through on this branch. It is intended to be the single tracking artifact for the
"study current middleware stack + plan additions" activity.

---

## 1. Baseline — current stack (latest `main`)

OpenBSW already delivers a strong **communication + diagnostics + persistence + safety (E2E)** core on
POSIX and reference hardware (S32K148, STM32, etc.).

### 1.1 BSW module inventory (47 modules, `libs/bsw/`)

`async`, `asyncConsole`, `asyncFreeRtos`, `asyncImpl`, `asyncThreadX`, `blob`, `bsp`, `canstack`,
`commchannel`, `commgateway`, `common`, `cpp2can`, `cpp2ethernet`, `cpp2someip`, `docan`, `doip`,
`ecumode`, `ethdriver`, `ethdriverbinding`, `ethlink`, `faultmanager`, `framegateway`, `gwbridge`,
`integrity`, `io`, `lifecycle`, `lindriver`, `lindriverbinding`, `linlink`, `logger`,
`loggerIntegration`, `lwipSocket`, `middleware`, `modearbiter`, `nm`, `persistence`, `routing`,
`runtime`, `shed`, `stdioConsoleInput`, `storage`, `time`, `timer`, `transport`,
`transportRouterSimple`, `uds`, `util`.

### 1.2 Test baseline

- 365 unit-test files under `libs/bsw/**/test`.
- Build presets: `tests-posix-debug` (POSIX-first verification gate).
- Reference applications: `referenceApp`, `unitTest` executables.

### 1.3 Already delivered (✅ on `main`)

- CAN stack (signal-based, multi-channel), CAN-FD via `cpp2can` (64-byte frames).
- Ethernet TCP/UDP (`lwipSocket`), DoIP (`doip`), XCP (partial).
- Frame gateway (PDU routing, ISO-TP, DoIP, XCP), docan ISO-TP (210 tests).
- Persistence/NvM (`persistence`), Dem (`faultmanager`), E2E (`integrity`, profiles 1/2/4/5/11).
- RTOS abstraction (`async` FreeRTOS/ThreadX), POSIX socketcan simulation.
- Diagnostics core UDS (0x10–0x3E partial), DoIP.
- Network Management: `CanNm` + `GatewayNm`, plus `LinLinkManager` (LinNm) and `EthLinkManager`
  (EthNm) with active/passive NM, PN, CSR coordinator, remote wakeup, bus-off handling.
- Communication Manager: `commchannel::CommChannelManager` (channels, users, FULL/SILENT/NO modes,
  request/release aggregation) — Phase B delivered.
- BSW Mode Manager: `modearbiter::ModeArbiter` (rules + actions, EcuMode integration) — Phase C
  delivered.
- Driver bindings: `lindriverbinding`, `ethdriverbinding` (IFlashDriver-style HAL binding) — R7
  substantially addressed.
- EcuMode (NORMAL/STANDBY/SLEEP/DIAG), wakeup source registration.
- Build/CI: CMake, Bazel, clang-tidy, cppcheck, gcov/lcov, pytest HIL.

### 1.4 Partial / in progress (🟡)

- Flash binding for `FlashWearStore` (IFlashDriver) — R1.
- UDS routing to Dem/Persistence/Security (0x14/0x19/0x27/0x85/0x34/0x36/0x37/0x86) — R2.
- LinIf/EthIf real driver binding — R7.
- EcuM startup/shutdown seq, run requests — partial.
- PduR full routing, CanIf full API — partial.
- SOME/IP (cpp2someip) — needs SoAd + SD.
- XCP calibration — partial.
- Wakeup sources — partial.
- Safe state handling (EcuMode) — partial.
- Permanent DTCs — partial.

### 1.5 Missing (❌) — largest gaps

Network Management beyond CAN (LinNm/EthNm/UdpNm), ComM, BswM, multi-core OS + MPU + timing
protection, security/crypto (Csm, KeyM, SecOC, secure boot, TLS, IdsM), full UDS/DCM, OTA/FOTA,
watchdog manager (WdgM), functional safety (PFM), manufacturing/EOL, bootloader, power management,
observability (DLT, tracing, profiling), AUTOSAR Adaptive, TSN, DDS, MQTT/AMQP, V2X, FlexRay.

---

## 2. Best skill to work from (AxiomEmbedded)

**Primary skill:** `codebase-analysis`
(`E:\My_Project\My_Github\AxiomEmbedded\.opencode\skills\codebase-analysis\SKILL.md`)

Rationale: it is the only skill whose explicit outcome is a durable **CODEBASE_MAP.md** that maps
SWCs/BSW/RTE/signal flow/ASIL zones per feature. That is exactly what a "study current middleware
stack" activity needs before planning additions. It is platform-agnostic (Classic/Adaptive/mixed) and
produces decision-ready artifacts with verification checklists.

**Complementary skills (load alongside):**

| Skill | Use for |
|---|---|
| `axiom-autosar-classic-bsw-integration` | Inventory ARXML/BMD/RTE/OS/COM/PduR/CanIf/CanTp/Dcm/Dem/NvM/EcuM config; integration report + validation matrix. |
| `automotive-middleware` | SDV middleware selection (ara::com, SOME/IP-SD, DDS, Zenoh, MQTT, TSN, zero-copy IPC, VSS/VSC). |
| `automotive-networks` | CAN-FD/XL, LIN, Eth+TSN topology, gateway routing, busload/timing, DBC/ARXML. |
| `autosar` | Classic/Adaptive selection, layered BSW/RTE rules, SOME/IP. |
| `axiom-autosar-classic-architecture` | Classic BSW layering & architecture review. |
| `axiom-functional-safety` | ISO 26262 work products, ASIL zoning, safety case inputs. |
| `axiom-vehicle-cybersecurity` | ISO/SAE 21434, SecOC, key/cert, intrusion detection. |
| `project-audit` | Criteria-first audits, sampling, release gates. |
| `requirements` / `requirements-traceability` | SYS→SWC→src→test traceability matrix. |

**Recommended working set for this study:** `codebase-analysis` (primary) + `automotive-networks` +
`axiom-autosar-classic-bsw-integration` + `axiom-functional-safety`.

---

## 3. Study objectives & deliverables

1. **CODEBASE_MAP.md** for OpenBSW: per-feature/component map of BSW + middleware + signal flow +
   ASIL zones (output of `codebase-analysis`).
2. **Middleware addition plan** (this doc, maintained) — gap list, missions, todos.
3. **Validation matrix** per module (build, static review, communication, diagnostics, fault
   handling, regression) — from `axiom-autosar-classic-bsw-integration`.
4. **Traceability matrix** SYS-xxx → SWC/BSW → src → test → evidence.
5. **Evidence pack** for each verified phase (test logs, guard-audit clean).

---

## 4. What can be added to the plan (gap-driven additions)

This section consolidates everything not yet done, grouped so it can be appended to
`PRODUCTION_GRADE_PLAN.md` without duplication. Source of truth for status remains the roadmap.

### 4.1 P0 — finish remaining Phases 1-8 gaps (🔴/🟠)

| ID | Addition | Pri | Status | Acceptance |
|---|---|---|---|---|
| R1 | Real HAL binding for `FlashWearStore` via `IFlashDriver` | 🔴 | 🟡 | Wired in reference app; POSIX mock test green. |
| R2 | UDS routing to Dem/Persistence/Security (0x14/0x19/0x27/0x85/0x34/0x36/0x37/0x86) | 🔴 | 🟡 | End-to-end UDS→Dem/Persistence round trip on POSIX. |
| R7 | LinIf / EthIf real driver binding | 🟠 | 🟡→✅ | `lindriverbinding` + `ethdriverbinding` added; HW binding still pending. |
| R8 | ComM channel model | 🔴 | ✅ | `commchannel::CommChannelManager` delivered with tests. |
| R9 | Wakeup source registry fully wired into EcuMode transitions | 🟠 | 🟡 | All wakeup sources propagate to NORMAL mode. |

### 4.2 Phase A — Network Management (🔴 critical, #1)

A1 CanNm state machine · A2 NM PDU encode/decode · A3 passive/active + node tracking ·
A4 sleep/wake callbacks to EcuMode · A5 LinNm · A6 EthNm/UdpNm · A7 partial networking ·
A8 coordinated sleep.

Status update (latest main): `linlink::LinLinkManager` (LinNm) and `ethlink::EthLinkManager`
(EthNm) delivered with active/passive NM, multi-user aggregation, PN, CSR coordinator, remote
wakeup, bus-off handling, gateway coordination. CanNm/GatewayNm (in `nm`) remain the CAN variant.
Remaining gaps: UdpNm, NM PDU forwarding between buses, PN filter lists (multiple groups), NM
coordinator voting, ComM↔NM integration (commchannel available), bus-off retry strategy, timing
config validation.

**Additions:**
- A9 LinNm (LIN master/slave scheduler) — depends on LinIf driver (R7).
- A10 EthNm/UdpNm (Ethernet/UDP NM).
- A11 NM PDU forwarding between buses (gateway).
- A12 PN filter lists (multiple groups per node).
- A13 NM coordinator voting algorithm (beyond CSR bit).
- A14 ComM channel-mode integration (depends on Phase B).
- A15 Bus-off recovery retry/re-init strategy.
- A16 Timing config validation (T_NM, T_REPEAT_MESSAGE, T_WAIT_BUS_SLEEP, T_TX).

### 4.3 Phase B — Communication Manager (ComM) (🔴 critical, prerequisite for NM)

B1 channel model + user request API · B2 channel modes (FULL/NO/SILENT) · B3 user request
aggregation · B4 mode-dependent tie-in to EcuMode.

Status update (latest main): `commchannel::CommChannelManager` delivered with channels, users,
FULL/SILENT/NO modes, request/release aggregation, tick-driven mainFunction. Phase B
substantially complete (integration with EcuMode/modearbiter ongoing).

### 4.4 Phase C — BswM (🟠 high)

C1 mode arbitration (rules+actions) · C2 mode request API · C3 action execution.

Status update (latest main): `modearbiter::ModeArbiter` delivered (rules + actions, EcuMode
integration). Phase C substantially complete.

### 4.5 Phase D — Multi-core & Partitioning (🔴 critical)

D1 multi-core OS (spinlocks, task affinity) · D2 MPU/MMU partitions · D3 timing protection ·
D4 ASIL decomposition support.

### 4.6 Phase E — Security & Crypto (🔴 critical)

E1 Csm abstraction (AES/SHA/HMAC/MAC/RSA/ECDSA) · E2 KeyM + key/cert storage · E3 SecOC ·
E4 secure boot · E5 TLS/DTLS (mbedTLS) · E6 X.509 cert mgmt · E7 IdsM · E8 secure diagnostics
(0x27/0x29).

### 4.7 Phase F — SOME/IP & Service Discovery (🟠 high)

F1 full SoAd (TCP/UDP, SOME/IP) · F2 Service Discovery (SD) · F3 ara::com (Adaptive) basic.

### 4.8 Phase G — DDS (🟠 high)

G1 DDS pub/sub + QoS + discovery.

### 4.9 Phase H — OTA/FOTA (🟠 high)

H1 dual-bank A/B · H2 signed updates · H3 rollback · H4 progress (UDS 0x36) ·
H5 post-update validation · H6 delta updates (bsdiff) · H7 campaign management.

### 4.10 Phase I — Advanced Diagnostics (🔴 critical)

I1 full UDS service set (0x11 reset types, 0x34/0x36/0x37, 0x86) · I2 OBD-II modes 01-0A ·
I3 WWH-OBD (ISO 27145) · I4 readiness monitors · I5 permanent DTCs (partial) · I6 flash
programming via UDS.

### 4.11 Phase J — Functional Safety (🔴 critical)

J1 WdgM (window + Q&A + alive supervision) · J2 PFM/CFI · J3 safe state handling ·
J4 fault injection framework · J5 FMEDA tooling/safety manual · J6 freedom from interference.

### 4.12 Phase K — MCAL / manufacturing / bootloader / power / observability / tooling / Adaptive

K1 Wdg driver · K2 ADC/PWM/DIO/ICU/SPI/I2C/UART · K3 temp/battery monitor · K4 PMIC ·
K5 GPT/MCU/PLL/clock · K6 NVM (MemIf/MemAcc) · K7 EOL programming/variant coding ·
K8 traceability (serial/part numbers) · K9 secure bootloader · K10 version mgmt/rollback ·
K11 sleep/stop/standby + wakeup · K12 PMIC (power) · K13 DLT protocol · K14 tracing (ITM/SWO, RTT) ·
K15 perf counters/profiling · K16 safety tooling (Polyspace/QAC, MISRA) CI ·
K17 AUTOSAR Adaptive (ara::com/exec/diag/ucm/iam/per/tsync) · K18 TSN (802.1Qbv/Qbu/Qcc/1AS/1CB) ·
K19 MQTT/AMQP (cloud), V2X · K20 FlexRay driver.

### 4.13 Cross-cutting additions (new, not in original plan)

| ID | Addition | Pri | Rationale |
|---|---|---|---|
| X1 | CODEBASE_MAP.md (per-feature BSW/middleware/ASIL map) | 🔴 | Study deliverable; enables all later work. |
| X2 | Per-module validation matrix (build/static/comm/diag/fault/regression) | 🟠 | From BSW integration skill. |
| X3 | Busload & worst-case latency analysis per bus (CAN/LIN/Eth) | 🟠 | Production blocker for timing. |
| X4 | DBC/ARXML/LDF/FIBEX description-file discipline + hashes | 🟠 | Source-of-truth for comms. |
| X5 | Guard-audit clean as phase gate (already convention) | 🔴 | Keep enforced. |
| X6 | Safety tooling in CI (MISRA, Polyspace/QAC) | 🟡 | Compliance. |
| X7 | Observability: per-component logging + DLT + tracing + perf counters | 🟡 | Field diagnostics. |
| X8 | HIL test expansion (pytest) for new modules | 🟠 | HW validation. |
| X9 | Documentation: Doxygen/Sphinx updates per module | 🟡 | Maintainability. |

---

## 5. Missions (study + implementation)

Each mission is a self-contained, testable unit. Work them in the order of §6 (execution order).

### M0 — Baseline study (study, no code)

- **Objective:** Produce CODEBASE_MAP.md for OpenBSW using `codebase-analysis`.
- **Tasks:** Detect Classic platform; discover repo structure; identify features/SWCs/BSW modules;
  map signal flow; ASIL zones; traceability IDs; write CODEBASE_MAP.md.
- **Acceptance:** CODEBASE_MAP.md exists and is internally consistent; "Verified against: …; could not verify: …" line present.
- **Skill:** `codebase-analysis` (primary).

### M1 — Close P0 gaps (R1, R2, R7, R8, R9)

- **Objective:** Finish remaining Phase 1-8 gaps before new phases.
- **Tasks:** Wire FlashWearStore to IFlashDriver; UDS routing e2e; LinIf/EthIf binding; ComM stub; wakeup registry.
- **Acceptance:** Focused + broader tests green; guard-audit clean.
- **Skills:** `axiom-autosar-classic-bsw-integration`, `axiom-functional-safety`.

### M2 — ComM (Phase B)

- **Objective:** Implement ComM channel model + user requests + modes + EcuMode tie-in.
- **Tasks:** B1–B4 with unit tests.
- **Acceptance:** Tests green; integration with EcuMode.
- **Skill:** `automotive-networks` (comms), `autosar`.

### M3 — CanNm completion + LinNm/EthNm/UdpNm (Phase A)

- **Objective:** Complete NM family and coordinated sleep.
- **Tasks:** A5–A16.
- **Acceptance:** NM tests + gateway sleep coordination e2e.
- **Skill:** `automotive-networks`.

### M4 — BswM (Phase C)

- **Objective:** Mode arbitration/rules/actions.
- **Tasks:** C1–C3.
- **Acceptance:** Rule evaluation tests.
- **Skill:** `autosar`.

### M5 — Multi-core & partitioning (Phase D)

- **Objective:** Spinlocks, task affinity, MPU, timing protection.
- **Tasks:** D1–D4.
- **Acceptance:** POSIX sim tests.
- **Skill:** `axiom-rtos-architecture` (AxiomEmbedded), `mpu`.

### M6 — Security & crypto (Phase E)

- **Objective:** Csm, KeyM, SecOC, secure boot, TLS, certs, IdsM, secure diagnostics.
- **Tasks:** E1–E8.
- **Acceptance:** Tests green.
- **Skill:** `axiom-vehicle-cybersecurity`, `security`.

### M7 — SOME/IP & SD (Phase F)

- **Objective:** Full SoAd + SD + ara::com basic.
- **Tasks:** F1–F3.
- **Acceptance:** Tests green.
- **Skill:** `automotive-middleware`.

### M8 — DDS (Phase G)

- **Objective:** DDS pub/sub + QoS + discovery.
- **Tasks:** G1.
- **Acceptance:** Tests green.
- **Skill:** `automotive-middleware`.

### M9 — OTA/FOTA (Phase H)

- **Objective:** Dual-bank, signed updates, rollback, progress, validation, delta, campaigns.
- **Tasks:** H1–H7.
- **Acceptance:** Tests green.
- **Skill:** `automotive-ota` (AxiomEmbedded).

### M10 — Advanced diagnostics (Phase I)

- **Objective:** Full UDS set, OBD-II, WWH-OBD, readiness, permanent DTCs, flash programming.
- **Tasks:** I1–I6.
- **Acceptance:** Tests green.
- **Skill:** `axiom-diagnostics-uds`, `axiom-diagnostics-doip`.

### M11 — Functional safety (Phase J)

- **Objective:** WdgM, PFM/CFI, safe state, fault injection, FMEDA, FFI.
- **Tasks:** J1–J6.
- **Acceptance:** Tests green.
- **Skill:** `axiom-functional-safety`, `safety`.

### M12 — MCAL / manufacturing / bootloader / power / observability / tooling / Adaptive (Phase K)

- **Objective:** K1–K20.
- **Acceptance:** Per-item tests.
- **Skills:** `drivers`, `mcu`, `bootloader`, `automotive-spice`.

### M13 — Cross-cutting (X1–X9)

- **Objective:** CODEBASE_MAP, validation matrix, busload analysis, description-file discipline,
  guard-audit, safety tooling CI, observability, HIL expansion, docs.
- **Acceptance:** Artifacts present; CI green.
- **Skills:** `project-audit`, `test-automation`, `documentation`.

---

## 6. Execution order

1. M0 (study) → M1 (P0) → M2 (ComM) → M3 (NM) → M4 (BswM) →
2. M5 (multi-core) → M6 (security) → M7 (SOME/IP) → M8 (DDS) →
3. M9 (OTA) → M10 (diagnostics) → M11 (safety) → M12 (K) → M13 (cross-cutting).

> Phase B (ComM) is prerequisite for Phase A (NM); Phase J (WdgM) can run in parallel with Phase A.

---

## 7. To-do backlog (prioritised, actionable)

### Now (this branch, study-first)

- [ ] T0.1 Run `codebase-analysis` over openbsw; produce `docs/engineering/CODEBASE_MAP.md`.
- [ ] T0.2 Inventory BSW modules + map signal flow + ASIL zones.
- [ ] T0.3 Record traceability IDs (SYS-xxx) found in source (Doxygen @req/@trace).
- [ ] T0.4 Verify map internal consistency; write "Verified against / could not verify" line.
- [ ] T1.1 Wire `FlashWearStore::setFlashDriver()` in reference app (R1).
- [ ] T1.2 UDS→Dem/Persistence round trip test (R2).
- [ ] T1.3 LinIf/EthIf binding scaffolding (R7).
- [ ] T1.4 ComM stub module skeleton + tests (R8).
- [ ] T1.5 Wakeup registry integration test (R9).

### Next (after P0)

- [ ] T2.1 ComM channel model + user request API (B1).
- [ ] T2.2 ComM modes (B2).
- [ ] T2.3 ComM aggregation (B3).
- [ ] T2.4 ComM↔EcuMode (B4).
- [ ] T3.1 LinNm (A5/A9).
- [ ] T3.2 EthNm/UdpNm (A6/A10).
- [ ] T3.3 NM PDU forwarding (A11).
- [ ] T3.4 PN filter lists (A12).
- [ ] T3.5 NM coordinator voting (A13).
- [ ] T3.6 ComM↔NM integration (A14).
- [ ] T3.7 Bus-off recovery retry (A15).
- [ ] T3.8 NM timing validation (A16).
- [ ] T4.1 BswM arbitration (C1).
- [ ] T4.2 BswM mode request (C2).
- [ ] T4.3 BswM actions (C3).

### Later

- [ ] T5.* Multi-core & partitioning (D1–D4).
- [ ] T6.* Security & crypto (E1–E8).
- [ ] T7.* SOME/IP & SD (F1–F3).
- [ ] T8.* DDS (G1).
- [ ] T9.* OTA/FOTA (H1–H7).
- [ ] T10.* Advanced diagnostics (I1–I6).
- [ ] T11.* Functional safety (J1–J6).
- [ ] T12.* MCAL/manufacturing/bootloader/power/observability/tooling/Adaptive (K1–K20).
- [ ] T13.* Cross-cutting (X1–X9).

---

## 8. Verification gates

Each mission/phase is `VERIFIED` only after:

1. Focused unit tests pass.
2. Broader build `tests-posix-debug` passes.
3. Guard-audit clean.
4. Traceability updated (SYS→src→test).
5. Evidence archived.

Mark phase `VERIFIED` only with evidence (test logs + commit hash).

---

## 9. Working on this branch

- Branch: `study/automotive-middleware-stack-plan` (created from `main` @ `51d2a8bc`).
- Commit plan + CODEBASE_MAP as they are produced.
- Do not push without explicit user confirmation.
- Keep changes consistent with repo conventions (reuse modules, smallest complete change, POSIX-first).

---

## 10. Open questions / decisions needed

1. Which skill to install into openbsw's `.agents/skills/` for reuse (recommend `codebase-analysis` + `automotive-networks`)?
2. Priority order confirmation: NM → ComM (prerequisite) vs ComM first?
3. Hardware target for HAL binding (IFlashDriver) — which MCU first?
4. Scope of Adaptive (ara::*) — in or out of this study?
5. Export format for plan (Markdown only, or also PDF/RST for docs site)?

---

*Generated as part of the "automotive embedded middleware stack study" activity.*
