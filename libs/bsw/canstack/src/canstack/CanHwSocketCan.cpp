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
#include "canstack/CanHwSocketCan.h"
#include "canstack/CanStackLogger.h"

#ifdef CANSTACK_WITH_SOCKETCAN

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace canstack
{
namespace logger = ::util::logger;

CanHwSocketCan::CanHwSocketCan(std::string interfaceName)
: m_interfaceName(std::move(interfaceName)), m_socketFd(-1)
{
}

CanHwSocketCan::~CanHwSocketCan() { shutdown(); }

bool CanHwSocketCan::init(uint8_t const channelId, uint32_t const baudrate)
{
    m_socketFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socketFd < 0)
    {
        logger::Logger::error(logger::CANSTACK, "SocketCAN: socket creation failed");
        return false;
    }

    // Non-blocking: mainFunction() drains whatever is available.
    int const flags = fcntl(m_socketFd, F_GETFL, 0);
    (void)fcntl(m_socketFd, F_SETFL, flags | O_NONBLOCK);

    ifreq ifr{};
    (void)strncpy(ifr.ifr_name, m_interfaceName.c_str(), sizeof(ifr.ifr_name) - 1U);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1U] = '\0';

    if (ioctl(m_socketFd, SIOCGIFINDEX, &ifr) < 0)
    {
        logger::Logger::error(
            logger::CANSTACK, "SocketCAN: interface %s not found", m_interfaceName.c_str());
        (void)close(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(m_socketFd, reinterpret_cast<sockaddr const*>(&addr), sizeof(addr)) < 0)
    {
        logger::Logger::error(
            logger::CANSTACK, "SocketCAN: bind to %s failed", m_interfaceName.c_str());
        (void)close(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    m_channelId   = channelId;
    m_baudrate    = baudrate; // Actual bit timing is managed by the interface (ip link).
    m_initialized = true;

    logger::Logger::info(
        logger::CANSTACK,
        "SocketCAN up on %s (channel %u)",
        m_interfaceName.c_str(),
        channelId);

    return true;
}

void CanHwSocketCan::shutdown()
{
    if (m_socketFd >= 0)
    {
        (void)close(m_socketFd);
        m_socketFd = -1;
    }
    m_initialized = false;
}

bool CanHwSocketCan::transmit(
    uint8_t const /*mailboxId*/, uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    if (!m_initialized)
    {
        return false;
    }

    can_frame frame{};
    frame.can_id = frameId;
    if (frameId > CanFrame::MAX_BASE_ID)
    {
        frame.can_id |= CAN_EFF_FLAG;
    }
    frame.can_dlc = dlc;
    if ((data != nullptr) && (dlc > 0U))
    {
        (void)memcpy(frame.data, data, static_cast<size_t>(dlc));
    }

    return write(m_socketFd, &frame, sizeof(frame)) == static_cast<ssize_t>(sizeof(frame));
}

void CanHwSocketCan::registerRxCallback(uint8_t const /*mailboxId*/, RxCallback cb)
{
    m_rxCallbacks.push_back(std::move(cb));
}

void CanHwSocketCan::mainFunction()
{
    if (!m_initialized)
    {
        return;
    }

    for (;;)
    {
        can_frame frame{};
        ssize_t const readBytes = read(m_socketFd, &frame, sizeof(frame));
        if (readBytes != static_cast<ssize_t>(sizeof(frame)))
        {
            break;
        }

        uint32_t const mask
            = ((frame.can_id & CAN_EFF_FLAG) != 0) ? CAN_EFF_MASK : CAN_SFF_MASK;
        uint32_t const frameId = frame.can_id & mask;

        dispatchReceived(frameId, frame.can_dlc, frame.data);
    }
}

bool CanHwSocketCan::isInitialized() const { return m_initialized; }

void CanHwSocketCan::dispatchReceived(
    uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    // Single delivery: the first registered callback receives the frame
    // (the driver models one receive buffer).
    for (auto const& cb : m_rxCallbacks)
    {
        if (cb)
        {
            cb(frameId, dlc, data);
            return;
        }
    }
}

} // namespace canstack

#endif // CANSTACK_WITH_SOCKETCAN
