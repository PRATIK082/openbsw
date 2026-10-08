persistence
===========

Phase 6: block-based persistence manager with RAM mirror, CRC protection,
immediate/deferred writes, redundant storage, ``FlashWearStore`` wear leveling
and ``EepromStore`` EEPROM emulation. Call ``shutdown()`` before Phase-5 SLEEP
to flush deferred blocks.
