..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

Architecture
============

Module layout
-------------

All headers live under ``canstack/``:

===================== =======================================================
File                  Purpose
===================== =======================================================
``CanFrame.h``        Raw frame value object (id, dlc, payload).
``CanHwInterface.h``  Hardware driver interface (init/transmit/rx poll).
``CanHwStub.h``       In-memory driver for tests and simulation (loopback).
``CanHwMcp2515.h``     Register level MCP2515 driver via an injected SPI bus.
``CanHwSocketCan.h``  Linux SocketCAN driver (``CANSTACK_WITH_SOCKETCAN``).
``CanHwTransceiverAdapter.h`` Adapter around a cpp2can ``ICanTransceiver``.
``CanChannel.h``      Multi channel manager binding a driver to upper layers.
``SignalDb.h``        DBC derived signal database, pack/unpack, tables.
``CanRouter.h``       PDU routing across channels (gateway core).
``CanTxScheduler.h``   Cycle time based transmission scheduler.
``CanInterface.h``    Application facing API of the whole stack.
``CanSubsystem.h``   Lifecycle integration (``LifecycleComponent``).
===================== =======================================================

Hardware abstraction
--------------------

``CanHwInterface`` is the only chip touching abstraction. Mailbox parameters
are driver defined: ``transmit`` selects a hardware transmit buffer,
``registerRxCallback`` subscribes to the frames a receive buffer accepts.

Like a real controller, a received frame is delivered exactly once: the
MCP2515 driver dispatches from the buffer that received it, the stub and
the SocketCAN driver deliver to the callback of the lowest registered
mailbox. Channel level fan-out is done above the drivers.

The concrete drivers cover three integration paths:

* **Simulation / tests**: ``CanHwStub`` keeps frames in memory and can feed
  transmitted frames back (loopback).
* **Direct register access**: ``CanHwMcp2515`` implements the SPI instruction
  set and register map of the Microchip MCP2515. The transport is injected
  (``IMcp2515Bus``), so the driver itself stays platform independent and is
  unit tested against a register level fake.
* **Existing OpenBSW transceivers**: ``CanHwTransceiverAdapter`` wraps any
  cpp2can ``can::ICanTransceiver``. This is the integration path for chips
  with platform transceivers in OpenBSW (S32K1xx FlexCAN, STM32 bxCAN/FDCAN,
  POSIX SocketCAN transceiver), covering e.g. the S32K144.
* **POSIX SocketCAN**: ``CanHwSocketCan`` binds a ``CAN_RAW`` socket directly
  (compiled with ``CANSTACK_WITH_SOCKETCAN``, enabled for the POSIX
  reference app build).

Receive path
------------

Polling drivers dispatch from ``mainFunction()`` (called via the channel
manager from ``CanInterface::mainFunctionRx()``); interrupt driven
transceivers dispatch from their own context. Frames are forwarded through
the channel manager with the channel id attached, enter the router (gateway
rules) and the bounded receive queue, and are unpacked by
``mainFunctionRx()``:

.. code-block:: text

   driver rx -> CanChannel (channelId) -> CanRouter.onFrameReceived()
                                        -> rx queue -> unpackSignal()
                                                   -> value cache + callbacks

Multiplexed frames are honored: guarded signals only update the cache and
fire callbacks when the multiplexer switch value selects them.

Transmit path
-------------

``CanTxScheduler`` registers cyclic signals (``TxSignalEntry``) and packs a
frame whenever ``(now - lastTxTime) >= cycleTimeMs``. Values come from an
injected value provider or from ``CanInterface::sendSignal()`` (the value
provider wins). All registered signals of a due frame are packed into one
zeroed buffer, so a frame is transmitted as a whole.

The stack's main functions run with a 1 ms period from the lifecycle run
path (see :doc:`integration`).

Signal database
---------------

``SignalDatabase`` stores ``FrameConfig``/``SignalConfig`` per
``(channelId, frameId)`` and implements DBC bit numbering for both byte
orders:

* Intel (``@0``): linear bit positions, LSB first.
* Motorola (``@1``): DBC sawtooth walk starting at the MSB.
* Signed signals (``@1-``/``@0-``) are sign extended on unpack and encoded
  as two's complement on pack.
* Values are clamped to ``[min, max]``; the raw value is
  ``round((value - offset) / factor)``.

The database is filled either from code-first tables
(``loadFrameTable()``/``loadSignalTable()``) or at runtime from a DBC file
(``loadFromDbc()``).

Routing
-------

``CanRouter`` rules map an input ``(channel, frameId)`` to one or more output
destinations:

* **Frame forwarding** (same frame id, no transform): the payload is copied
  unmodified.
* **Signal gateway** (different frame id or a transform present): the input
  frame is unpacked; every signal that exists in the output frame by name is
  packed into the output frame. A ``transform`` lambda may scale/offset the
  value; returning NaN drops the signal from the routed frame.
