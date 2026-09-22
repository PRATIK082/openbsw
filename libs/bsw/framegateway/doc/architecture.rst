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

* ``PduAssembler`` - stateless offset-based multi-PDU pack/unpack, variable
  lengths, padding. Unpacked views alias the frame buffer.
* ``FrameGateway`` - frame table keyed by (channelType, channelId, frameId);
  policy check, PDU extraction (``ROUTE_ONLY`` / ``EXTRACT_AND_SIGNAL`` /
  ``BOTH``), frame-level PDU routes, TX-side ``storePdu`` +
  ``packAndTransmit``, oversize-PDU handoff to the transport protocol.
* ``PolicyEngine`` - first-match rules: ``ALLOW`` / ``DENY`` / ``TRANSFORM``
  / ``RATE_LIMIT`` (1 s windows) / ``LOG_ONLY`` / ``REDIRECT``.
* ``TpGateway`` - ISO-TP normal addressing: SingleFrame (incl. CAN FD
  escape), FirstFrame (12-bit length), ConsecutiveFrames (sequence checked),
  FlowControl (CTS/WAIT/OVERFLOW). TX sessions pause for CTS; block size is
  accepted but frames go back-to-back (STmin parsed, not delayed).
* ``DiagLink`` - diagnostic sessions keyed by requestFrameId (the PDU id
  carrying diagnostics must equal it); injected ``UdsHandler`` (bind to the
  ``uds`` module), session timeouts.
* ``DoIpHandler`` - ISO 13400 header codec, routing activation, UDS-message
  (0x8001) dispatch over an injected datagram sender.
* ``XcpSymbolTable`` / ``XcpServer`` - symbol lookup, MTA-based
  UPLOAD/DOWNLOAD/BUILD_CHECKSUM over registered memory regions
  (bounds-checked; symbol writes honor ``isCalibration``), cyclic and
  event-triggered DAQ lists emitting DTOs via the transport handler.
  Response format is a simplified CTO (PID 0xFF + return code).
* ``GatewayStack`` - lifecycle orchestrator wiring all of the above plus
  ``GatewayStatistics``.

Time bases are injected millisecond ticks (wrap-safe), keeping every module
OS-free and unit testable.
