# Network Management (NM) — Use Case Analysis (Automotive)

Companion to `PRODUCTION_GRADE_ROADMAP.md` (Phase 9 NM) and `PRODUCTION_GRADE_PLAN.md` (Phase A).
Analyses all automotive NM use cases against the implemented `CanNm` + `GatewayNm` modules.

## Standards
- AUTOSAR Generic NM + CanNm (NM PDU, state machine, partial networking)
- ISO 11898 (CAN/CAN-FD), ISO 17987 (LIN), IEEE 802.3 (Ethernet)
- ISO 14229 / ISO 13400 (diagnostics keep-awake)

## Use cases and coverage

| # | Use case | Automotive need | CanNm/GatewayNm support | Status |
|---|---|---|---|---|
| 1 | Active NM node transmits NM PDUs and coordinates sleep | Standard node | `activeNm=true`, `transmitPdu()` periodic TX | ✅ |
| 2 | Passive NM node listens only, never transmits | Low-cost node / gateway port | `activeNm=false` suppresses TX; still tracks alive + sleep | ✅ |
| 3 | Network request/release by communication users | ComM users | `userNetworkRequest/Release`, `registerUser`, aggregation | ✅ |
| 4 | Multi-user aggregation per channel | Multiple SWCs | `requestCount_`, per-user array | ✅ |
| 5 | No-communication mode inhibits requests | ComM NoComm | `setCommMode(NoCommunication)` clears requests | ✅ |
| 6 | Silent communication suppresses TX but keeps NM | ComM SilentComm | `SilentCommunication` suppresses TX | ✅ |
| 7 | Sleep coordination (ready sleep → prepare bus sleep → bus sleep) | Power saving | State machine + timers tWaitBusSleep | ✅ |
| 8 | Repeat message phase on wake | Ensure all nodes see activity | RepeatMessage state, tRepeatMessage | ✅ |
| 9 | Remote wakeup via NM PDU | Wake sleeping ECU | AW bit triggers wakeupCallback + transition | ✅ |
| 10 | NM timeout / node loss detection | Diagnostics | tNm expiry removes node from alive list | ✅ |
| 11 | Partial networking (selective wake-up) | Energy saving | PN group filter, PNI bit, `pnGroup` | ✅ |
| 12 | NM coordinator decides sleep readiness | Central coordinator | `nmCoordinator` sets CSR bit in ReadySleep | ✅ |
| 13 | Gateway coordinates multi-channel sleep | Gateway ECU | `GatewayNm`: request propagation, all-ready gate | ✅ |
| 14 | Bus-off recovery | Robustness | `onBusOff()` → BusSleep + callback | ✅ |
| 15 | ECU mode integration (sleep callback) | EcuM | `setSleepCallback`, `setWakeupCallback` | ✅ |
| 16 | Node user data exchange | Application data | `setUserData`/`getUserData`, per-node storage | ✅ |

## Remaining gaps (future slices)
- LinNm / EthNm / UdpNm (LIN/Ethernet NM) — requires LinIf/EthIf drivers.
- NM PDU forwarding between buses (gateway NM forwarding) — currently only sleep coordination.
- Partial networking PN filter lists (multiple groups per node).
- NM coordinator algorithm (voting) beyond CSR bit.
- Integration with ComM channel mode (ComM module not yet implemented).
- Bus-off recovery retry / re-initialisation strategy.
- Timing configuration per AUTOSAR (T_NM, T_REPEAT_MESSAGE, T_WAIT_BUS_SLEEP, T_TX) validation.
- NM variants with/without user data, with/without PN.

## Gateway NM behaviour
`GatewayNm::mainFunction()`:
1. If any channel requests the network, propagate request to all channels (global wake).
2. Run each channel's `CanNm::mainFunction()`.
3. Global sleep allowed only when all channels are in ReadySleep/PrepareBusSleep/BusSleep
   with no pending requests; then fire `globalSleepCallback`.
