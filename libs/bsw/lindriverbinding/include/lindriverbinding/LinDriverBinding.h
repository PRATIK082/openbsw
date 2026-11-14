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

#include "commgateway/LinHwInterface.h"
#include "lindriverbinding/ILinDriver.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace lindriverbinding
{

/// Binding between commgateway::LinHwInterface and a UART/LIN transceiver driver.
/// When a driver is bound, all operations delegate to it; otherwise an in-memory
/// simulation is used (suitable for POSIX tests).
class LinDriverBinding : public commgateway::LinHwInterface
{
public:
    struct TxFrame
    {
        uint8_t pid = 0U;
        uint8_t dlc = 0U;
        std::vector<uint8_t> data;
    };

    LinDriverBinding() = default;
    explicit LinDriverBinding(ILinDriver* driver) : _linDriver(driver) {}
    explicit LinDriverBinding(ILinDriver& driver) : _linDriver(&driver) {}

    // ---- driver binding (FlashWearStore R1 style) ----
    void setLinDriver(ILinDriver* driver) { _linDriver = driver; }
    void setLinDriver(ILinDriver& driver) { _linDriver = &driver; }
    ILinDriver* getLinDriver() const { return _linDriver; }
    bool hasLinDriver() const { return _linDriver != nullptr; }

    void setDriver(ILinDriver* driver) { setLinDriver(driver); }
    void setDriver(ILinDriver& driver) { setLinDriver(&driver); }
    ILinDriver* getDriver() const { return getLinDriver(); }
    bool hasDriver() const { return hasLinDriver(); }

    void bind(ILinDriver* driver) { setLinDriver(driver); }
    void bind(ILinDriver& driver) { setLinDriver(&driver); }
    void unbind() { _linDriver = nullptr; }

    // ---- LinHwInterface implementation ----
    bool init(uint8_t channelId, uint32_t baudrate) override;
    bool init() { return LinDriverBinding::init(0U, 0U); }
    bool init(uint8_t channelId) { return LinDriverBinding::init(channelId, 0U); }
    void shutdown() override;
    bool transmitFrame(uint8_t pid, uint8_t* data, uint8_t dlc) override;
    void registerRxCallback(uint8_t pid, RxCallback cb) override;
    void registerTxDoneCallback(TxDoneCallback cb) override;
    void mainFunction() override;
    void enterSleepMode() override;
    void wakeup() override;

    // ---- in-memory helpers ----
    std::vector<TxFrame> const& getTxLog() const { return _txLog; }
    void clearTxLog() { _txLog.clear(); }

    void injectRx(uint8_t pid, uint8_t* data, uint8_t dlc);

    uint8_t getChannelId() const { return _channelId; }
    bool isInitialized() const { return _initialized; }
    bool isSleeping() const { return _sleeping; }

    void setLoopback(bool enabled) { _loopback = enabled; }
    bool getLoopback() const { return _loopback; }

    std::map<uint8_t, RxCallback> const& getRxCallbacks() const { return _rxCallbacks; }

private:
    ILinDriver* _linDriver = nullptr;

    uint8_t _channelId   = 0U;
    uint32_t _baudrate   = 0U;
    bool _initialized    = false;
    bool _sleeping       = false;
    bool _loopback       = true;

    std::map<uint8_t, RxCallback> _rxCallbacks;
    TxDoneCallback _txDone;
    std::vector<TxFrame> _txLog;
};

// Compatibility aliases
using LinDriver            = LinDriverBinding;
using LinBinding          = LinDriverBinding;
using Binding             = LinDriverBinding;
using LinDriverBindingType = LinDriverBinding;
using LinHwBinding        = LinDriverBinding;
using LinHwDriverBinding  = LinDriverBinding;
using LinTransceiverBinding = LinDriverBinding;

} // namespace lindriverbinding

