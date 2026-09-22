..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

integration guide
=================

Wiring a channel
----------------

.. code-block:: cpp

   CanIoBridge bridge;
   bridge.bind(canChannel);  // upper-layer callback, no CanChannel changes

   // RouterBridge channel tables reference bridge.rxReader()/txWriter();
   // BridgeSupervisor::execute() calls router.run() then bridge.pumpTx().

Production blob configuration
-----------------------------

Do not write a new blob tool: extend
``executables/referenceApp/configuration/routing.jsonl`` (consumed by the
existing ``tools/blob/regenerate.sh``):

.. code-block:: json

   {"type": "channel", "value": {"name": "GW_CAN0", "id": 0, "type": "can", "configuration": {}}}
   {"type": "routing", "value": {"input": {"channel-name": "GW_CAN0", "message-id": 256, "offset": 0, "length": 8}, "outputs": [{"channel-name": "GW_CAN1", "message-id": 512, "offset": 0, "length": 8}]}}

Then regenerate the ``blob::`` headers; ``RouterBridge`` table structs map
1:1 to these entries for code-first (blob-free) bring-up.

Diagnostic / transport migration notes
--------------------------------------

* UDS: bind ``uds`` jobs to Phase 3 ``DiagLink::setUdsHandler``; full
  ``IncomingDiagConnection`` integration needs a ``transport`` message
  provider (future work, not a thin adapter).
* ISO-TP: Phase 3 ``TpGateway`` stays; bridging ``docan`` requires an
  ``IDoCanPhysicalTransceiver`` over ``CanChannel`` plus tick generator
  (future work).
* XCP: Phase 3 ``XcpServer`` stays (no OpenBSW XCP exists); transport via
  ``BridgeSupervisor`` channels.
