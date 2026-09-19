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
#include <functional>
#include <vector>

namespace canstack
{

/// Receive mailbox description of a channel.
struct MailboxConfig
{
    uint8_t mailboxId;
    /// true when the mailbox accepts every frame id
    bool acceptAll;
    /// accepted raw frame id when acceptAll is false
    uint32_t frameId;
};

/// Channel configuration: one entry per physical CAN bus.
struct CanChannelConfig
{
    uint8_t channelId;
    CanHwInterface* hwDriver;
    uint32_t baudrate;
    uint8_t maxMailboxes;
};

/// Upper layer notification: (channelId, frameId, dlc, data).
using ChannelRxCallback = std::function<void(uint8_t, uint32_t, uint8_t, uint8_t const*)>;

/**
 * One CanChannel instance per physical CAN bus.
 *
 * A channel owns the binding between a hardware driver and the upper layers:
 * it registers one receive callback per configured mailbox at the driver and
 * forwards received frames to the registered upper layer callback with the
 * channel id attached. Transmissions are distributed over the mailboxes in
 * round robin order.
 */
class CanChannel
{
public:
    CanChannel() = default;

    /**
     * Binds the channel to its hardware driver.
     * \param config driver, baudrate and mailbox layout
     * \return false when the config is invalid (null driver, no mailboxes) or
     * the driver fails to come up
     */
    bool init(CanChannelConfig const& config);

    /// Releases the hardware driver.
    void shutdown();

    /**
     * Forwards a received frame to the upper layer callback.
     * Called by the hardware receive path; also usable to inject frames (e.g.
     * from tests or a gateway replaying buffered traffic).
     */
    void onFrameReceived(uint32_t frameId, uint8_t dlc, uint8_t const* data);

    /// Transmits a frame over the bound driver.
    bool transmitFrame(uint32_t frameId, uint8_t dlc, uint8_t const* data);

    /// Registers the upper layer receive callback (channelId prefixed).
    void registerUpperLayerCallback(ChannelRxCallback cb);

    /// Polls the hardware driver; call periodically from the stack's run path.
    void mainFunction();

    /// \return the mailbox configuration of this channel
    std::vector<MailboxConfig> const& getMailboxes() const;

    uint8_t getChannelId() const;
    bool isInitialized() const;
    CanHwInterface* getHwDriver() const;

private:
    void onFrameReceivedForChannel(
        uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data);

    CanChannelConfig m_config{};
    ChannelRxCallback m_upperCallback;
    std::vector<MailboxConfig> m_mailboxes;
    uint8_t m_nextTxMailbox = 0U;
};

} // namespace canstack
