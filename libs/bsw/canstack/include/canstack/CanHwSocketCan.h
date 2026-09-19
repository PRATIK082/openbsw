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
 * \ingroup canstack
 */
#pragma once

#include "canstack/CanHwInterface.h"

#include <cstdint>
#include <string>
#include <vector>

namespace canstack
{

#ifdef CANSTACK_WITH_SOCKETCAN

/**
 * Linux SocketCAN driver.
 *
 * The driver opens a CAN_RAW socket on the interface given in the constructor
 * (e.g. "can0" or a virtual "vcan0"). Frames are received from mainFunction(),
 * so the driver behaves like a polling driver. Every registered receive
 * callback receives every frame (the kernel may apply interface filters set
 * outside of this driver).
 *
 * Only compiled when CANSTACK_WITH_SOCKETCAN is defined (POSIX builds).
 */
class CanHwSocketCan final : public CanHwInterface
{
public:
    /**
     * \param interfaceName network interface to bind the socket to
     */
    explicit CanHwSocketCan(std::string interfaceName = "can0");

    ~CanHwSocketCan() override;

    bool init(uint8_t channelId, uint32_t baudrate) override;
    void shutdown() override;
    bool transmit(uint8_t mailboxId, uint32_t frameId, uint8_t dlc, uint8_t const* data) override;
    void registerRxCallback(uint8_t mailboxId, RxCallback cb) override;
    void mainFunction() override;

    bool isInitialized() const;

private:
    void dispatchReceived(uint32_t frameId, uint8_t dlc, uint8_t const* data);

    std::string const m_interfaceName;
    int m_socketFd;
    bool m_initialized = false;
    uint8_t m_channelId = 0U;
    uint32_t m_baudrate = 0U;
    std::vector<RxCallback> m_rxCallbacks;
};

#endif // CANSTACK_WITH_SOCKETCAN

} // namespace canstack
