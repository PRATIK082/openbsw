..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

framegateway - multi-protocol frame gateway (Phase 3)
=====================================================

The ``framegateway`` module adds frame/PDU-level routing, ISO-TP transport,
diagnostics (UDS/DoIP) and XCP calibration/measurement on top of the
:doc:`canstack <../../canstack/doc/index>` and
:doc:`commgateway <../../commgateway/doc/index>` stacks. Phase 1 and 2
modules are used through their public APIs only and are not modified.

.. code-block:: text

   +------------------+     +--------------------------------------+
   | Lower layers     |----> GatewayStack (LifecycleComponent, 1 ms)
   | (Can/Lin/Eth     |     +--------------------------------------+
   |  upper callbacks)|         |        |         |          |
   +------------------+   FrameGateway TpGateway DiagLink  XcpServer
                          + PolicyEngine  (ISO-TP)  (UDS/DoIP)
                                |
                          PduAssembler (multi-PDU pack/unpack)

Integration without touching Phase 1/2: register ``GatewayStack`` (or
``FrameGateway``) callbacks as upper-layer receivers, e.g.
``canChannel.registerUpperLayerCallback(...)`` forwards to
``FrameGateway::onFrameReceived(0, channelId, frameId, dlc, data)``.
Existing ``docan``/``uds``/``routing`` modules bind via the injected
``UdsHandler``/sender callbacks.

.. toctree::
   :maxdepth: 1

   architecture
   config
   use_cases
