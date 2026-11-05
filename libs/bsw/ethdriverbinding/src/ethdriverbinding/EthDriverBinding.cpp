/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "ethdriverbinding/EthDriverBinding.h"

namespace ethdriverbinding
{

bool EthDriverBinding::send(
    uint32_t destIp, uint16_t destPort, uint16_t srcPort, uint8_t const* data, uint16_t length)
{
    if (data == nullptr && length > 0U)
    {
        return false;
    }
    Datagram datagram{};
    datagram.destIp   = destIp;
    datagram.destPort = destPort;
    datagram.srcPort  = srcPort;
    if (length > 0U)
    {
        datagram.payload.assign(data, data + length);
    }
    _txLog.push_back(datagram);

    if (_ethDriver != nullptr)
    {
        return _ethDriver->send(destIp, destPort, srcPort, data, length);
    }

    if (_loopback && _rxCallback)
    {
        _rxCallback(destIp, srcPort, datagram.payload.data(),
                    static_cast<uint16_t>(datagram.payload.size()));
    }
    return true;
}

void EthDriverBinding::registerRxCallback(RxCallback cb)
{
    _rxCallback = std::move(cb);
    if (_ethDriver != nullptr)
    {
        _ethDriver->registerRxCallback(_rxCallback);
    }
}

void EthDriverBinding::injectRx(uint32_t srcIp, uint16_t srcPort, uint8_t const* data, uint16_t length)
{
    if (_rxCallback && (data != nullptr || length == 0U))
    {
        _rxCallback(srcIp, srcPort, data, length);
    }
}

void EthDriverBinding::setLoopback(bool enabled)
{
    _loopback = enabled;
}

} // namespace ethdriverbinding