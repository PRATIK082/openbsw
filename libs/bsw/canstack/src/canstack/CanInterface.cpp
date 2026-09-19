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
#include "canstack/CanInterface.h"
#include "canstack/CanStackLogger.h"

#include <cmath>
#include <cstring>

namespace canstack
{
namespace logger = ::util::logger;

CanInterface& CanInterface::getInstance()
{
    static CanInterface instance;
    return instance;
}

bool CanInterface::init(CanStackConfig const& config)
{
    if (m_initialized)
    {
        logger::Logger::warn(logger::CANSTACK, "Stack already initialized");
        return true;
    }

    if ((config.channelConfigs == nullptr) || (config.channelCount == 0U))
    {
        logger::Logger::error(logger::CANSTACK, "Stack config has no channels");
        return false;
    }

    if (config.frameTable != nullptr)
    {
        m_signalDb.loadFrameTable(config.frameTable, config.frameCount);
    }
    if (config.signalTable != nullptr)
    {
        m_signalDb.loadSignalTable(config.signalTable, config.signalCount);
    }

    // Construct the channels in place: CanChannel registers callbacks bound to
    // its own address at the driver, so channels must never be moved or copied
    // after init (the reserve guarantees stable storage).
    m_channels.reserve(config.channelCount);
    for (size_t i = 0U; i < config.channelCount; ++i)
    {
        m_channels.emplace_back();

        CanChannel& channel = m_channels.back();
        if (!channel.init(config.channelConfigs[i]))
        {
            logger::Logger::error(
                logger::CANSTACK, "Channel %u failed to come up", config.channelConfigs[i].channelId);

            // Tear down what was already built so the stack stays consistent.
            for (auto& builtChannel : m_channels)
            {
                builtChannel.shutdown();
            }
            m_channels.clear();
            return false;
        }

        channel.registerUpperLayerCallback(
            [this](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
                onChannelFrameReceived(channelId, frameId, dlc, data);
            });
    }

    m_txScheduler.setDatabase(&m_signalDb);
    m_txScheduler.setTransmitFunction(
        [this](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
            return sendFrame(channelId, frameId, dlc, data);
        });

    m_router.setDatabase(&m_signalDb);
    m_router.setTransmitFunction(
        [this](uint8_t channelId, uint32_t frameId, uint8_t dlc, uint8_t const* data) {
            return sendFrame(channelId, frameId, dlc, data);
        });

    m_rxQueue.clear();
    m_rxDroppedCount = 0U;
    m_initialized     = true;

    logger::Logger::info(
        logger::CANSTACK,
        "Stack up: %u channels, %u frames, %u signals",
        static_cast<unsigned>(m_channels.size()),
        static_cast<unsigned>(m_signalDb.getFrameCount()),
        static_cast<unsigned>(m_signalDb.getSignalCount()));

    return true;
}

void CanInterface::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    for (auto& channel : m_channels)
    {
        channel.shutdown();
    }
    m_channels.clear();
    m_rxQueue.clear();
    m_rxValues.clear();
    m_txScheduler.clear();
    m_router.clearRules();
    m_initialized = false;
}

bool CanInterface::sendSignal(std::string const& signalName, double const value)
{
    bool registered = false;
    for (auto const& candidate : m_txScheduler.getEntries())
    {
        if (candidate.signalName == signalName)
        {
            registered = true;
            break;
        }
    }

    if (!registered)
    {
        logger::Logger::debug(
            logger::CANSTACK, "sendSignal: %s is not registered for cyclic tx", signalName.c_str());
        return false;
    }

    m_txScheduler.setSignalValue(signalName, value);

    return true;
}

double CanInterface::readSignal(std::string const& signalName)
{
    auto const it = m_rxValues.find(signalName);
    return (it != m_rxValues.end()) ? it->second : 0.0;
}

bool CanInterface::sendFrame(
    uint8_t const channelId, uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    CanChannel* channel = findChannel(channelId);
    if (channel == nullptr)
    {
        logger::Logger::warn(logger::CANSTACK, "sendFrame: no channel %u", channelId);
        return false;
    }
    return channel->transmitFrame(frameId, dlc, data);
}

void CanInterface::registerSignalCallback(
    std::string const& signalName, std::function<void(double)> callback)
{
    m_signalCallbacks[signalName] = std::move(callback);
}

void CanInterface::mainFunctionTx() { m_txScheduler.mainFunction(); }

void CanInterface::mainFunctionRx()
{
    // Poll the hardware drivers (dispatches rx callbacks for polling drivers).
    for (auto& channel : m_channels)
    {
        channel.mainFunction();
    }

    // Route and unpack everything the hardware delivered since the last call.
    while (!m_rxQueue.empty())
    {
        RxQueueItem const item = m_rxQueue.front();
        m_rxQueue.pop_front();
        processReceivedFrame(item);
    }
}

SignalDatabase& CanInterface::getSignalDatabase() { return m_signalDb; }

SignalDatabase const& CanInterface::getSignalDatabase() const { return m_signalDb; }

CanRouter& CanInterface::getRouter() { return m_router; }

CanTxScheduler& CanInterface::getTxScheduler() { return m_txScheduler; }

CanChannel* CanInterface::getChannel(uint8_t const channelId)
{
    return findChannel(channelId);
}

size_t CanInterface::getChannelCount() const { return m_channels.size(); }

bool CanInterface::isInitialized() const { return m_initialized; }

void CanInterface::onChannelFrameReceived(
    uint8_t const channelId, uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    // Gateway first: routed copies share the receive path with local consumers.
    m_router.onFrameReceived(channelId, frameId, dlc, data);

    if (m_rxQueue.size() >= CANSTACK_RX_QUEUE_DEPTH)
    {
        m_rxQueue.pop_front();
        ++m_rxDroppedCount;
        logger::Logger::debug(
            logger::CANSTACK, "Rx queue overflow, dropped %lu frames so far", m_rxDroppedCount);
    }

    RxQueueItem item;
    item.channelId = channelId;
    item.frameId   = frameId;
    item.dlc       = dlc;
    if ((data != nullptr) && (dlc > 0U))
    {
        (void)memcpy(item.data, data, static_cast<size_t>(dlc));
    }
    m_rxQueue.push_back(item);
}

void CanInterface::processReceivedFrame(RxQueueItem const& item)
{
    FrameConfig const* frame = m_signalDb.getFrameByChannelAndId(item.channelId, item.frameId);
    if (frame == nullptr)
    {
        return;
    }

    // Resolve the multiplexer switch value of the frame.
    double switchValue = 0.0;
    bool hasSwitch = false;
    for (auto const& signal : frame->signals)
    {
        if (signal.isMultiplexerSwitch)
        {
            switchValue = m_signalDb.unpackSignal(item.data, signal);
            hasSwitch   = true;
            break;
        }
    }

    for (auto const& signal : frame->signals)
    {
        if (hasSwitch && (signal.multiplexValue >= 0) && !signal.isMultiplexerSwitch)
        {
            if (static_cast<int64_t>(switchValue) != static_cast<int64_t>(signal.multiplexValue))
            {
                continue;
            }
        }

        double const value = m_signalDb.unpackSignal(item.data, signal);

        m_rxValues[signal.signalName] = value;
        notifySignal(signal, value);
    }
}

void CanInterface::notifySignal(SignalConfig const& signal, double const value)
{
    auto const it = m_signalCallbacks.find(signal.signalName);
    if ((it != m_signalCallbacks.end()) && it->second)
    {
        it->second(value);
    }
}

CanChannel* CanInterface::findChannel(uint8_t const channelId)
{
    for (auto& channel : m_channels)
    {
        if (channel.getChannelId() == channelId)
        {
            return &channel;
        }
    }
    return nullptr;
}

} // namespace canstack
