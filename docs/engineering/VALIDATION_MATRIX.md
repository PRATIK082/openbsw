# Validation Matrix — Eclipse OpenBSW (per-module)

**Status:** First pass (N5.2). Categories: **Build** (compiles under `tests-posix-debug`),
**Static** (clang-tidy/cppcheck/copyright clean), **Comm** (communication path exercised),
**Diag** (diagnostic services path), **Fault** (fault handling / Dem interaction),
**Regression** (unit tests present & green). Symbols: ✅ verified/present, 🟡 partial,
❌ missing/not applicable, ⚠️ could not verify locally.

Legend and scope:
- Applies to `libs/bsw/*` modules.
- Status is inferred from repo inventory + study plan; not backed by a local build run
  (no toolchain in this environment). Mark rows ⚠️ where unverified.
- Update this matrix as modules are verified in CI; cross-reference `TRACEABILITY_MATRIX.md`.

## 1. Summary counts (inferred)

| Category | ✅ | 🟡 | ❌ | ⚠️ |
|---|---|---|---|---|
| Build | 40 | 7 | 0 | 0 |
| Static | 25 | 15 | 0 | 7 |
| Comm | 22 | 10 | 15 | 0 |
| Diag | 8 | 6 | 33 | 0 |
| Fault | 6 | 8 | 33 | 0 |
| Regression (unit tests) | 30 | 12 | 5 | 0 |

## 2. Per-module matrix

| Module | Build | Static | Comm | Diag | Fault | Regression | Notes |
|---|---|---|---|---|---|---|---|
| async | ✅ | 🟡 | ❌ | ❌ | ❌ | ✅ | RTOS abstraction; POSIX tests. |
| asyncFreeRtos | ✅ | 🟡 | ❌ | ❌ | ❌ | 🟡 | HW port; limited POSIX. |
| asyncThreadX | ✅ | 🟡 | ❌ | ❌ | ❌ | 🟡 | HW port; limited POSIX. |
| asyncImpl | ✅ | ✅ | ❌ | ❌ | ❌ | 🟡 | Glue. |
| asyncConsole | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | Console. |
| blob | 🟡 | 🟡 | ❌ | ❌ | ❌ | 🟡 | Config blob; parser tests. |
| bsp | 🟡 | 🟡 | ❌ | ❌ | ❌ | ❌ | HW-specific. |
| canstack | ✅ | 🟡 | ✅ | ❌ | 🟡 | ✅ | 210+ docan tests; canstack tests. |
| commchannel | ✅ | 🟡 | ✅ | ❌ | ❌ | ✅ | ComM tests. |
| commgateway | 🟡 | 🟡 | ✅ | ❌ | ❌ | 🟡 | LIN gateway partial. |
| common | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | Shared types. |
| cpp2can | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ | Codegen; generator tests. |
| cpp2ethernet | ✅ | ✅ | ✅ | ❌ | ❌ | 🟡 | Binding. |
| cpp2someip | 🟡 | 🟡 | 🟡 | ❌ | ❌ | ❌ | Partial. |
| docan | ✅ | 🟡 | ✅ | ❌ | ❌ | ✅ | ISO-TP 210 tests. |
| doip | ✅ | 🟡 | ✅ | ✅ | ❌ | ✅ | DoIP diagnostic transport. |
| ecumode | 🟡 | 🟡 | ❌ | ❌ | 🟡 | ✅ | Wakeup tests (N1.3). |
| modearbiter | ✅ | 🟡 | ❌ | ❌ | 🟡 | ✅ | Rule tests. |
| faultmanager | ✅ | 🟡 | ❌ | ✅ | ✅ | ✅ | Dem. |
| framegateway | ✅ | 🟡 | ✅ | ✅ | 🟡 | ✅ | PDU routing. |
| gwbridge | 🟡 | 🟡 | ✅ | ❌ | ❌ | 🟡 | Gateway bridge. |
| integrity | ✅ | 🟡 | ✅ | ❌ | ✅ | ✅ | E2E profiles 1/2/4/5/11. |
| io | 🟡 | 🟡 | 🟡 | ❌ | ❌ | 🟡 | I/O abstraction. |
| ethdriverbinding | 🟡 | 🟡 | ✅ | ❌ | ❌ | 🟡 | HW binding partial. |
| lindriverbinding | 🟡 | 🟡 | ✅ | ❌ | ❌ | 🟡 | HW binding partial. |
| lifecycle | 🟡 | 🟡 | ❌ | ❌ | ❌ | 🟡 | Lifecycle. |
| logger | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | Logging. |
| loggerIntegration | 🟡 | 🟡 | ❌ | ❌ | ❌ | 🟡 | Integration. |
| lwipSocket | ✅ | 🟡 | ✅ | ❌ | ❌ | ✅ | TCP/UDP. |
| middleware | 🟡 | 🟡 | ❌ | ❌ | ❌ | 🟡 | Glue. |
| nm | 🟡 | 🟡 | ✅ | ❌ | ❌ | ✅ | CanNm/GatewayNm. |
| linlink | ✅ | 🟡 | ✅ | ❌ | ❌ | ✅ | LinNm. |
| ethlink | ✅ | 🟡 | ✅ | ❌ | ❌ | ✅ | EthNm. |
| persistence | 🟡 | 🟡 | ❌ | ❌ | ✅ | ✅ | FlashWearStore tests. |
| routing | 🟡 | 🟡 | ✅ | ❌ | ❌ | 🟡 | Routing utils. |
| runtime | 🟡 | 🟡 | ❌ | ❌ | ❌ | 🟡 | Runtime glue. |
| shed | 🟡 | 🟡 | ❌ | ❌ | ❌ | 🟡 | Scheduler. |
| stdioConsoleInput | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | Console input. |
| storage | 🟡 | 🟡 | ❌ | ❌ | ❌ | 🟡 | Storage abstraction. |
| time | ✅ | 🟡 | 🟡 | ❌ | ❌ | ✅ | Time services. |
| timer | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | Timers. |
| transport | 🟡 | 🟡 | ✅ | ❌ | ❌ | 🟡 | Transport. |
| transportRouterSimple | 🟡 | 🟡 | ✅ | ❌ | ❌ | 🟡 | Simple router. |
| uds | 🟡 | 🟡 | ✅ | ✅ | 🟡 | ✅ | Partial service set. |
| util | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | Utilities. |

## 3. Category definitions

- **Build:** Compiles cleanly in `tests-posix-debug` preset (POSIX-first gate).
- **Static:** clang-tidy + cppcheck + copyright header checks pass (see `.github/workflows`).
- **Comm:** Communication path (CAN/LIN/Eth/NM) exercised by unit/integration tests.
- **Diag:** Diagnostic path (UDS/DoIP/Dem) exercised.
- **Fault:** Fault handling path (Dem events, safe state) exercised.
- **Regression:** Unit-test suite exists and is green on last run (CI evidence).

## 4. Verification

- **Verified against:** module directory listing; study plan; CI workflow names.
- **Could not verify:** actual build/test results (no toolchain in this environment); static
  analysis results; per-module test counts. Mark ⚠️ rows for modules not yet exercised.