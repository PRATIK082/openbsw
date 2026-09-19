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

#include <cstdint>
#include <functional>

namespace canstack
{
/**
 * Hardware abstraction interface for CAN controller drivers.
 *
 * One implementing object represents a single CAN controller (chip or
 * on-controller peripheral). Concrete drivers are:
 *
 * - CanHwStub: in-memory driver for tests and simulation (supports loopback).
 * - CanHwMcp2515: register-level driver for the Microchip MCP2515 via an
 *   injected SPI bus.
 * - CanHwSocketCan: Linux SocketCAN driver (only compiled with
 *   CANSTACK_WITH_SOCKETCAN).
 * - CanHwTransceiverAdapter: adapter around an existing cpp2can
 *   `can::ICanTransceiver` implementation (e.g. the platform transceivers for
 *   S32K1xx or STM32).
 *
 * Mailbox semantics are driver defined: `transmit` selects the transmit buffer
 * (hardware TX buffer index), `registerRxCallback` subscribes to frames
 * accepted by the given receive buffer index. Drivers with a single receive
 * path may ignore the mailbox index for reception and deliver all frames to
 * every registered callback.
 *
 * Callback signature: (frameId, dlc, data).
 */
class CanHwInterface
{
public:
    using RxCallback = std::function<void(uint32_t, uint8_t, uint8_t const*)>;

    virtual ~CanHwInterface() = default;

    CanHwInterface(CanHwInterface const&)            = delete;
    CanHwInterface& operator=(CanHwInterface const&) = delete;

    /**
     * Brings the controller up with the given baudrate.
     * \param channelId logical channel the controller is attached to
     * \param baudrate bus baudrate in bits per second
     * \return true if the controller was initialized successfully
     */
    virtual bool init(uint8_t channelId, uint32_t baudrate) = 0;

    /// Brings the controller down and releases hardware resources.
    virtual void shutdown() = 0;

    /**
     * Enqueues a frame for transmission.
     * \param mailboxId driver defined transmit buffer index
     * \param frameId raw frame identifier
     * \param dlc payload length in bytes
     * \param data payload bytes
     * \return true if the frame was accepted by the controller
     */
    virtual bool transmit(
        uint8_t mailboxId, uint32_t frameId, uint8_t dlc, uint8_t const* data)
        = 0;

    /**
     * Registers the receive callback for a receive buffer.
     * \param mailboxId driver defined receive buffer index
     * \param cb callback invoked from mainFunction() (or interrupt context for
     * interrupt driven drivers)
     */
    virtual void registerRxCallback(uint8_t mailboxId, RxCallback cb) = 0;

    /// Polling/interrupt handler; must be called periodically by the owner of
    /// the driver (e.g. from CanChannel::mainFunction()).
    virtual void mainFunction() = 0;

protected:
    CanHwInterface() = default;
};

} // namespace canstack
