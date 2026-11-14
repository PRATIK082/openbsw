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

#include "lindriverbinding/ILinDriver.h"

#include <gmock/gmock.h>

namespace lindriverbinding
{

class LinDriverMock : public ILinDriver
{
public:
    MOCK_METHOD(bool, init, (uint8_t, uint32_t), (override));
    MOCK_METHOD(void, shutdown, (), (override));
    MOCK_METHOD(bool, transmitFrame, (uint8_t, uint8_t*, uint8_t), (override));
    MOCK_METHOD(void, registerRxCallback, (uint8_t, RxCallback), (override));
    MOCK_METHOD(void, registerTxDoneCallback, (TxDoneCallback), (override));
    MOCK_METHOD(void, mainFunction, (), (override));
    MOCK_METHOD(void, enterSleepMode, (), (override));
    MOCK_METHOD(void, wakeup, (), (override));
    MOCK_METHOD(size_t, writeBytes, (uint8_t const*, size_t), (override));
    MOCK_METHOD(size_t, readBytes, (uint8_t*, size_t), (override));
};

// Compatibility type aliases
using MockLinDriver    = LinDriverMock;
using LinDriverMockType = LinDriverMock;

} // namespace lindriverbinding

