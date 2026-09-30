..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

ecumode - ECU mode-based communication control (Phase 5)
========================================================

The ``ecumode`` module extends ``commgateway`` with an ECU-wide mode
state machine (NORMAL, STANDBY, SLEEP, DIAGNOSTIC, TRANSPORT,
MANUFACTURING). Each mode has a default filter policy plus optional
per-frame exceptions, applied to Rx/Tx filtering decisions. Mode
requests latch as pending and take effect in ``mainFunction()``, firing a callback and
dispatching caller-registered action handlers; rules live in ``config/ecumode_rules.json``.
