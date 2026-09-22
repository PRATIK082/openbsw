..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

gwbridge - reuse-first gateway bridges (Phase 4)
================================================

The ``gwbridge`` module connects Phase 1-3 channels to the EXISTING
``routing`` module with thin adapters. No routing, transport, diagnostic or
hardware logic is duplicated; see :doc:`reuse` for the verified inventory.

.. code-block:: text

   CanChannel -- CanIoBridge --\
   LinChannel -- LinIoBridge ----> routing::Router (+ Rx/Tx adapters)
   EthIpdu    -- EthIoBridge ---/         (existing OpenBSW module)

   BridgeSupervisor: 1 ms lifecycle pump (router.run + channel TX drains).

Each bridge speaks the routing message format (8-byte big-endian id/length
header + payload) over ``io::MemoryQueue`` endpoints.

.. toctree::
   :maxdepth: 1

   reuse
   integration
