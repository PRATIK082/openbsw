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
 * \ingroup gwbridge
 *
 * Reuse note: queueing uses the existing io::MemoryQueue with its
 * io::IReader/io::IWriter implementations (libs/bsw/io). The 8-byte
 * [BE messageId][BE length] framing matches routing::RxAdapter
 * (libs/bsw/routing/src/routing/RxAdapter.cpp) and LegacyTxAdapter.
 */
#pragma once

#include <io/MemoryQueue.h>

#include <cstddef>
#include <cstdint>
#include <functional>

namespace gwbridge
{

/// Outbound frame sink: (messageId, length, data) -> accepted.
using TxSink = std::function<bool(uint32_t, uint16_t, uint8_t const*)>;

/// Size of the routing message header: BE id + BE length.
static size_t const MESSAGE_HEADER_SIZE = 8U;

/**
 * Bidirectional queue bridge between a bus channel and the routing module.
 *
 * RX: pushRxFrame() stores one framed message; rxReader() exposes it as
 * io::IReader for routing::LegacyRxAdapter.
 * TX: txWriter() accepts framed PDUs as io::IWriter; drainTxFrame() pops
 * one message into a TxSink (usually bound to the channel transmit path).
 */
template <size_t RX_CAPACITY, size_t RX_ELEMENT_SIZE, size_t TX_CAPACITY, size_t TX_ELEMENT_SIZE>
class IoBridge
{
public:
    IoBridge() : m_rxReader(m_rxQueue), m_rxWriter(m_rxQueue), m_txReader(m_txQueue), m_txWriter(m_txQueue) {}

    IoBridge(IoBridge const&)            = delete;
    IoBridge& operator=(IoBridge const&) = delete;

    /**
     * Queues one received frame as a routing message.
     * \return false when the frame does not fit or the queue is full.
     */
    bool pushRxFrame(uint32_t messageId, uint8_t const* data, uint16_t length)
    {
        if ((data == nullptr) && (length > 0U))
        {
            return false;
        }
        size_t const total = MESSAGE_HEADER_SIZE + length;
        ::etl::span<uint8_t> slot = m_rxWriter.allocate(total);
        if (slot.size() != total)
        {
            m_rxDropCount++;
            return false;
        }
        writeBigEndian32(slot.data(), messageId);
        writeBigEndian32(slot.data() + 4U, length);
        for (uint16_t i = 0U; i < length; ++i)
        {
            slot[MESSAGE_HEADER_SIZE + i] = data[i];
        }
        m_rxWriter.commit();
        return true;
    }

    /**
     * Pops one queued TX message into the sink.
     * \return false when empty, malformed, or rejected by the sink.
     */
    bool drainTxFrame(TxSink const& sink)
    {
        ::etl::span<uint8_t> message = m_txReader.peek();
        if (message.empty())
        {
            return false;
        }
        bool accepted = false;
        if (message.size() >= MESSAGE_HEADER_SIZE)
        {
            uint32_t const messageId = readBigEndian32(message.data());
            uint32_t const length    = readBigEndian32(message.data() + 4U);
            if ((length <= 0xFFFFU)
                && ((MESSAGE_HEADER_SIZE + length) <= message.size()) && sink)
            {
                accepted = sink(messageId, static_cast<uint16_t>(length),
                                message.data() + MESSAGE_HEADER_SIZE);
            }
        }
        m_txReader.release();
        if (!accepted)
        {
            m_txDropCount++;
        }
        return accepted;
    }

    ::io::IReader& rxReader() { return m_rxReader; }
    ::io::IWriter& txWriter() { return m_txWriter; }
    uint32_t getRxDropCount() const { return m_rxDropCount; }
    uint32_t getTxDropCount() const { return m_txDropCount; }

    static void writeBigEndian32(uint8_t* out, uint32_t value)
    {
        out[0] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
        out[1] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
        out[2] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
        out[3] = static_cast<uint8_t>(value & 0xFFU);
    }

    static uint32_t readBigEndian32(uint8_t const* in)
    {
        return (static_cast<uint32_t>(in[0]) << 24U) | (static_cast<uint32_t>(in[1]) << 16U)
               | (static_cast<uint32_t>(in[2]) << 8U) | static_cast<uint32_t>(in[3]);
    }

private:
    ::io::MemoryQueue<RX_CAPACITY, RX_ELEMENT_SIZE> m_rxQueue;
    ::io::MemoryQueue<TX_CAPACITY, TX_ELEMENT_SIZE> m_txQueue;
    ::io::MemoryQueueReader< ::io::MemoryQueue<RX_CAPACITY, RX_ELEMENT_SIZE>> m_rxReader;
    ::io::MemoryQueueWriter< ::io::MemoryQueue<RX_CAPACITY, RX_ELEMENT_SIZE>> m_rxWriter;
    ::io::MemoryQueueReader< ::io::MemoryQueue<TX_CAPACITY, TX_ELEMENT_SIZE>> m_txReader;
    ::io::MemoryQueueWriter< ::io::MemoryQueue<TX_CAPACITY, TX_ELEMENT_SIZE>> m_txWriter;
    uint32_t m_rxDropCount = 0U;
    uint32_t m_txDropCount = 0U;
};

} // namespace gwbridge
