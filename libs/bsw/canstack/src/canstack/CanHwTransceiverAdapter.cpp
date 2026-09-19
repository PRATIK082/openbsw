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
#include "canstack/CanHwTransceiverAdapter.h"
#include "canstack/CanFrame.h"
#include "canstack/CanStackLogger.h"

#include "can/canframes/CanId.h"

namespace canstack
{
namespace logger = ::util::logger;

CanHwTransceiverAdapter::CanHwTransceiverAdapter(::can::ICanTransceiver& transceiver)
: m_transceiver(transceiver), m_filter()
{
    // Accept every frame; fine filtering is done by the canstack upper layers.
    m_filter.open();
}

CanHwTransceiverAdapter::~CanHwTransceiverAdapter()
{
    if (m_initialized)
    {
        m_transceiver.removeCANFrameListener(*this);
    }
}

bool CanHwTransceiverAdapter::init(uint8_t const channelId, uint32_t const baudrate)
{
    if (m_transceiver.init() != ::can::ICanTransceiver::ErrorCode::CAN_ERR_OK)
    {
        logger::Logger::error(logger::CANSTACK, "Transceiver init failed");
        return false;
    }

    if (m_transceiver.getBaudrate() != baudrate)
    {
        logger::Logger::warn(
            logger::CANSTACK,
            "Transceiver baudrate %lu differs from requested %lu",
            m_transceiver.getBaudrate(),
            baudrate);
    }

    if (m_transceiver.open() != ::can::ICanTransceiver::ErrorCode::CAN_ERR_OK)
    {
        logger::Logger::error(logger::CANSTACK, "Transceiver open failed");
        return false;
    }

    m_transceiver.addCANFrameListener(*this);

    m_channelId   = channelId;
    m_initialized = true;

    return true;
}

void CanHwTransceiverAdapter::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    m_transceiver.removeCANFrameListener(*this);
    (void)m_transceiver.close();

    m_initialized = false;
}

bool CanHwTransceiverAdapter::transmit(
    uint8_t const /*mailboxId*/,
    uint32_t const frameId,
    uint8_t const dlc,
    uint8_t const* data)
{
    if (!m_initialized)
    {
        return false;
    }

    bool const isExtended = (frameId > CanFrame::MAX_BASE_ID);
    uint32_t const qualifiedId = ::can::CanId::id(frameId, isExtended);

    ::can::CANFrame frame(qualifiedId, data, dlc, isExtended);

    return m_transceiver.write(frame) == ::can::ICanTransceiver::ErrorCode::CAN_ERR_OK;
}

void CanHwTransceiverAdapter::registerRxCallback(uint8_t const /*mailboxId*/, RxCallback cb)
{
    m_rxCallbacks.push_back(std::move(cb));
}

void CanHwTransceiverAdapter::mainFunction()
{
    // The underlying transceiver is interrupt/callback driven; nothing to poll.
}

bool CanHwTransceiverAdapter::isInitialized() const { return m_initialized; }

void CanHwTransceiverAdapter::frameReceived(::can::CANFrame const& frame)
{
    uint32_t const rawId = ::can::CanId::rawId(frame.getId());

    // Single delivery: the first registered callback receives the frame
    // (the adapter models one receive buffer).
    for (auto const& cb : m_rxCallbacks)
    {
        if (cb)
        {
            cb(rawId, frame.getPayloadLength(), frame.getPayload());
            return;
        }
    }
}

::can::IFilter& CanHwTransceiverAdapter::getFilter() { return m_filter; }

} // namespace canstack
