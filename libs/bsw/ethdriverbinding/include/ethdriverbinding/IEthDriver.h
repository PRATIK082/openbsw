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

#include <commgateway/EthIpdu.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ethdriverbinding
{

/// Abstraction for an Ethernet PHY / transport driver (real driver or mock).
class IEthDriver
{
public:
    using RxCallback = commgateway::EthTransportIf::RxCallback;

    virtual ~IEthDriver() = default;

    virtual bool init() = 0;
    virtual bool init(uint8_t) { return init(); }
    virtual bool init(uint16_t) { return init(); }
    virtual bool init(uint32_t) { return init(); }
    virtual void shutdown() = 0;
    virtual bool send(uint32_t destIp, uint16_t destPort, uint16_t srcPort, uint8_t const* data, uint16_t length) = 0;
    virtual void registerRxCallback(RxCallback cb) = 0;
    virtual void mainFunction() = 0;
    virtual bool isLinkUp() const { return true; }
    virtual void setLinkUp(bool /*up*/) {}

    /// Optional raw byte access.
    virtual size_t writeBytes(uint8_t const* /*data*/, size_t length) { return length; }
    virtual size_t readBytes(uint8_t* /*data*/, size_t /*length*/) { return 0U; }

protected:
    IEthDriver() = default;
    IEthDriver(IEthDriver const&)            = delete;
    IEthDriver& operator=(IEthDriver const&) = delete;
};

// Compatibility type aliases
using EthDriverInterface = IEthDriver;
using EthPhy             = IEthDriver;
using EthPhyIf           = IEthDriver;
using EthTransceiver     = IEthDriver;
using IEthPhy            = IEthDriver;

} // namespace ethdriverbinding

