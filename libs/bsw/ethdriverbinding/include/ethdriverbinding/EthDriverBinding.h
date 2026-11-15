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

#include "commgateway/EthIpdu.h"
#include "ethdriverbinding/IEthDriver.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ethdriverbinding
{

/// Binding between commgateway::EthTransportIf (EthChannel) and an Eth PHY driver.
/// When a driver is bound, send delegates to it; otherwise an in-memory loopback
/// (recording tx log, redelivery) is used.
class EthDriverBinding : public commgateway::EthTransportIf
{
public:
    using RxCallback = commgateway::EthTransportIf::RxCallback;

    struct Datagram
    {
        uint32_t destIp  = 0U;
        uint16_t destPort = 0U;
        uint16_t srcPort  = 0U;
        std::vector<uint8_t> payload;
    };

    EthDriverBinding() = default;
    explicit EthDriverBinding(IEthDriver* driver) : _ethDriver(driver) {}
    explicit EthDriverBinding(IEthDriver& driver) : _ethDriver(&driver) {}

    // ---- driver binding (FlashWearStore R1 style) ----
    void setEthDriver(IEthDriver* driver) { _ethDriver = driver; }
    void setEthDriver(IEthDriver& driver) { _ethDriver = &driver; }
    IEthDriver* getEthDriver() const { return _ethDriver; }
    bool hasEthDriver() const { return _ethDriver != nullptr; }

    void setDriver(IEthDriver* driver) { setEthDriver(driver); }
    void setDriver(IEthDriver& driver) { setEthDriver(&driver); }
    IEthDriver* getDriver() const { return getEthDriver(); }
    bool hasDriver() const { return hasEthDriver(); }

    void bind(IEthDriver* driver) { setEthDriver(driver); }
    void bind(IEthDriver& driver) { setEthDriver(&driver); }
    void unbind() { _ethDriver = nullptr; }

    // ---- EthTransportIf implementation ----
    bool send(uint32_t destIp, uint16_t destPort, uint16_t srcPort, uint8_t const* data, uint16_t length) override;
    void registerRxCallback(RxCallback cb) override;

    // ---- in-memory helpers (EthLoopbackChannel compatible) ----
    void injectRx(uint32_t srcIp, uint16_t srcPort, uint8_t const* data, uint16_t length);
    void setLoopback(bool enabled);
    bool getLoopback() const { return _loopback; }

    std::vector<Datagram> const& getTxLog() const { return _txLog; }
    void clearTxLog() { _txLog.clear(); }

    bool isInitialized() const { return _initialized; }
    void init() { _initialized = true; if (_ethDriver) (void)_ethDriver->init(); }
    void init(uint8_t) { init(); }
    void init(uint16_t) { init(); }
    void init(uint32_t) { init(); }
    void shutdown() { _initialized = false; if (_ethDriver) _ethDriver->shutdown(); }

    void mainFunction() { if (_ethDriver) _ethDriver->mainFunction(); }

private:
    IEthDriver* _ethDriver = nullptr;

    RxCallback _rxCallback;
    std::vector<Datagram> _txLog;
    bool _loopback    = true;
    bool _initialized = false;
};

// Compatibility aliases
using EthDriver            = EthDriverBinding;
using EthBinding          = EthDriverBinding;
using Binding             = EthDriverBinding;
using EthDriverBindingType = EthDriverBinding;
using EthHwBinding        = EthDriverBinding;
using EthPhyBinding       = EthDriverBinding;
using EthTransceiverBinding = EthDriverBinding;

} // namespace ethdriverbinding

