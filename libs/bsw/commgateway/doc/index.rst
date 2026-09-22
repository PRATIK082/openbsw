..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

commgateway - multi-protocol gateway stack (Phase 2)
====================================================

The ``commgateway`` module extends the :doc:`canstack <../../canstack/doc/index>`
CAN stack with LIN, Ethernet and protocol-agnostic gateway features. The
existing CAN modules are used through their public API only and are not
modified.

.. code-block:: text

   +------------------+     +--------------------------------------+
   | Application SWC  |----> CommStack (LifecycleComponent, 1 ms)
   +------------------+     +--------------------------------------+
                                |          |             |
                          CanInterface  LinChannel  EthIpduManager
                          (existing)    (master/     (UDP/TCP via
                                         slave)      EthTransportIf)
                                |          |             |
                                +----+-----+------+------+
                                     |            |
                               SignalGateway  TimeoutMonitor
                               (signal         (rx timeouts)
                                routing)
                                     |
                        TxConfirmationMgr + CommStateManager
                        (tx acks)         (ACTIVE/PASSIVE/
                                           BUS_OFF/COMM_FAILURE)
                                     |
                              InterruptRouter
                              (CAN/LIN/ETH ISR fan-out)

All time bases are injected millisecond ticks (wrap-safe ``now - last``
comparisons), so every module is OS-free and unit testable.

.. toctree::
   :maxdepth: 1

   architecture
   gateway
