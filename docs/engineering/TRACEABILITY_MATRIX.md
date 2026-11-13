# Traceability Matrix — Eclipse OpenBSW (bootstrap)

**Status:** Bootstrap (first pass). Requirement IDs are study-defined (`SYS-xxx`) and are
referenced from source via Doxygen `\req` / `\trace` tags. No external requirements tool
(DOORS/Polarion) is present in the repository.

## 1. Convention

- Requirement IDs use `SYS-<MODULE>-<NNN>`.
- Source tags: Doxygen `\req SYS-...` in module headers / class docs.
- Test tags: `\trace TEST: <TestSuite> (<path>)`.
- Evidence: focused test name + commit hash + build preset.

## 2. Bootstrapped mappings

| Req ID | Module | Source | Test | Status |
|---|---|---|---|---|
| SYS-ECUMODE-001 | ecumode | `libs/bsw/ecumode/include/ecumode/CommStateManagerPhase5.h` | `EcuModeManagerTest` | Partial |
| SYS-ECUMODE-002 | ecumode | same | `EcuModeManagerTest` | Partial |
| SYS-ECUMODE-003 | ecumode | same | `EcuModeManagerTest` | Partial |
| SYS-ECUMODE-004 | ecumode | same | `wakeup_source_propagates_to_normal_mode`, `multiple_wakeup_sources_are_reported_and_propagate_from_standby` | Partial |
| SYS-ECUMODE-005 | ecumode | same | `EcuModeManagerTest` (run requests) | Partial |
| SYS-ECUMODE-006 | ecumode | same | `EcuModeManagerTest` | Partial |
| SYS-PERSIST-001 | persistence | `libs/bsw/persistence/include/persistence/FlashWearStore.h` | `FlashWearStoreTest` | Partial |
| SYS-PERSIST-002 | persistence | same | `FlashWearStoreTest.DelegatesToFlashDriverWhenBound` | Partial |
| SYS-UDS-001 | uds | `libs/bsw/uds/include/uds/services/cleardiagnosticinformation/ClearDiagnosticInformation.h` | `ClearDiagnosticInformationTest`, `UdsDemSecurityE2ETest.clear_dtc_invokes_dem_clear_all` | Partial |
| SYS-UDS-002 | uds | `libs/bsw/uds/include/uds/services/readdtcinformation/ReadDTCInformation.h` | `ReadDTCInformationTest`, `UdsDemSecurityE2ETest.read_dtc_invokes_dem_get_dtcs_with_status` | Partial |
| SYS-UDS-003 | uds | `libs/bsw/uds/include/uds/services/controldtcsetting/ControlDTCSetting.h` | `ControlDTCSettingTest`, `UdsDemSecurityE2ETest.control_dtc_setting_invokes_dem_suppression` | Partial |
| SYS-UDS-004 | uds | `libs/bsw/uds/include/uds/services/securityaccess/SecurityAccess.h` | `SecurityAccessTest`, `UdsDemSecurityE2ETest.security_access_seed_key_round_trip` | Partial |
| SYS-UDS-005 | uds | `libs/bsw/uds/include/uds/services/...` (pending 0x34/0x36/0x37 transfer services) | not implemented | Pending |
| SYS-FAULT-001 | faultmanager | `libs/bsw/faultmanager` | `FaultManagerTest` (if present) | Not verified |

## 3. Next steps (N1.4)

1. Tag remaining delivered modules (ComM, ModeArbiter, LinNm, EthNm, integrity, uds services).
2. Add per-test `\req` references where suites map 1:1.
3. Generate matrix automatically (script) once tags are in place.
4. Keep this document in sync with `MIDDLEWARE_STACK_STUDY_PLAN.md` / `NEXT_ACTIVITY_PLAN.md`.

## 4. Verification

- Focused tests: `ecumode` + `persistence` unit tests (POSIX) — not run locally (no toolchain).
- Evidence: commit hash + test logs to be archived in CI.