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

#include "canstack/CanChannel.h"
#include "canstack/CanRouter.h"
#include "canstack/CanTxScheduler.h"
#include "canstack/SignalDb.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace canstack
{

/// Receive queue depth between hardware callbacks and mainFunctionRx().
static uint8_t const CANSTACK_RX_QUEUE_DEPTH = 32U;

/**
 * Code-first configuration of the whole stack: constexpr tables plus driver
 * instances provided by the integrator.
 */
struct CanStackConfig
{
    /// Channel table; one entry per physical bus.
    CanChannelConfig const* channelConfigs;
    size_t channelCount;
    /// Optional code-first frame table (frame names and dlc).
    FrameTableEntry const* frameTable;
    size_t frameCount;
    /// Code-first signal table (typically generated from a DBC).
    SignalTableEntry const* signalTable;
    size_t signalCount;
};

/**
 * Application facing API of the CAN stack.
 *
 * The interface combines the signal database, the channel manager, the
 * transmit scheduler and the router:
 *
 * - sendSignal()/readSignal() work on signal level with the DBC derived
 *   database; cyclic transmission is handled by the scheduler.
 * - sendFrame() bypasses the database for custom protocols.
 * - registerSignalCallback() subscribes to received signal values.
 * - mainFunctionTx()/mainFunctionRx() drive the stack from the lifecycle run
 *   path (dedicated task/async context).
 */
class CanInterface
{
public:
    /// \return the process wide stack instance (used by application components).
    static CanInterface& getInstance();

    /**
     * Builds the stack from the code-first configuration.
     * \param config constexpr tables and channel layout
     * \return true when all channels came up
     */
    bool init(CanStackConfig const& config);

    /// Stops all channels and drops cached values.
    void shutdown();

    /// Signal-based API: stores a value for cyclic transmission.
    /// \return false when the signal is unknown or not registered for tx.
    bool sendSignal(std::string const& signalName, double value);

    /// Signal-based API: last received value of a signal (0.0 when unknown).
    double readSignal(std::string const& signalName);

    /// Frame-based API for custom protocols; bypasses the signal database.
    bool sendFrame(uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data);

    /// Registers a callback for received values of a signal.
    void registerSignalCallback(std::string const& signalName, std::function<void(double)> callback);

    /// Drives the transmit scheduler; call periodically (1 ms period).
    void mainFunctionTx();

    /// Polls the hardware, routes and unpacks received frames, fires callbacks.
    void mainFunctionRx();

    SignalDatabase& getSignalDatabase();
    SignalDatabase const& getSignalDatabase() const;
    CanRouter& getRouter();
    CanTxScheduler& getTxScheduler();

    /// \return the channel with the given id or nullptr.
    CanChannel* getChannel(uint8_t channelId);
    size_t getChannelCount() const;

    bool isInitialized() const;

private:
    struct RxQueueItem
    {
        uint8_t channelId;
        uint32_t frameId;
        uint8_t dlc;
        uint8_t data[8U];
    };

    void onChannelFrameReceived(uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data);
    void processReceivedFrame(RxQueueItem const& item);
    void notifySignal(SignalConfig const& signal, double value);
    CanChannel* findChannel(uint8_t channelId);

    SignalDatabase m_signalDb;
    std::vector<CanChannel> m_channels;
    CanTxScheduler m_txScheduler;
    CanRouter m_router;
    std::map<std::string, std::function<void(double)>> m_signalCallbacks;
    std::map<std::string, double> m_rxValues;
    std::deque<RxQueueItem> m_rxQueue;
    uint32_t m_rxDroppedCount = 0U;
    bool m_initialized = false;
};

} // namespace canstack
