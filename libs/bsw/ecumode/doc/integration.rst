..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

integration guide (Phase 5 ``ecumode``)
=======================================

Filtering at the gateway call sites
------------------------------------

Query ``isFrameAllowed(frameId, channelType, isTx)`` at each
``FrameGateway`` / ``SignalGateway`` / ``GatewayStack`` RX/TX call site
and early-return when the frame is denied. ``isTx`` is ``false`` on the
receive path and ``true`` on the transmit path.

RX call site (drop denied frames)::

   if (!ecumode::CommStateManagerPhase5::getInstance().isFrameAllowed(frameId, channel, false)) return;

TX call site (block denied frames)::

   if (!ecumode::CommStateManagerPhase5::getInstance().isFrameAllowed(frameId, channel, true)) return;

Signal-gateway RX forwarder (same pattern)::

   if (!ecumode::CommStateManagerPhase5::getInstance().isFrameAllowed(frameId, channel, false)) return;

Mode requests are latched; they take effect on the next tick, so a mode
change never applies in the middle of a forwarding burst.

Lifecycle note
--------------

Pump the mode state machine from the existing stack tick::

   ecumode::CommStateManagerPhase5::getInstance().mainFunction(tick);

``mainFunction`` first runs the base ``commgateway`` channel state
machine, then applies any pending ``requestEcuMode`` mode, fires the
mode-change callback once, and dispatches the entering mode's
caller-registered action handlers. Load rules once at startup with
``loadConfig(jsonText, error)`` (see
``config/ecumode_rules.json``) and register action handlers for the
symbolic action names before entering modes that reference them.
