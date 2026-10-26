..
   *******************************************************************************
   Copyright (c) 2026 Accenture

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

integrity
=========

Phase 8: message integrity protection library (profiles P01/P02/P04/P05/P11)
with CRC, sequence counter and data ID. Use ``protect()`` on TX and ``check()``
on RX; wire into FrameGateway per frame-to-profile map.
