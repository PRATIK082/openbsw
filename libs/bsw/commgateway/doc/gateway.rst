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

``CommStack::loadGatewayRules()`` (via ``GatewayConfigParser``) applies a
``gateway_rules.json`` document; see ``config/gateway_rules.json`` for a full
example. The parser is self-contained (no JSON dependency).

Sections
--------

* ``signalRouting`` - ``signalName``, ``destinations`` (``channelType``:
  0 = CAN, 1 = LIN, 2 = ETH; ``channelId``), optional ``transform``
  (``scale``/``offset``), ``minIntervalMs`` rate limit and ``timeoutMs``
  (signal timeout supervision).
* ``frameTimeouts`` - ``channelType``/``channelId``/``frameId``/``timeoutMs``.
* ``channelStates`` - registers the channel and sets ``autoRecovery`` /
  ``recoveryTimeMs``.

Example
-------

.. code-block:: json

   {
     "signalRouting": [
       {
         "signalName": "VehicleSpeed",
         "destinations": [
           {"channelType": 1, "channelId": 0},
           {"channelType": 2, "channelId": 0}
         ],
         "timeoutMs": 500,
         "minIntervalMs": 100
       }
     ],
     "frameTimeouts": [
       {"channelType": 0, "channelId": 0, "frameId": 256, "timeoutMs": 200}
     ],
     "channelStates": [
       {"channelType": 0, "channelId": 0, "autoRecovery": true, "recoveryTimeMs": 1000}
     ]
   }

LIN signal mapping is done with ``LinChannel::scheduleFrame()`` (``pid`` +
``associatedSignal``); Ethernet PDU mapping with
``CommStack::registerEthSignalMapping()``.
