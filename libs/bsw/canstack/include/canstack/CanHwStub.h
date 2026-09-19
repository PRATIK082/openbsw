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

#include "canstack/CanFrame.h"
#include "canstack/CanHwInterface.h"

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace canstack
{
/**
 * In-memory CAN hardware driver.
 *
 * The stub keeps all transmitted and received frames in memory. It is the
 * reference implementation of CanHwInterface semantics, the driver used for
 * unit tests and the driver of choice for simulations (e.g. the POSIX
 * reference build without a bus).
 *
 * With loopback enabled every transmitted frame is fed back into the receive
 * queue, so a node can talk to itself. Received frames are dispatched from
 * mainFunction(), mirroring polling drivers. Like a real controller with
 * several receive buffers, each received frame is delivered exactly once, to
 * the callback registered for the lowest mailbox id.
 */
class CanHwStub final : public CanHwInterface
{
public:
    CanHwStub() = default;

    /// \param enabled when true, transmitted frames are looped back to rx
    void setLoopback(bool enabled);

    bool init(uint8_t channelId, uint32_t baudrate) override;
    void shutdown() override;
    bool transmit(uint8_t mailboxId, uint32_t frameId, uint8_t dlc, uint8_t const* data) override;
    void registerRxCallback(uint8_t mailboxId, RxCallback cb) override;
    void mainFunction() override;

    /// Enqueues a frame as if it had been received from the bus.
    void injectRxFrame(uint32_t frameId, uint8_t dlc, uint8_t const* data);

    /// \return all frames passed to transmit() since the last clearTxLog()
    std::vector<CanFrame> const& getTxLog() const;
    void clearTxLog();

    bool isInitialized() const;
    uint8_t getChannelId() const;
    uint32_t getBaudrate() const;

private:
    struct RxItem
    {
        uint32_t frameId;
        uint8_t dlc;
        uint8_t data[CanFrame::MAX_DATA_LENGTH];
    };

    void dispatch(RxItem const& item);

    bool m_initialized = false;
    bool m_loopback    = false;
    uint8_t m_channelId = 0U;
    uint32_t m_baudrate = 0U;
    /// Registered callbacks by mailbox id, kept in ascending mailbox order.
    std::vector<std::pair<uint8_t, RxCallback>> m_rxCallbacks;
    std::vector<CanFrame> m_txLog;
    std::deque<RxItem> m_rxQueue;
};

} // namespace canstack
