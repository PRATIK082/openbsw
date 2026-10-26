/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * \file
 * \ingroup persistence
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <etl/span.h>
#include <etl/vector.h>

namespace persistence
{

/// In-RAM EEPROM emulation used when no hardware EEPROM driver is bound.
/// Models byte-addressable storage with a backing vector.
class EepromStore
{
public:
    static constexpr size_t MAX_SIZE = 32U * 1024U;

    EepromStore() = default;

    bool init(size_t size);
    bool read(uint32_t address, uint8_t* data, size_t length) const;
    bool write(uint32_t address, uint8_t const* data, size_t length);
    size_t size() const { return _size; }

private:
    etl::vector<uint8_t, MAX_SIZE> _store{};
    size_t _size{0U};
};

} // namespace persistence
