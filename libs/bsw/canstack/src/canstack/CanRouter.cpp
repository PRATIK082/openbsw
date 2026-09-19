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
#include "canstack/CanRouter.h"
#include "canstack/CanFrame.h"
#include "canstack/CanStackLogger.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace canstack
{
namespace logger = ::util::logger;

void CanRouter::setDatabase(SignalDatabase const* db) { m_signalDb = db; }

void CanRouter::setTransmitFunction(TransmitFunction transmit) { m_transmit = std::move(transmit); }

void CanRouter::addRoutingRule(RoutingRule const& rule) { m_routingTable.push_back(rule); }

void CanRouter::clearRules() { m_routingTable.clear(); }

size_t CanRouter::getRuleCount() const { return m_routingTable.size(); }

void CanRouter::onFrameReceived(
    uint8_t const channelId, uint32_t const frameId, uint8_t const dlc, uint8_t const* data)
{
    for (auto const& rule : m_routingTable)
    {
        if ((rule.inputChannel != channelId) || (rule.inputFrameId != frameId))
        {
            continue;
        }

        bool const frameLevel
            = (rule.transform == nullptr)
              && std::all_of(
                  rule.outputDestinations.begin(),
                  rule.outputDestinations.end(),
                  [frameId](std::pair<uint8_t, uint32_t> const& destination) {
                      return destination.second == frameId;
                  });

        if (frameLevel)
        {
            forwardFrameLevel(rule, dlc, data);
        }
        else
        {
            forwardSignalLevel(rule, channelId, frameId, dlc, data);
        }
    }
}

void CanRouter::forwardFrameLevel(RoutingRule const& rule, uint8_t const dlc, uint8_t const* data)
{
    if (!m_transmit)
    {
        return;
    }

    for (auto const& destination : rule.outputDestinations)
    {
        (void)m_transmit(destination.first, destination.second, dlc, data);
    }
}

void CanRouter::forwardSignalLevel(
    RoutingRule const& rule,
    uint8_t const inputChannel,
    uint32_t const inputFrameId,
    uint8_t const dlc,
    uint8_t const* data)
{
    if ((m_signalDb == nullptr) || (m_transmit == nullptr))
    {
        return;
    }

    FrameConfig const* inputFrame = m_signalDb->getFrameByChannelAndId(inputChannel, inputFrameId);
    if (inputFrame == nullptr)
    {
        logger::Logger::debug(
            logger::CANSTACK, "Router: input frame 0x%lx unknown", inputFrameId);
        return;
    }

    uint8_t inputData[CanFrame::MAX_DATA_LENGTH] = {};
    (void)memcpy(inputData, data, static_cast<size_t>(dlc));

    for (auto const& destination : rule.outputDestinations)
    {
        FrameConfig const* outputFrame
            = m_signalDb->getFrameByChannelAndId(destination.first, destination.second);
        if (outputFrame == nullptr)
        {
            logger::Logger::debug(
                logger::CANSTACK, "Router: output frame 0x%lx unknown", destination.second);
            continue;
        }

        uint8_t outputData[CanFrame::MAX_DATA_LENGTH] = {};

        for (auto const& outputSignal : outputFrame->signals)
        {
            // Find the matching input signal by name.
            SignalConfig const* inputSignal = nullptr;
            for (auto const& candidate : inputFrame->signals)
            {
                if (candidate.signalName == outputSignal.signalName)
                {
                    inputSignal = &candidate;
                    break;
                }
            }

            if (inputSignal == nullptr)
            {
                continue;
            }

            double value = m_signalDb->unpackSignal(inputData, *inputSignal);

            if (rule.transform)
            {
                value = rule.transform(value);
                if (std::isnan(value))
                {
                    // The transform dropped this signal.
                    continue;
                }
            }

            m_signalDb->packSignal(outputData, outputSignal, value);
        }

        uint8_t const outputDlc = (outputFrame->dlc > 0U) ? outputFrame->dlc : dlc;
        (void)m_transmit(destination.first, destination.second, outputDlc, outputData);
    }
}

} // namespace canstack
