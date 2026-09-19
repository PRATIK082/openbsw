..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

canstack - signal oriented CAN communication
============================================

The ``canstack`` module is a layered CAN communication subsystem built on
plain C++: frame handling, hardware drivers, a multi channel manager, a
DBC derived signal database with packing/unpacking logic, a cycle time based
transmit scheduler, a PDU router for gateway scenarios and an application
facing API.

Configuration is code-first: signal and frame layouts live in C++ tables that
are generated from a DBC file (see :doc:`dbc_generation`) and compiled into the
application. No ARXML or post-build tooling is involved.

.. code-block:: text

   +--------------------+     +------------------------------------+
   | Application SWC    |----> CanInterface::sendSignal / readSignal
   +--------------------+     +------------------------------------+
                                     |                |
                              CanTxScheduler    CanRouter (gateway)
                                     |                |
                                SignalDatabase (DBC derived)
                                     |
                               CanChannel (one per bus)
                                     |
                        CanHwInterface (chip drivers)
                                     |
             +-----------+-----------+------------+---------------+
             |           |           |            |               |
          CanHwStub  CanHwMcp2515  CanHwSocketCan  CanHwTransceiverAdapter
          (tests/    (SPI, register (POSIX          (existing cpp2can
           sim)       level)        SocketCAN)      platform drivers)

.. toctree::
   :maxdepth: 1

   architecture
   integration
   dbc_generation
