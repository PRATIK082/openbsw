# Next Activity Plan — Improve Current Codebase & Fill Gaps

**Project:** Eclipse OpenBSW (`openbsw`)
**Branch:** `plan/next-activity-gap-closure`
**Baseline commit:** current `main` (post realignment, 20 commits ahead of upstream)
**Companion docs:** `MIDDLEWARE_STACK_STUDY_PLAN.md`, `CODEBASE_MAP.md`, `PRODUCTION_GRADE_ROADMAP.md`,
`PRODUCTION_GRADE_PLAN.md`, `NM_USE_CASE_ANALYSIS.md`
**Status legend:** ✅ Done · 🟡 Partial · ❌ Not started · 🚧 In progress
**Priority:** 🔴 Critical · 🟠 High · 🟡 Medium · 🟢 Nice-to-have

---

## 0. Current state (after latest main)

Already delivered (✅): CAN stack, Ethernet/DoIP, frame gateway, persistence, Dem, E2E, RTOS
abstraction, diagnostics core, CanNm/GatewayNm + LinNm + EthNm, CommChannelManager, ModeArbiter,
driver bindings (LIN/Eth), EcuMode + wakeup registry.

Remaining high-impact gaps (🟡/❌): FlashWearStore HAL binding wiring, UDS→Dem/Persistence/
Security end-to-end, multi-core/MPU/timing protection, security/crypto (Csm/KeyM/SecOC/secure
boot/TLS/IdsM), OTA/FOTA, WdgM/PFM, observability (DLT/tracing/profiling), traceability
(no `@req`/`@trace` tags in source), busload analysis, description-file discipline.

---

## 1. Guiding principles

1. POSIX-first: every new module must build and test under `tests-posix-debug`.
2. Smallest complete change: reuse existing modules (`commchannel`, `modearbiter`, `ecumode`,
   `persistence`, `faultmanager`, `integrity`) rather than duplicating.
3. Phase gate: a phase is `VERIFIED` only with focused tests + broader build + guard-audit clean +
   traceability update + evidence archived.
4. No agent/AI tooling files committed; keep `.agents/` local-only (`.gitignore`).
5. Author identity: all commits as `PRATIK082 <pratik.patil082@gmail.com>`.

---

## 2. Milestones

### M-N1 — P0 closure (2 weeks) 🔴

| ID | Task | Acceptance |
|---|---|---|
| N1.1 | Wire `FlashWearStore::setFlashDriver()` in reference app with a POSIX mock `IFlashDriver`; add unit test. | Mock flash driver round-trip test green; reference app builds. |
| N1.2 | UDS→Dem/Persistence/Security end-to-end round trip (0x14, 0x19, 0x27, 0x85). Transfer services 0x34/0x36/0x37 remain pending (SYS-UDS-005). | POSIX e2e test green. |
| N1.3 | Verify wakeup source registry fully wired: all sources propagate to NORMAL mode from SLEEP/STANDBY. | Tests for single/multiple sources. |
| N1.4 | Traceability bootstrap: add `SYS-xxx` IDs to Doxygen comments for delivered modules; generate matrix. | Matrix doc + at least one module fully tagged. |
| N1.5 | Guard-audit + clang-tidy clean on touched modules. | CI green. |

### M-N2 — Security & crypto foundation (4 weeks) 🔴

| ID | Task | Acceptance |
|---|---|---|
| N2.1 | Csm abstraction (AES/SHA/HMAC/MAC/RSA/ECDSA) over mbedTLS. | Unit tests for each primitive. |
| N2.2 | KeyM + key/cert storage (NvM-backed). | Key load/store/rotate tests. |
| N2.3 | SecOC (freshness, MAC verify) integration with framegateway. | SecOC round-trip test. |
| N2.4 | Secure diagnostics (0x27/0x29) wired to Csm/KeyM. | UDS security access test. |
| N2.5 | TLS/DTLS socket (mbedTLS) in lwipSocket. | Handshake test on POSIX. |

### M-N3 — OTA/FOTA foundation (4 weeks) 🟠

| ID | Task | Acceptance |
|---|---|---|
| N3.1 | Dual-bank A/B flash layout + bank selection. | Bank switch test. |
| N3.2 | Signed update verification (Csm). | Signature verify test. |
| N3.3 | Rollback + post-update validation. | Rollback test. |
| N3.4 | UDS 0x34/0x36/0x37 progress reporting. | Progress callback test. |

### M-N4 — Functional safety (4 weeks) 🔴

| ID | Task | Acceptance |
|---|---|---|
| N4.1 | WdgM (window + Q&A + alive supervision). | Supervision tests. |
| N4.2 | PFM/CFI (program flow monitoring). | CFI trap test. |
| N4.3 | Safe state handling via ModeArbiter actions. | Safe-state transition test. |
| N4.4 | Fault injection framework (test infra). | At least one injected fault test. |

### M-N5 — Cross-cutting (ongoing) 🟠

| ID | Task | Acceptance |
|---|---|---|
| N5.1 | Complete `CODEBASE_MAP.md` ASIL zones + signal flow (finish M0). | Map verified against source. |
| N5.2 | Per-module validation matrix (build/static/comm/diag/fault/regression). | Matrix doc present. |
| N5.3 | Busload & worst-case latency analysis (CAN/LIN/Eth). | Analysis doc with numbers. |
| N5.4 | DBC/ARXML/LDF/FIBEX description-file discipline + hashes. | Hash check in CI. |
| N5.5 | Observability: DLT + tracing + perf counters. | DLT log output test. |
| N5.6 | HIL test expansion (pytest) for new modules. | At least 3 HIL tests. |

---

## 3. Execution order

1. M-N1 (P0 closure) → 2. M-N2 (security) → 3. M-N3 (OTA) → 4. M-N4 (safety) → 5. M-N5 (cross-cutting, parallel).

> M-N4 (WdgM) can run in parallel with M-N1/M-N2. M-N5 runs continuously.

---

## 4. To-do backlog (prioritised, actionable)

### Now (this branch)

- [ ] N1.1 Wire FlashWearStore to POSIX mock IFlashDriver + test.
- [ ] N1.2 UDS→Dem/Persistence/Security e2e test.
- [ ] N1.3 Wakeup source registry verification tests.
- [ ] N1.4 Traceability bootstrap (SYS-xxx tags + matrix).
- [ ] N1.5 Guard-audit + clang-tidy on touched modules.

### Next

- [ ] N2.1–N2.5 Security & crypto foundation.
- [ ] N3.1–N3.4 OTA/FOTA foundation.
- [ ] N4.1–N4.4 Functional safety.

### Later

- [ ] N5.1–N5.6 Cross-cutting.

---

## 5. Verification gates

Each task is `VERIFIED` only after:

1. Focused unit tests pass.
2. Broader build `tests-posix-debug` passes.
3. Guard-audit clean.
4. Traceability updated (SYS→src→test).
5. Evidence archived (test logs + commit hash).

---

## 6. Open questions

1. Hardware target for IFlashDriver binding (which MCU first)?
2. Scope of Adaptive (ara::*) — in or out?
3. Security library choice: mbedTLS vs wolfSSL?
4. OTA transport: UDS 0x34/0x36/0x37 only, or also DoIP?

---

*Generated as the next-activity plan for the automotive embedded middleware stack study.*
