/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

# Production-Grade Deep Review

Branch: `study/production-grade-deep-review`

Source: `codebase-memory-auditor` (Tier 3) audit of `libs/bsw`, emphasizing `csm`, `wdgm`, `uds`, `faultmanager`, `nm`, `commgateway`, `ethlink`, `linlink`, `linknm`, `commchannel`, and `modearbiter`.

## P0 - Build / CI / integration gaps

1. Missing `module.spec` and `BUILD.bazel` for newly added modules (`csm`, `wdgm`, `nm`, `commchannel`, `modearbiter`, `linknm`, `ethlink`, `linlink`).
2. `linknm` has no direct tests or test CMake integration.
3. Graph/index metadata is stale for changed modules; re-index before relying on graph queries.

## P1 - Logic defects / missing error handling

4. `CanNm::registerUser()` always returns slot 0.
5. `LinkManager` duplicates `CanNm` and inherits the same defect.
6. NM node liveness tracking does not validate caller `nodeId` against decoded `srcNodeId`.
7. Unsigned time subtraction can wrap in NM/WdgM paths.
8. WdgM Q&A resets every cycle and can report false failures.
9. WdgM safe-state callback can be invoked repeatedly on every failing cycle.
10. `ModeArbiter` uses process-global static state.
11. `FaultManager::recordFault` immediately confirms DTCs (single-trip).
12. `SoftwareCryptoService` is not production-grade and not thread-safe.

## P2 - Missing functionality / traceability

13. UDS transfer services `0x34/0x36/0x37` are not implemented.
14. `InputOutputControlByIdentifier` has no tests.
15. `GatewayNm` has no dedicated tests.
16. `CommChannelManager`, `EthChannel`, `CommStatistics`, and `LinScheduler` lack direct tests.
17. Traceability tags are incomplete for new modules.

## P3 - Test quality / maintainability

18. Csm tests rely on a process-global singleton and do not reset state.
19. `ethlink`/`linlink` tests are thin aliases over `LinkManager`.
20. `CommChannelManager::registerChannel` capacity behavior is convoluted.

## Limitations

- No compiler/toolchain or test execution was available to the auditor; findings are static-analysis based.
- Graph index is best-effort and stale for some modules; source reads were used as fallback.
- `faultmanager/doc` and `commgateway/doc` are excluded from indexing.
- No runtime, concurrency stress, or hardware-in-the-loop validation was performed.
