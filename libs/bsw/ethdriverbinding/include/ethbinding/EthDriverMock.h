/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#pragma once

#include "ethdriverbinding/IEthDriver.h"

#include <gmock/gmock.h>

namespace ethdriverbinding
{

class EthDriverMock : public IEthDriver
{
public:
    MOCK_METHOD(bool, init, (), (override));
    MOCK_METHOD(void, shutdown, (), (override));
    MOCK_METHOD(bool, send, (uint32_t, uint16_t, uint16_t, uint8_t const*, uint16_t), (override));
    MOCK_METHOD(void, registerRxCallback, (RxCallback), (override));
    MOCK_METHOD(void, mainFunction, (), (override));
    MOCK_METHOD(bool, isLinkUp, (), (const, override));
    MOCK_METHOD(void, setLinkUp, (bool), (override));
    MOCK_METHOD(size_t, writeBytes, (uint8_t const*, size_t), (override));
    MOCK_METHOD(size_t, readBytes, (uint8_t*, size_t), (override));
};

// Compatibility type aliases
using MockEthDriver    = EthDriverMock;
using EthDriverMockType = EthDriverMock;

} // namespace ethdriverbinding

namespace ethdriver = ethdriverbinding;
namespace ethbinding = ethdriverbinding;
namespace eth       = ethdriverbinding;