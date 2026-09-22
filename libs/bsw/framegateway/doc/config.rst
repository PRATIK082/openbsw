..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

gateway configuration
=====================

``GatewayStack::loadConfig()`` applies a ``frame_gateway_config.json``
document; see ``config/frame_gateway_config.json``. The parser is
self-contained (no JSON dependency).

Sections
--------

* ``frameConfigs`` - ``frameId``, ``channelType`` (0 = CAN, 1 = LIN,
  2 = ETH), ``channelId``, ``frameLength``, ``action``
  (``ROUTE_ONLY`` / ``EXTRACT_AND_SIGNAL`` / ``BOTH``), ``pdus`` with
  ``pduId`` / ``startByteOffset`` / ``length`` / optional
  ``isVariableLength`` + ``lengthFieldOffset``.
* ``diagnosticSessions`` - ``channelType`` / ``channelId`` /
  ``requestFrameId`` / ``responseFrameId`` / ``pduOffset`` /
  ``sessionTimeoutMs``.
* ``xcpSymbols`` - ``name`` / ``address`` / ``length`` / ``dataType``
  (``UINT8``..``FLOAT64``) / ``isCalibration`` / ``min`` / ``max`` /
  ``unit``. Generate from A2L with ``tools/gateway/a2l_to_xcp.py``.
* ``gatewayPolicies`` - ``frameId`` / ``channelType`` / ``channelId`` /
  ``action`` (``ALLOW`` / ``DENY`` / ``TRANSFORM`` / ``RATE_LIMIT`` /
  ``LOG_ONLY`` / ``REDIRECT``), ``filter`` (``"source == 0xNN"`` or
  ``"source != 0xNN"`` on ``data[0]``), ``maxRatePerSec``.

XCP symbols from A2L
--------------------

.. code-block:: text

   python tools/gateway/a2l_to_xcp.py ecu_calibration.a2l --output-dir src/config

Emits ``XcpSymbols.h`` / ``XcpSymbols.cpp`` (code-first table) plus an
``XcpSymbols.json`` fragment for the config file. The built-in parser covers
``CHARACTERISTIC`` / ``MEASUREMENT`` blocks (name, ``ECU_ADDRESS``,
datatype, ``UNIT``); ``min``/``max`` default to 0 and should be completed
from the A2L conversion tables during integration.
