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

#include <commgateway/LinHwInterface.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace lindriverbinding
{

/// Abstraction for a UART/LIN transceiver (real driver or mock).
class ILinDriver
{
public:
    using RxCallback     = commgateway::LinHwInterface::RxCallback;
    using TxDoneCallback = commgateway::LinHwInterface::TxDoneCallback;

    virtual ~ILinDriver() = default;

    virtual bool init(uint8_t channelId, uint32_t baudrate)             = 0;
    virtual bool init() { return init(0U, 0U); }
    virtual bool init(uint8_t channelId) { return init(channelId, 0U); }
    virtual void shutdown()                                             = 0;
    virtual bool transmitFrame(uint8_t pid, uint8_t* data, uint8_t dlc) = 0;
    virtual void registerRxCallback(uint8_t pid, RxCallback cb)         = 0;
    virtual void registerTxDoneCallback(TxDoneCallback cb)              = 0;
    virtual void mainFunction()                                         = 0;
    virtual void enterSleepMode()                                       = 0;
    virtual void wakeup()                                               = 0;

    /// Optional UART-level byte access (default no-op).
    virtual size_t writeBytes(uint8_t const* /*data*/, size_t length) { return length; }
    virtual size_t readBytes(uint8_t* /*data*/, size_t /*length*/) { return 0U; }

protected:
    ILinDriver() = default;
    ILinDriver(ILinDriver const&)            = delete;
    ILinDriver& operator=(ILinDriver const&) = delete;
};

// Compatibility type aliases
using LinDriverInterface = ILinDriver;
using LinTransceiver     = ILinDriver;
using LinTransceiverIf   = ILinDriver;
using ITransceiver       = ILinDriver;
using LinHwDriver        = ILinDriver;

} // namespace lindriverbinding

// Compatibility aliases
