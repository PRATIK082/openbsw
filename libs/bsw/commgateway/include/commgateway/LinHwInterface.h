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
 * \ingroup commgateway
 */
#pragma once

#include <cstdint>
#include <functional>

namespace commgateway
{

/**
 * Hardware abstraction for a LIN controller (master or slave).
 *
 * Mirrors the canstack::CanHwInterface pattern: one object per controller,
 * callbacks for rx/tx-done, polling via mainFunction(). Concrete drivers
 * (e.g. a UART based LIN driver in the BSP) implement this interface.
 *
 * Callback signatures: rx(pid, data, dlc), txDone(pid, success).
 */
class LinHwInterface
{
public:
    using RxCallback     = std::function<void(uint8_t, uint8_t*, uint8_t)>;
    using TxDoneCallback = std::function<void(uint8_t, bool)>;

    virtual ~LinHwInterface() = default;

    LinHwInterface(LinHwInterface const&)            = delete;
    LinHwInterface& operator=(LinHwInterface const&) = delete;

    virtual bool init(uint8_t channelId, uint32_t baudrate) = 0;
    virtual void shutdown()                                 = 0;
    virtual bool transmitFrame(uint8_t pid, uint8_t* data, uint8_t dlc) = 0;
    virtual void registerRxCallback(uint8_t pid, RxCallback cb)        = 0;
    virtual void registerTxDoneCallback(TxDoneCallback cb)             = 0;
    /// Master mode schedule tick; slave drivers may leave it empty.
    virtual void mainFunction() = 0;
    virtual void enterSleepMode() = 0;
    virtual void wakeup()         = 0;

protected:
    LinHwInterface() = default;
};

} // namespace commgateway
