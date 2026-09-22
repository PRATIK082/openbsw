..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

architecture
============

Modules
-------

* ``LinHwInterface`` - LIN controller abstraction (mirrors
  ``canstack::CanHwInterface``).
* ``LinScheduler`` - master schedule table executor (cyclic slots;
  ``cycleTimeMs == 0`` is event-triggered).
* ``LinChannel`` - channel manager: PID parity, classic/enhanced checksums,
  schedule execution, sleep/wakeup, per-kind error counters.
* ``EthTransportIf`` - injectable datagram transport. Production code binds
  this to ``cpp2ethernet`` sockets; tests use ``EthLoopbackChannel``.
* ``EthIpduManager`` - PDU <-> datagram mapping, rx demultiplexing
  (source-port map, default = ``primaryPduId``), cyclic retransmission.
* ``SignalGateway`` - signal-name keyed routing to CAN/LIN/ETH with optional
  transform (NaN drops) and rate limiting (pending queue + ``mainFunction``).
* ``TimeoutMonitor`` - signal- and frame-level rx timeouts, one-shot callbacks
  re-armed by activity.
* ``TxConfirmationMgr`` - process-wide tx acknowledgment tracker (register /
  confirm / timeout with ``success = false``).
* ``CommStateManager`` - per-channel state machine (CAN -> ``BUS_OFF`` past
  255 errors, LIN -> ``PASSIVE`` past threshold, ETH -> ``COMM_FAILURE``) with
  optional timed auto-recovery.
* ``InterruptRouter`` - ISR fan-out with static wrappers for BSP vectors.
* ``CommStack`` - lifecycle orchestrator wiring all of the above to the
  existing ``canstack::CanInterface``.

Signal value encoding on LIN/ETH
--------------------------------

CAN keeps full DBC fidelity (pack/unpack via ``SignalDatabase``). LIN frames
carry the raw value little-endian in the frame payload; Ethernet PDUs carry
the IEEE 754 ``double``. These are documented simplifications for gateway
prototyping, not a wire-format standard.
