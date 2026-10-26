/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <persistence/EepromStore.h>

namespace persistence
{

bool EepromStore::init(size_t size)
{
    if ((size == 0U) || (size > MAX_SIZE))
    {
        return false;
    }
    _store.clear();
    _store.resize(size, 0xFFU);
    _size = size;
    return true;
}

bool EepromStore::read(uint32_t address, uint8_t* data, size_t length) const
{
    if ((data == nullptr) || (length == 0U) || (_size == 0U))
    {
        return false;
    }
    if ((static_cast<size_t>(address) + length) > _size)
    {
        return false;
    }
    for (size_t i = 0U; i < length; ++i)
    {
        data[i] = _store[static_cast<size_t>(address) + i];
    }
    return true;
}

bool EepromStore::write(uint32_t address, uint8_t const* data, size_t length)
{
    if ((data == nullptr) || (length == 0U) || (_size == 0U))
    {
        return false;
    }
    if ((static_cast<size_t>(address) + length) > _size)
    {
        return false;
    }
    for (size_t i = 0U; i < length; ++i)
    {
        _store[static_cast<size_t>(address) + i] = data[i];
    }
    return true;
}

} // namespace persistence
