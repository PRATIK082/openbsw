..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

reuse inventory (verified against source)
=========================================

Reused directly (with evidence):

* ``routing::Router<MAX>`` - ``libs/bsw/routing/include/routing/Router.h``:
  ``init(table, readers, writers)`` + ``run()``.
* ``routing::LegacyRxAdapter`` / ``LegacyTxAdapter`` - same directory;
  multi-PDU split and 0xFF-padded frame rebuild.
* ``routing::PduRoutingTable`` / ``RxAdapterTable`` / ``TxAdapterTable`` -
  built code-first by ``RouterBridge`` (same fields the blob loader fills).
* ``routing::route()`` / ``outputPdu()`` - ``routing/pduRouting.h``.
* ``routing::ErrorHandler`` - delegate hook, counted by ``RouterBridge``.
* ``io::IReader`` / ``io::IWriter`` - ``libs/bsw/io/include/io/``.
* ``io::MemoryQueue`` + ``MemoryQueueReader`` / ``MemoryQueueWriter`` -
  ``libs/bsw/io/include/io/MemoryQueue.h``.
* ``blob::Blob`` / ``blob::config()`` binary format - ``libs/bsw/blob``;
  production tables come from ``tools/blob/`` (JSONL input), not from a new
  tool.
* ``etl`` containers/spans/big-endian types - ``libs/3rdparty/etl``.

Phase 4 spec claims NOT followed (verified absent or different):

* ``routing::IReader`` / ``IWriter`` - real namespace is ``io``.
* ``Router::addChannel`` / ``onFrameReceived`` / ``routePdu`` - do not
  exist; the pattern is readers/writers + ``run()`` (see
  ``routing/Integration.h``).
* ``transport::TpRouter`` / ``IsoTpLayer`` - do not exist; the real
  ``transport::AbstractTransportLayer`` framework is heavier than the
  tested Phase 3 ``TpGateway``, which is kept as-is.
* ``can::CanController`` / ``lin::LinController`` - no ``libs/bsw/can`` or
  ``libs/bsw/lin`` modules exist; bridges bind Phase 1/2 channels directly.
* ``common::RingBuffer`` - does not exist; ``io::MemoryQueue`` is used.
* ``docan::DoCanSystem`` / ``uds::UdsSystem`` singletons - do not exist;
  the real ``docan``/``uds`` frameworks need transceiver, tick and job
  wiring that is not "thin". Phase 3 ``DiagLink`` (injected handler) is
  kept; bridge points are documented in :doc:`integration`.
* ``blob::BlobLoader`` / ``async::AsyncExecutor`` - do not exist.
* A new ``config_to_blob.py`` - duplicates ``tools/blob/``; not created.
