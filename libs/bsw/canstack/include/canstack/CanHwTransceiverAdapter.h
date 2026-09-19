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

#include "can/canframes/CANFrame.h"
#include "can/filter/MaskFilter.h"
#include "can/framemgmt/ICANFrameListener.h"
#include "can/transceiver/ICanTransceiver.h"

#include <cstdint>
#include <vector>

namespace canstack
{
/**
 * Adapter that turns an existing cpp2can transceiver into a CanHwInterface.
 *
 * The platform transceivers of OpenBSW (e.g. `canflex2Transceiver` for S32K1xx
 * or the STM32 bspCan transceivers) implement `can::ICanTransceiver`. This
 * adapter forwards the canstack calls to such a transceiver:
 *
 * - init() calls the transceiver's init() and open().
 * - transmit() builds a `can::CANFrame` and writes it.
 * - registerRxCallback() adds an internal frame listener; every received frame
 *   is dispatched to every registered callback.
 * - mainFunction() is a no-op: the underlying transceiver is interrupt/callback
 *   driven; received frames arrive on the transceiver's own execution context.
 *
 * The mailboxId parameters are ignored; the adapter has a single transmit path
 * and delivers received frames to all callbacks.
 */
class CanHwTransceiverAdapter final
: public CanHwInterface
, private ::can::ICANFrameListener
{
public:
    explicit CanHwTransceiverAdapter(::can::ICanTransceiver& transceiver);

    ~CanHwTransceiverAdapter() override;

    bool init(uint8_t channelId, uint32_t baudrate) override;
    void shutdown() override;
    bool transmit(uint8_t mailboxId, uint32_t frameId, uint8_t dlc, uint8_t const* data) override;
    void registerRxCallback(uint8_t mailboxId, RxCallback cb) override;
    void mainFunction() override;

    /// \return true after a successful init()
    bool isInitialized() const;

private:
    // ::can::ICANFrameListener
    void frameReceived(::can::CANFrame const& frame) override;
    ::can::IFilter& getFilter() override;

    ::can::ICanTransceiver& m_transceiver;
    ::can::MaskFilter m_filter;
    bool m_initialized = false;
    uint8_t m_channelId = 0U;
    std::vector<RxCallback> m_rxCallbacks;
};

} // namespace canstack
