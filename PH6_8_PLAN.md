<!--
 *******************************************************************************
  Copyright (c) 2026 Accenture

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
 *******************************************************************************
-->

# Phase 6–8 Implementation Plan: NvRAM Manager, Diagnostic Event Manager, Integrity Protection

Branch: `feature/phase6-8-nvm-faultmanager-integrity` (cut from `Ecu_mode`)

## 1. Goals

| Phase | Module (`libs/bsw/...`) | Scope |
|-------|-------------------------|-------|
| 6 | `persistence` | Block-based Persistence manager: RAM mirror, immediate vs deferred writes, CRC-8/16/32, single/redundant storage, `FlashWearStore` sector-rotation wear leveling, `EepromStore` EEPROM emulation, `shutdown()` flush for Phase-5 SLEEP, async API via callback |
| 7 | `faultmanager` | DTC storage: record/clear, ISO-14229 status bits, freeze-frame, extended data, aging over operation cycles, primary/secondary memory limits, permanent DTCs, transport-mode suppression (Phase-5), Persistence persistence hooks (Phase-6) |
| 8 | `integrity` | Integrity profiles P01/P02/P04/P05/P11: CRC + counter + data-ID protect/check, timeout monitoring, gateway-ready `protect`/`check` per PDU |

Naming avoids AUTOSAR API collisions: `persistence::Persistence::read/write`, `faultmanager::FaultManager::recordFault/clearFault`,
`integrity::IntegrityGuard::protect/check` (never `NvM_*`, `Dem_*`, `E2E_Pxx*`).

## 2. Dependency Strategy (no cycles)

- `persistence` depends only on `etl`, `util`. It does **not** include `ecumode`; Phase-5 SLEEP integration
  is done via `Persistence::setDeferredWritesBlocked(bool)` + `shutdown()` called by the ECU-mode manager.
- `faultmanager` depends only on `etl`, `util`. Persistence persistence is dependency-free: `FaultManager::serialize()` /
  `FaultManager::deserialize()` produce a byte blob the integrator stores in any Persistence block (Phase-6 block 2).
- `integrity` is header-mostly pure (CRC + counters), depends on `etl` only. FrameGateway/XCP integration is
  documented, not hard-coded, so `framegateway` never becomes a dependency of `integrity`.
- `uds` integration is additive (services 0x03/0x14/0x19 call into `faultmanager::FaultManager`) and left to the
  integrator; this change set does not modify `uds/` to keep the diff reviewable.

## 3. File Map

```text
libs/bsw/persistence/
  CMakeLists.txt  BUILD.bazel  module.spec
  include/persistence/Persistence.h  include/persistence/FlashWearStore.h  include/persistence/EepromStore.h  include/persistence/Crc.h
  src/persistence/Persistence.cpp  src/persistence/FlashWearStore.cpp  src/persistence/EepromStore.cpp  src/persistence/Crc.cpp
  config/persistence_config.yaml  doc/index.rst
  test/CMakeLists.txt  test/src/persistence/PersistenceTest.cpp  test/src/persistence/FlashWearStoreTest.cpp
libs/bsw/faultmanager/
  CMakeLists.txt  BUILD.bazel  module.spec
  include/faultmanager/FaultManager.h  src/faultmanager/FaultManager.cpp
  config/faultmanager_config.yaml  doc/index.rst
  test/CMakeLists.txt  test/src/faultmanager/FaultManagerTest.cpp
libs/bsw/integrity/
  CMakeLists.txt  BUILD.bazel  module.spec
  include/integrity/IntegrityGuard.h  include/integrity/Crc.h  src/integrity/IntegrityGuard.cpp  src/integrity/Crc.cpp
  config/integrity_config.yaml  doc/index.rst
  test/CMakeLists.txt  test/src/integrity/IntegrityGuardTest.cpp
tools/blob/persistence_fault_integrity.py   # YAML -> blob converter extension (persistence/faultmanager/integrity sections)
doc/sphinx/... (via each module doc/index.rst)
```

`libs/bsw/CMakeLists.txt` gains `add_subdirectory(persistence)`, `add_subdirectory(faultmanager)`, `add_subdirectory(integrity)`
inside the `OPENBSW_CONFIGURE_GENERIC_BSW` block (same pattern as `storage`/`uds`).

## 4. API Summary

### Phase 6 — `persistence::Persistence` (singleton)

```cpp
bool init(BlockConfig const* blocks, size_t count);
Result read(uint16_t blockId, void* data, uint16_t length, uint16_t offset = 0) const;
Result write(uint16_t blockId, void const* data, uint16_t length, uint16_t offset = 0);
using Callback = etl::delegate<void(Result)>;
Result readAsync(blockId, data, len, Callback, offset = 0) const;
Result writeAsync(blockId, data, len, Callback, offset = 0);
Result invalidateBlock(blockId);  Result setToDefault(blockId);
Result shutdown();                // flush deferred blocks to flash
void mainFunction();              // background wear-leveling tick
void setDeferredWritesBlocked(bool);  // Phase-5 SLEEP/STANDBY hook
uint32_t getWriteCounter(blockId) const;
```

- RAM mirror always updated on `write()`; flash updated immediately iff `immediateWrite` else on `shutdown()`.
- Redundant blocks keep 2 flash copies; read prefers copy 0, falls back to copy 1, then default per
  `recoveryOnCrcError`.
- `FlashWearStore` maps logical block address -> physical sector with least erase cycles; `getWearLevel()` = max
  cycles / `maxEraseCycles`.
- Corruption test hook: `Persistence::corruptFlashForTest(blockId, offset, xorMask)`.

### Phase 7 — `faultmanager::FaultManager` (singleton)

```cpp
bool init(DtcConfig const* dtcs, size_t count, FaultManagerConfig const& cfg);
void recordFault(uint32_t dtc, Severity sev = Severity::Major);
void clearFault(uint32_t dtc);
uint8_t getDtcStatus(uint32_t dtc) const;          // ISO-14229-1 status byte
etl::vector<uint32_t> getStoredDtcs() const;      // confirmed/pending for UDS 0x03
etl::vector<DtcWithStatus> getDtcsWithStatus() const;  // UDS 0x19
bool getFreezeFrame(uint32_t dtc, FreezeFrame&) const;
bool getExtendedData(uint32_t dtc, ExtendedData&) const;
bool storeExtendedData(uint32_t dtc, uint8_t const* d, uint8_t len);
void clearAllDtcs();                              // UDS 0x14
void notifyOperationCycle(char const* cycleName); // aging input
void setDtcRecordingSuppressed(bool);             // Phase-5 TRANSPORT hook
void mainFunction();                              // aging + memory-limit enforcement
size_t serialize(span<uint8_t>) const;  bool deserialize(span<uint8_t const>);
```

- Status bits used: `TEST_FAILED(0x01) | TEST_FAILED_THIS_OC(0x02) | PENDING(0x04) | CONFIRMED(0x08) |
  TEST_NOT_COMPLETED_SINCE_CLEAR(0x10) | TEST_FAILED_SINCE_CLEAR(0x20) |
  TEST_NOT_COMPLETED_THIS_OC(0x40) | WARNING_INDICATOR(0x80)`.
- Aging: `failureCycles` reset on `recordFault`; `notifyOperationCycle("WARMUP_CYCLE")` (or any
  configured cycle) increments `agingCycles` for cleared-but-confirmed DTCs and erases them at threshold.
- Permanent DTCs never age and survive `clearAllDtcs()` unless explicitly cleared via `clearFault`
  after the fault is gone (WWH-OBD semantic subset).
- Freeze frame captures `timestamp` (caller-provided tick via `setTimeProvider`) + up to 6 signal values.

### Phase 8 — `integrity::IntegrityGuard` (singleton)

```cpp
bool init(ProfileConfig const* profiles, size_t count);
CheckResult protect(uint8_t profileId, uint8_t* data, uint16_t len, bool inc = true);
CheckResult check(uint8_t profileId, uint8_t const* data, uint16_t len);
protectProfile1/checkProfile1 ... protectProfile11/checkProfile11 (typed helpers)
uint32_t getCounter(profileId) const;  void reset(profileId);  void mainFunction();
```

Layouts (payload includes Integrity footer, big-endian data-ID where explicit):

- P01: `[data(N-2)][ctrHi4|dataIdNibble][CRC8]` over data + header byte + implicit 16-bit data-ID.
- P02: `[data(N-2)][ctrHi4|nibble][CRC8]` with rotating 1-byte data-ID list (index = counter % list size).
- P04: `[data(N-12)][len16 BE][ctr16 BE][dataId32 BE][CRC32]` (CRC over full header + data).
- P05: `[data(N-3)][ctr8][dataId16 BE implicit in CRC][CRC16]` (CRC over data + ctr + data-ID).
- P11: `[data(N-2)][ctr8][CRC8]` with 16-bit data-ID folded into CRC.
- Counter validation rejects duplicates, accepts next/gap (forward window), rejects rollback.

`Crc.h` provides `crc8SaeJ1850`, `crc16CcittFalse`, `crc32Ieee` (bitwise, table-free, MISRA-friendly).

## 5. Testing

- `PersistenceTest`: write/read round-trip, CRC error detection via corruption hook, immediate vs deferred
  persistence across `shutdown()`, redundant fallback, wear-counter increments, invalid block handling.
- `FlashWearStoreTest`: logical read/write, wear spreads across sectors (max-min erase delta small), bad-sector
  remap keeps data readable, wear level 0–100%.
- `FaultManagerTest`: record → pending+confirmed, freeze frame stored, extended data round-trip, aging clears
  after N warmup cycles, permanent DTC survives clear-all, suppression blocks recording, serialize round-trip.
- `IntegrityGuardTest`: per-profile protect→check OK; single-bit flip → CRC error; replay same frame → counter error;
  data-ID mismatch → data-ID error; counter wrap accepted.

Build & run (Windows/PowerShell, from repo root):

```powershell
cmake -S . -B build/unit -DBUILD_EXECUTABLE=unitTest -DCMAKE_BUILD_TYPE=Debug
cmake --build build/unit --target persistenceTest faultmanagerTest integrityTest -j
ctest --test-dir build/unit -R "(persistenceTest|faultmanagerTest|integrityTest)" --output-on-failure
```

Bazel: `bazel test //libs/bsw/persistence/... //libs/bsw/faultmanager/... //libs/bsw/integrity/...`.

## 6. Integration Cookbook (follow-ups, not in this diff)

1. Phase-5 ECU mode: call `persistence::Persistence::instance().shutdown()` before SLEEP; set
   `setDeferredWritesBlocked(true)` in STANDBY; `faultmanager::FaultManager::instance().setDtcRecordingSuppressed(true)`
   in TRANSPORT; `clearAllDtcs()` on MANUFACTURING end-of-line.
2. Phase-3 UDS: route 0x03/0x07/0x0A/0x14/0x19 to `FaultManager` getters; route XCP DOWNLOAD (0xD0) markings to
   `Persistence::writeAsync` on the XCP calibration block.
3. Phase-4 gateway: wrap RX with `IntegrityGuard::check()` (drop on fail unless passthrough configured) and TX with
   `IntegrityGuard::protect()` per frame→profile map.
4. Blob: `python tools/blob/persistence_fault_integrity.py --persistence <yaml> --faultmanager <yaml> --integrity <yaml> --out comm_stack.blob.json`.

## 7. Risks / Limits

- `FlashWearStore` emulates flash in RAM (no real HAL link yet); HAL binding (`microcontroller::Flash`) is a
  follow-up behind an `IFlashDriver` interface to avoid platform coupling.
- `FaultManager` freeze-frame signals are value snapshots, not live RTE reads; RTE binding is a follow-up.
- Integrity timeout monitoring uses caller tick (`setTickProvider`); default tick is a monotonic counter
  incremented in `mainFunction()`.
