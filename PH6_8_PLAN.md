# Phase 6–8 Implementation Plan: NvRAM Manager, Diagnostic Event Manager, E2E Protection

Branch: `feature/phase6-8-nvm-dem-e2e` (cut from `Ecu_mode`)

## 1. Goals

| Phase | Module (`libs/bsw/...`) | Scope |
|-------|-------------------------|-------|
| 6 | `nvram` | Block-based NVRAM manager: RAM mirror, immediate vs deferred writes, CRC-8/16/32, single/redundant storage, `Fee` sector-rotation wear leveling, `Eep` EEPROM emulation, `shutdown()` flush for Phase-5 SLEEP, async API via callback |
| 7 | `dem` | DTC storage: record/clear, ISO-14229 status bits, freeze-frame, extended data, aging over operation cycles, primary/secondary memory limits, permanent DTCs, transport-mode suppression (Phase-5), NVRAM persistence hooks (Phase-6) |
| 8 | `e2e` | E2E profiles P01/P02/P04/P05/P11: CRC + counter + data-ID protect/check, timeout monitoring, gateway-ready `protect`/`check` per PDU |

Naming avoids AUTOSAR API collisions: `nvram::Nvram::read/write`, `dem::Dem::recordFault/clearFault`,
`e2e::E2e::protect/check` (never `NvM_*`, `Dem_*`, `E2E_Pxx*`).

## 2. Dependency Strategy (no cycles)

- `nvram` depends only on `etl`, `util`. It does **not** include `ecumode`; Phase-5 SLEEP integration
  is done via `Nvram::setDeferredWritesBlocked(bool)` + `shutdown()` called by the ECU-mode manager.
- `dem` depends only on `etl`, `util`. NVRAM persistence is dependency-free: `Dem::serialize()` /
  `Dem::deserialize()` produce a byte blob the integrator stores in any NVRAM block (Phase-6 block 2).
- `e2e` is header-mostly pure (CRC + counters), depends on `etl` only. FrameGateway/XCP integration is
  documented, not hard-coded, so `framegateway` never becomes a dependency of `e2e`.
- `uds` integration is additive (services 0x03/0x14/0x19 call into `dem::Dem`) and left to the
  integrator; this change set does not modify `uds/` to keep the diff reviewable.

## 3. File Map

```text
libs/bsw/nvram/
  CMakeLists.txt  BUILD.bazel  module.spec
  include/nvram/Nvram.h  include/nvram/Fee.h  include/nvram/Eep.h  include/nvram/Crc.h
  src/nvram/Nvram.cpp  src/nvram/Fee.cpp  src/nvram/Eep.cpp  src/nvram/Crc.cpp
  config/nvram_config.yaml  doc/index.rst
  test/CMakeLists.txt  test/src/nvram/NvramTest.cpp  test/src/nvram/FeeTest.cpp
libs/bsw/dem/
  CMakeLists.txt  BUILD.bazel  module.spec
  include/dem/Dem.h  src/dem/Dem.cpp
  config/dem_config.yaml  doc/index.rst
  test/CMakeLists.txt  test/src/dem/DemTest.cpp
libs/bsw/e2e/
  CMakeLists.txt  BUILD.bazel  module.spec
  include/e2e/E2e.h  include/e2e/Crc.h  src/e2e/E2e.cpp  src/e2e/Crc.cpp
  config/e2e_config.yaml  doc/index.rst
  test/CMakeLists.txt  test/src/e2e/E2eTest.cpp
tools/blob/nvram_dem_e2e.py   # YAML -> blob converter extension (nvram/dem/e2e sections)
doc/sphinx/... (via each module doc/index.rst)
```

`libs/bsw/CMakeLists.txt` gains `add_subdirectory(nvram)`, `add_subdirectory(dem)`, `add_subdirectory(e2e)`
inside the `OPENBSW_CONFIGURE_GENERIC_BSW` block (same pattern as `storage`/`uds`).

## 4. API Summary

### Phase 6 — `nvram::Nvram` (singleton)

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
- `Fee` maps logical block address -> physical sector with least erase cycles; `getWearLevel()` = max
  cycles / `maxEraseCycles`.
- Corruption test hook: `Nvram::corruptFlashForTest(blockId, offset, xorMask)`.

### Phase 7 — `dem::Dem` (singleton)

```cpp
bool init(DtcConfig const* dtcs, size_t count, DemConfig const& cfg);
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

### Phase 8 — `e2e::E2e` (singleton)

```cpp
bool init(ProfileConfig const* profiles, size_t count);
CheckResult protect(uint8_t profileId, uint8_t* data, uint16_t len, bool inc = true);
CheckResult check(uint8_t profileId, uint8_t const* data, uint16_t len);
protectProfile1/checkProfile1 ... protectProfile11/checkProfile11 (typed helpers)
uint32_t getCounter(profileId) const;  void reset(profileId);  void mainFunction();
```

Layouts (payload includes E2E footer, big-endian data-ID where explicit):

- P01: `[data(N-2)][ctrHi4|dataIdNibble][CRC8]` over data + header byte + implicit 16-bit data-ID.
- P02: `[data(N-2)][ctrHi4|nibble][CRC8]` with rotating 1-byte data-ID list (index = counter % list size).
- P04: `[data(N-12)][len16 BE][ctr16 BE][dataId32 BE][CRC32]` (CRC over full header + data).
- P05: `[data(N-3)][ctr8][dataId16 BE implicit in CRC][CRC16]` (CRC over data + ctr + data-ID).
- P11: `[data(N-2)][ctr8][CRC8]` with 16-bit data-ID folded into CRC.
- Counter validation rejects duplicates, accepts next/gap (forward window), rejects rollback.

`Crc.h` provides `crc8SaeJ1850`, `crc16CcittFalse`, `crc32Ieee` (bitwise, table-free, MISRA-friendly).

## 5. Testing

- `NvramTest`: write/read round-trip, CRC error detection via corruption hook, immediate vs deferred
  persistence across `shutdown()`, redundant fallback, wear-counter increments, invalid block handling.
- `FeeTest`: logical read/write, wear spreads across sectors (max-min erase delta small), bad-sector
  remap keeps data readable, wear level 0–100%.
- `DemTest`: record → pending+confirmed, freeze frame stored, extended data round-trip, aging clears
  after N warmup cycles, permanent DTC survives clear-all, suppression blocks recording, serialize round-trip.
- `E2eTest`: per-profile protect→check OK; single-bit flip → CRC error; replay same frame → counter error;
  data-ID mismatch → data-ID error; counter wrap accepted.

Build & run (Windows/PowerShell, from repo root):

```powershell
cmake -S . -B build/unit -DBUILD_EXECUTABLE=unitTest -DCMAKE_BUILD_TYPE=Debug
cmake --build build/unit --target nvramTest demTest e2eTest -j
ctest --test-dir build/unit -R "(nvramTest|demTest|e2eTest)" --output-on-failure
```

Bazel: `bazel test //libs/bsw/nvram/... //libs/bsw/dem/... //libs/bsw/e2e/...`.

## 6. Integration Cookbook (follow-ups, not in this diff)

1. Phase-5 ECU mode: call `nvram::Nvram::instance().shutdown()` before SLEEP; set
   `setDeferredWritesBlocked(true)` in STANDBY; `dem::Dem::instance().setDtcRecordingSuppressed(true)`
   in TRANSPORT; `clearAllDtcs()` on MANUFACTURING end-of-line.
2. Phase-3 UDS: route 0x03/0x07/0x0A/0x14/0x19 to `Dem` getters; route XCP DOWNLOAD (0xD0) markings to
   `Nvram::writeAsync` on the XCP calibration block.
3. Phase-4 gateway: wrap RX with `E2e::check()` (drop on fail unless passthrough configured) and TX with
   `E2e::protect()` per frame→profile map.
4. Blob: `python tools/blob/nvram_dem_e2e.py --nvram <yaml> --dem <yaml> --e2e <yaml> --out comm_stack.blob.json`.

## 7. Risks / Limits

- `Fee` emulates flash in RAM (no real HAL link yet); HAL binding (`microcontroller::Flash`) is a
  follow-up behind an `IFlashDriver` interface to avoid platform coupling.
- `Dem` freeze-frame signals are value snapshots, not live RTE reads; RTE binding is a follow-up.
- E2E timeout monitoring uses caller tick (`setTickProvider`); default tick is a monotonic counter
  incremented in `mainFunction()`.
