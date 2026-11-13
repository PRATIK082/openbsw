# Busload & Worst-Case Latency Analysis — Eclipse OpenBSW

**Status:** Template + example numbers (N5.3). Values below are **illustrative**; replace with
measured/derived values per ECU configuration. No bus traces were captured in this environment.

## 1. Purpose

Quantify per-bus load and worst-case end-to-end latency for CAN, LIN, Ethernet to confirm timing
budgets (production blocker). Used as input to scheduling (shed), NM timing, and gateway design.

## 2. Method

For each bus:
1. Enumerate periodic + sporadic frames (source: DBC/LDF/FIBEX description files + codegen tables).
2. Compute frame time from bit rate + DLC + overhead (stuffing assumed 20% worst case).
3. Busload % = sum(frame times × frequency) / bus capacity.
4. Worst-case latency = max queuing + transmission + gateway forwarding + task scheduling delay.
5. Compare against budget; flag if > 70% load or latency > target.

Formulas:
- CAN classic frame time (µs) ≈ (47 + 8×DLC) bits / bitrate (with stuffing 1.2 factor).
- CAN-FD frame time ≈ (overhead + data bits) / data bitrate.
- LIN frame time ≈ (10 bits/byte × (DLC+3)) / bitrate (header+response).
- Eth frame time ≈ (preamble+SFD+MAC+IP+UDP+payload+CRC+IFG) / bitrate.

## 3. Assumptions (example ECU)

- CAN1: 500 kbit/s classic, 8-byte DLC max, 20% stuffing.
- CAN2 (FD): 500 kbit/s arbitration / 2 Mbit/s data, 64-byte DLC.
- LIN1: 19.2 kbit/s, master schedule, 8-byte max.
- Ethernet: 100 Mbit/s, VLAN tagged, 1500-byte MTU.
- Gateway forwards selected signals CAN1↔CAN2↔Eth with one shed tick (1 ms) delay.

## 4. CAN busload (example)

| Frame | ID | DLC | Period (ms) | Rate (fps) | Frame time (µs) | Load % |
|---|---|---|---|---|---|---|
| EngineSpeed | 0x100 | 8 | 10 | 100 | 200 | 2.0 |
| VehicleSpeed | 0x101 | 8 | 20 | 50 | 200 | 1.0 |
| BrakeStatus | 0x102 | 4 | 10 | 100 | 140 | 1.4 |
| NM_PDU | 0x500 | 8 | 200 | 5 | 200 | 0.1 |
| DiagReq (sporadic) | 0x7E0 | 8 | — | 2 | 200 | 0.04 |
| **Total** | | | | | | **4.5** |

Budget: < 30% periodic load; ✅ within budget. Worst-case TX queue delay @ 95th percentile: ~0.6 ms.

## 5. LIN busload (example)

| Frame | Schedule slot | DLC | Period (ms) | Frame time (ms) | Load % |
|---|---|---|---|---|---|
| MasterReq | 0 | 8 | 20 | 5.2 | 26.0 |
| SlaveResp_A | 1 | 8 | 20 | 5.2 | 26.0 |
| SlaveResp_B | 2 | 4 | 40 | 3.6 | 9.0 |
| **Total** | | | | | **61.0** |

Budget: < 60% load; ⚠️ at limit — reduce schedule or increase bitrate to 20 kbit/s.

## 6. Ethernet busload (example)

| Flow | Payload (B) | Rate (pps) | Frame time (µs) | Load % |
|---|---|---|---|---|---|
| SOMEIP telemetry | 200 | 100 | 3.2 | 0.032 |
| DoIP diagnostic | 128 | 5 | 2.4 | 0.0012 |
| NM keepalive | 48 | 10 | 1.6 | 0.00016 |
| **Total** | | | | **0.033** |

Budget: < 50%; ✅. Worst-case latency dominated by switch queuing (not computed here).

## 7. Worst-case latency (example, end-to-end)

| Path | Components | WC latency (ms) | Target (ms) | Status |
|---|---|---|---|---|
| CAN signal → SWC (RX) | driver + canstack + shed tick | 1.2 | 2.0 | ✅ |
| SWC → CAN TX | shed tick + canstack + driver | 1.5 | 2.0 | ✅ |
| CAN → Gateway → CAN2 | canstack + gateway + shed + canstack | 3.0 | 5.0 | ✅ |
| UDS over DoIP (Eth) | lwip + doip + uds handler | 8.0 | 10.0 | ✅ |
| NM sleep coordination | NM state + ComM + EcuMode | 50.0 | 100.0 | ✅ |

## 8. Tooling & evidence

- Source of truth: DBC/LDF/FIBEX files (see `DESCRIPTION_FILE_DISCIPLINE.md`) + generated tables
  (`tools/can/dbc_to_openbsw.py`).
- Capture: CANalyzer/PCAN traces, `socketcan` + `candump`, Ethernet `tcpdump`, gateway logs.
- Compute: spreadsheet or script; store results + traces in evidence pack per phase gate.
- Re-run on every description-file change (CI hash check triggers re-analysis).

## 9. Verification

- **Could not verify:** no bus traces or toolchain in this environment; numbers are illustrative.
- **Action:** populate with real ECU config before production release.