..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

reuse inventory (Phase 5 ``ecumode``)
====================================

What is reused
--------------

* Phase 2 ``commgateway::CommStateManager`` base class
  (``libs/bsw/commgateway/include/commgateway/CommStateManager.h``):
  ``CommStateManagerPhase5`` extends it for the channel state machine and
  hides (does not override) its non-virtual ``mainFunction(uint32_t)``,
  running the base channel logic before applying the pending ECU mode.
* C++ standard library only (``<cstdint>``, ``<functional>``, ``<map>``,
  ``<string>``, ``<utility>``, ``<vector>``) plus the self-contained JSON
  reader inside ``CommStateManagerPhase5.cpp``. No third-party JSON
  dependency was added.
* The shipped rule set ``libs/bsw/ecumode/config/ecumode_rules.json``
  (defaults plus per-frame exceptions and symbolic action names).

Deliberately NOT used (spec-style APIs verified absent)
-------------------------------------------------------

* ``blob::BlobLoader`` — configuration is parsed from a plain
  ``std::string`` via ``loadConfig(jsonText, error)``; no blob loading.
* ``SignalGateway`` / ``XcpServer`` singletons — the manager never calls
  other BSW singletons directly. Mode actions dispatch only to
  caller-registered handlers (``registerActionHandler`` /
  ``setModeActions``); unregistered action names are skipped.
* ``BspSystem::enterLowPowerMode`` — entering SLEEP only changes filter
  policy state; power-mode transitions are left to the integrator's
  registered action handlers.
* ``LifecycleManager`` rate API — there is no lifecycle-rate dependency;
  the integrator drives timing by calling ``mainFunction(tick)`` from the
  stack tick (see ``integration.rst``).
