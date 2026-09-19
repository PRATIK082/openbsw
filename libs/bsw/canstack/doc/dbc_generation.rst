..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

DBC to code generation
======================

``tools/can/dbc_to_openbsw.py`` extracts frames and signals from a Vector DBC
file and generates the code-first configuration tables of the ``canstack``
module.

Usage
-----

.. code-block:: text

   python tools/can/dbc_to_openbsw.py vehicle.dbc --channel-id 0 --output-dir src/config

Options:

``--channel-id``
  CAN channel id stored in the generated entries (DBC files carry no
  channel information; default 0).

``--output-dir``
  Directory for the generated files (default: current directory).

``--prefix``
  Table name prefix (default ``can``): ``g_canFrameTable``,
  ``g_canSignalTable``.

Outputs
-------

``<prefix>Tables.h``
  Extern declarations of the generated tables.

``FrameConfig_table.cpp``
  Frame table: id, name, dlc and channel of every ``BO_`` message.

``SignalConfig_table.cpp``
  Signal table: name, frame id, start bit, length, byte order, factor,
  offset, range, unit, multiplexing and signedness of every ``SG_`` signal.

The generated .cpp files are compiled into the OpenBSW build and passed to
the stack via ``CanStackConfig`` (see :doc:`integration`). The entries are
plain aggregates; the tables are ``extern const`` arrays, which is portable
across C++ standards (the entries themselves can also be used directly in
``constexpr`` tables).

Parser
------

The script uses `cantools <https://github.com/cantools/cantools>`_ when
installed and falls back to a small built-in parser covering the common
subset otherwise:

* ``BO_ <id> <name>: <dlc> <sender>`` (ids above 29 bits are masked).
* ``SG_ <name> [M|m<N>] : <start>|<len>@<endian><sign> (<f>,<o>)
  [<min>|<max>] "<unit>" <receivers>``.
* Multiplexing: ``M`` marks the multiplexer switch, ``m<N>`` a multiplexed
  signal; both are preserved in the generated tables and honored by the
  scheduler, the receive path and the router.

Example
-------

Given::

   BO_ 256 EngineData: 8 ECM
    SG_ EngineSpeed M : 7|16@1+ (0.25,0) [0|8000] "rpm" BCM
    SG_ OilTemp m0 : 23|8@1+ (1,-40) [-40|215] "degC" BCM

the generator emits::

   canstack::SignalTableEntry const g_canSignalTable[] = {
       {"EngineSpeed", 0x100U, 7U, 16U, true, 0.25, 0.0, 0.0, 8000.0, "rpm", 0U, true, -1, false},
       {"OilTemp", 0x100U, 23U, 8U, true, 1.0, -40.0, -40.0, 215.0, "degC", 0U, false, 0, false},
   };

Runtime alternative
-------------------

On platforms with a file system, ``SignalDatabase::loadFromDbc(path,
channelId)`` parses the same subset at runtime. Both paths produce identical
database content.
