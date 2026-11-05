/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "lindriverbinding/LinDriverBinding.h"

namespace lindriverbinding
{

bool LinDriverBinding::init(uint8_t channelId, uint32_t baudrate)
{
    _channelId = channelId;
    _baudrate  = baudrate;
    _initialized = true;
    if (_linDriver != nullptr)
    {
        return _linDriver->init(channelId, baudrate);
    }
    return true;
}

void LinDriverBinding::shutdown()
{
    _initialized = false;
    if (_linDriver != nullptr)
    {
        _linDriver->shutdown();
    }
}

bool LinDriverBinding::transmitFrame(uint8_t pid, uint8_t* data, uint8_t dlc)
{
    TxFrame frame{};
    frame.pid = pid;
    frame.dlc = dlc;
    if ((data != nullptr) && (dlc > 0U))
    {
        frame.data.assign(data, data + dlc);
    }
    _txLog.push_back(frame);

    if (_linDriver != nullptr)
    {
        return _linDriver->transmitFrame(pid, data, dlc);
    }

    if (_txDone)
    {
        _txDone(pid, true);
    }
    if (_loopback)
    {
        auto it = _rxCallbacks.find(pid);
        if (it != _rxCallbacks.end() && it->second)
        {
            it->second(pid, data, dlc);
        }
    }
    return true;
}

void LinDriverBinding::registerRxCallback(uint8_t pid, RxCallback cb)
{
    _rxCallbacks[pid] = std::move(cb);
    if (_linDriver != nullptr)
    {
        _linDriver->registerRxCallback(pid, _rxCallbacks[pid]);
    }
}

void LinDriverBinding::registerTxDoneCallback(TxDoneCallback cb)
{
    _txDone = std::move(cb);
    if (_linDriver != nullptr)
    {
        _linDriver->registerTxDoneCallback(_txDone);
    }
}

void LinDriverBinding::mainFunction()
{
    if (_linDriver != nullptr)
    {
        _linDriver->mainFunction();
    }
}

void LinDriverBinding::enterSleepMode()
{
    _sleeping = true;
    if (_linDriver != nullptr)
    {
        _linDriver->enterSleepMode();
    }
}

void LinDriverBinding::wakeup()
{
    _sleeping = false;
    if (_linDriver != nullptr)
    {
        _linDriver->wakeup();
    }
}

void LinDriverBinding::injectRx(uint8_t pid, uint8_t* data, uint8_t dlc)
{
    auto it = _rxCallbacks.find(pid);
    if (it != _rxCallbacks.end() && it->second)
    {
        it->second(pid, data, dlc);
    }
}

} // namespace lindriverbinding