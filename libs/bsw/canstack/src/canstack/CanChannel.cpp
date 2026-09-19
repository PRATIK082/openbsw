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
#include "canstack/CanChannel.h"
#include "canstack/CanStackLogger.h"

namespace canstack
{
namespace logger = ::util::logger;

bool CanChannel::init(CanChannelConfig const& config)
{
    if ((config.hwDriver == nullptr) || (config.maxMailboxes == 0U))
    {
        logger::Logger::error(logger::CANSTACK, "Channel %u: invalid config", config.channelId);
        return false;
    }

    if (!config.hwDriver->init(config.channelId, config.baudrate))
    {
        logger::Logger::error(
            logger::CANSTACK, "Channel %u: hardware init failed", config.channelId);
        return false;
    }

    m_config = config;

    m_mailboxes.clear();
    m_mailboxes.reserve(config.maxMailboxes);
    for (uint8_t i = 0U; i < config.maxMailboxes; ++i)
    {
        MailboxConfig mailbox{};
        mailbox.mailboxId = i;
        mailbox.acceptAll = true;
        mailbox.frameId    = 0U;
        m_mailboxes.push_back(mailbox);

        // Every mailbox forwards into the upper layer callback with the
        // channel id attached.
        uint8_t const channelId = config.channelId;
        config.hwDriver->registerRxCallback(
            i, [this, channelId](uint32_t frameId, uint8_t dlc, uint8_t const* data) {
                onFrameReceivedForChannel(channelId, frameId, dlc, data);
            });
    }

    m_nextTxMailbox = 0U;

    logger::Logger::debug(
        logger::CANSTACK,
        "Channel %u up with %u mailboxes",
        config.channelId,
        config.maxMailboxes);

    return true;
}

void CanChannel::onFrameReceivedForChannel(
    uint8_t const channelId, uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    if (m_upperCallback)
    {
        m_upperCallback(channelId, frameId, dlc, data);
    }
}

void CanChannel::shutdown()
{
    if (m_config.hwDriver != nullptr)
    {
        m_config.hwDriver->shutdown();
    }
    m_mailboxes.clear();
    m_config = CanChannelConfig{};
}

void CanChannel::onFrameReceived(uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    onFrameReceivedForChannel(m_config.channelId, frameId, dlc, data);
}

bool CanChannel::transmitFrame(uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    if ((m_config.hwDriver == nullptr) || m_mailboxes.empty())
    {
        return false;
    }

    uint8_t const mailboxId = m_nextTxMailbox;
    m_nextTxMailbox = static_cast<uint8_t>((m_nextTxMailbox + 1U) % m_mailboxes.size());

    return m_config.hwDriver->transmit(mailboxId, frameId, dlc, data);
}

void CanChannel::registerUpperLayerCallback(ChannelRxCallback cb)
{
    m_upperCallback = std::move(cb);
}

void CanChannel::mainFunction()
{
    if (m_config.hwDriver != nullptr)
    {
        m_config.hwDriver->mainFunction();
    }
}

std::vector<MailboxConfig> const& CanChannel::getMailboxes() const { return m_mailboxes; }

uint8_t CanChannel::getChannelId() const { return m_config.channelId; }

bool CanChannel::isInitialized() const { return !m_mailboxes.empty(); }

CanHwInterface* CanChannel::getHwDriver() const { return m_config.hwDriver; }

} // namespace canstack
