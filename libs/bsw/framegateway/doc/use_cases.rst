..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

SDV / zonal use cases
=====================

Zonal gateway (CAN FD <-> Ethernet)
-----------------------------------

A 64-byte CAN FD frame carries three PDUs: sensor data for SOME/IP,
an actuator command for a LIN zone, and locally consumed signals
(``BOTH``). PDU routes point at the Ethernet and LIN senders; the PDU sink
feeds ``SignalDb``/application code.

Central gateway (Ethernet <-> zones)
------------------------------------

A 1400-byte Ethernet frame holds one PDU per zone. ``FrameGateway`` splits
it; each PDU routes to its zone's CAN FD channel via ``packAndTransmit``
after ``storePdu``.

Diagnostic gateway (DoIP -> CAN UDS)
------------------------------------

``DoIpHandler`` extracts the UDS payload from the DoIP datagram; the
integrator forwards it with ``TpGateway::segmentAndTransmit`` to the CAN
diagnostic id. Responses travel the reverse path through ``DiagLink``.

Rate-limited diagnostics
------------------------

A ``RATE_LIMIT`` policy (e.g. 10/s on the functional request id) drops
flooding while legitimate traffic passes; denials show up in
``GatewayStatistics::framesDropped`` and ``PolicyEngine::getDenyCount``.
