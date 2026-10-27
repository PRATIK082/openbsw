..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

persistence
===========

Phase 6: block-based persistence manager with RAM mirror, CRC protection,
immediate/deferred writes, redundant storage, ``FlashWearStore`` wear leveling
and ``EepromStore`` EEPROM emulation. Call ``shutdown()`` before Phase-5 SLEEP
to flush deferred blocks.
