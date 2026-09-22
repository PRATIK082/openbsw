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
 * \ingroup commgateway
 */
#include "commgateway/CommStack.h"

#include "commgateway/GatewayConfigParser.h"

#include <cstring>

namespace commgateway
{

CommStack& CommStack::getInstance()
{
    static CommStack instance;
    return instance;
}

void CommStack::configure(CommStackConfig const& config, ::async::ContextType context)
{
    m_config     = config;
    m_context    = context;
    m_configured = true;
}

::async::ContextType CommStack::getTransitionContext(Transition::Type const /* transition */)
{
    return m_context;
}

bool CommStack::loadGatewayRules(std::string const& jsonText, std::string& error)
{
    return GatewayConfigParser::parse(jsonText, m_signalGateway, m_timeoutMonitor,
                                      m_commStateManager, error);
}

void CommStack::registerEthSignalMapping(uint32_t pduId, std::string const& signalName)
{
    m_ethSignalMap[pduId] = signalName;
}

SignalGateway& CommStack::getSignalGateway() { return m_signalGateway; }

TimeoutMonitor& CommStack::getTimeoutMonitor() { return m_timeoutMonitor; }

CommStateManager& CommStack::getCommStateManager() { return m_commStateManager; }

LinChannel& CommStack::getLinChannel() { return m_linChannel; }

EthIpduManager& CommStack::getEthIpduManager() { return m_ethIpdu; }

::canstack::CanInterface& CommStack::getCanInterface() { return m_canInterface; }

CommStatistics CommStack::getStatistics() const { return m_stats; }

uint32_t CommStack::getTimeMs() const { return m_timeMs; }

bool CommStack::isConfigured() const { return m_configured; }

void CommStack::init()
{
    if (!m_configured)
    {
        transitionDone();
        return;
    }

    if (m_config.canConfig != nullptr)
    {
        (void)m_canInterface.init(*m_config.canConfig);
    }
    if (m_config.linHw != nullptr)
    {
        m_linChannel.init(m_config.linHw, m_config.linChannelId);
        m_linChannel.registerUpperLayerCallback(
            [this](uint8_t channelId, uint8_t pid, uint8_t* data, uint8_t dlc) {
                onLinFrameReceived(channelId, pid, data, dlc);
            });
    }
    if (m_config.ethTransport != nullptr)
    {
        m_ethIpdu.init(m_config.ethConfig, m_config.ethTransport);
        m_ethIpdu.registerPduCallback(
            [this](uint32_t pduId, uint16_t length, uint8_t const* data) {
                onEthPduReceived(pduId, length, data);
            });
    }

    m_signalGateway.init(&m_canInterface.getSignalDatabase());
    m_signalGateway.setChannelSender([this](uint8_t channelType, uint32_t channelId,
                                            std::string const& signal, double value) {
        return dispatchToChannel(channelType, channelId, signal, value);
    });
    m_timeoutMonitor.init();
    TxConfirmationMgr::getInstance().init();
    m_commStateManager.init();
    m_commStateManager.registerChannel(CHANNEL_TYPE_CAN, 0U);
    m_commStateManager.registerChannel(CHANNEL_TYPE_LIN, m_config.linChannelId);
    m_commStateManager.registerChannel(CHANNEL_TYPE_ETH, 0U);

    transitionDone();
}

void CommStack::run()
{
    ::async::scheduleAtFixedRate(m_context, *this, m_timeout, COMMSTACK_RUN_PERIOD_MS,
                                 ::async::TimeUnit::MILLISECONDS);
    transitionDone();
}

void CommStack::shutdown()
{
    m_timeout.cancel();
    m_signalGateway.shutdown();
    m_timeoutMonitor.shutdown();
    TxConfirmationMgr::getInstance().shutdown();
    m_commStateManager.shutdown();
    m_linChannel.shutdown();
    m_ethIpdu.shutdown();
    m_canInterface.shutdown();
    transitionDone();
}

void CommStack::execute()
{
    m_timeMs += COMMSTACK_RUN_PERIOD_MS;

    m_canInterface.mainFunctionTx();
    m_canInterface.mainFunctionRx();
    m_linChannel.mainFunction(m_timeMs);
    m_ethIpdu.mainFunction(m_timeMs);
    m_signalGateway.mainFunction(m_timeMs);
    if ((m_timeMs % 10U) == 0U)
    {
        m_timeoutMonitor.mainFunction(m_timeMs);
    }
    TxConfirmationMgr::getInstance().mainFunction(m_timeMs);
    m_commStateManager.mainFunction(m_timeMs);

    m_stats.timeoutCount     = m_timeoutMonitor.getTimeoutCount();
    m_stats.gatewayDropCount = m_signalGateway.getDropCount();
}

bool CommStack::dispatchToChannel(uint8_t channelType, uint32_t channelId,
                                  std::string const& signal, double value)
{
    if (channelType == CHANNEL_TYPE_CAN)
    {
        ::canstack::SignalConfig const* signalConfig
            = m_canInterface.getSignalDatabase().getSignalByName(signal);
        if (signalConfig == nullptr)
        {
            return false;
        }
        ::canstack::FrameConfig const* frame = m_canInterface.getSignalDatabase()
                                                   .getFrameByChannelAndId(signalConfig->channelId,
                                                                           signalConfig->frameId);
        if (frame == nullptr)
        {
            return false;
        }
        uint8_t data[8U] = {0U};
        m_canInterface.getSignalDatabase().packSignal(data, *signalConfig, value);
        m_stats.txFrameCount++;
        return m_canInterface.sendFrame(static_cast<uint8_t>(channelId), signalConfig->frameId,
                                        frame->dlc, data);
    }
    if (channelType == CHANNEL_TYPE_LIN)
    {
        for (uint8_t pid = 0U; pid < 64U; ++pid)
        {
            LinFrameConfig const* frame = m_linChannel.findFrame(LinChannel::computePid(pid));
            if ((frame != nullptr) && (frame->associatedSignal == signal))
            {
                uint8_t data[8U] = {0U};
                uint32_t raw     = static_cast<uint32_t>(value);
                for (uint8_t i = 0U; (i < frame->dlc) && (i < 8U); ++i)
                {
                    data[i] = static_cast<uint8_t>((raw >> (8U * i)) & 0xFFU);
                }
                m_stats.txFrameCount++;
                return m_linChannel.transmitFrame(frame->pid, data, frame->dlc);
            }
        }
        return false;
    }
    if (channelType == CHANNEL_TYPE_ETH)
    {
        uint8_t data[8U] = {0U};
        (void)std::memcpy(data, &value, sizeof(value));
        m_stats.txFrameCount++;
        return m_ethIpdu.transmitPdu(m_ethIpdu.getConfig().primaryPduId, sizeof(data), data);
    }
    return false;
}

void CommStack::onLinFrameReceived(uint8_t channelId, uint8_t pid, uint8_t* data, uint8_t dlc)
{
    m_stats.rxFrameCount++;
    m_timeoutMonitor.notifyFrameRx(channelId, pid, m_timeMs);
    LinFrameConfig const* frame = m_linChannel.findFrame(pid);
    if ((frame == nullptr) || frame->associatedSignal.empty() || (data == nullptr)
        || (dlc == 0U))
    {
        return;
    }
    double const value = static_cast<double>(data[0]);
    m_timeoutMonitor.notifyRxActivity(frame->associatedSignal, m_timeMs);
    m_signalGateway.onSignalUpdate(frame->associatedSignal, value, m_timeMs);
}

void CommStack::onEthPduReceived(uint32_t pduId, uint16_t length, uint8_t const* data)
{
    m_stats.rxFrameCount++;
    auto it = m_ethSignalMap.find(pduId);
    if ((it == m_ethSignalMap.end()) || (data == nullptr) || (length == 0U))
    {
        return;
    }
    double value = 0.0;
    if (length >= sizeof(value))
    {
        (void)std::memcpy(&value, data, sizeof(value));
    }
    else
    {
        value = static_cast<double>(data[0]);
    }
    m_timeoutMonitor.notifyRxActivity(it->second, m_timeMs);
    m_signalGateway.onSignalUpdate(it->second, value, m_timeMs);
}

} // namespace commgateway
