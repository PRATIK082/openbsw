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
 * Reuse note: wires the EXISTING routing module (routing::Router,
 * LegacyRxAdapter, LegacyTxAdapter, PduRoutingTable, RxAdapterTable,
 * TxAdapterTable) to io::IReader/io::IWriter endpoints from IoBridge.
 * Tables are built code-first from plain structs; the same structs map 1:1
 * to tools/blob routing.jsonl entries for production blob generation.
 */
#pragma once

#include <routing/ErrorHandler.h>
#include <routing/LegacyRxAdapter.h>
#include <routing/LegacyTxAdapter.h>
#include <routing/PduRoutingTable.h>
#include <routing/Router.h>
#include <routing/RxAdapterTable.h>
#include <routing/TxAdapterTable.h>

#include <etl/span.h>
#include <etl/unaligned_type.h>
#include <io/IReader.h>
#include <io/IWriter.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace gwbridge
{

/// One PDU inside a received message.
struct BridgePduLayout
{
    uint32_t offset = 0U;
    uint32_t length = 0U;
};

/// One receivable message with its PDU split.
struct BridgeRxMessage
{
    uint32_t messageId     = 0U;
    uint32_t messageLength = 0U;
    std::vector<BridgePduLayout> pdus;
};

/// One transmittable message (single PDU placed at offset, 0xFF padded).
struct BridgeTxMessage
{
    uint32_t messageId     = 0U;
    uint32_t messageLength = 0U;
    uint32_t pduOffset     = 0U;
};

/// Per-channel adapter tables; channelIndex selects the router slot.
struct BridgeChannelConfig
{
    uint8_t channelIndex = 0U;
    std::vector<BridgeRxMessage> rxMessages;
    std::vector<BridgeTxMessage> txMessages;
};

/// One PDU route: (srcChannel, srcMessage, pduIndex) -> (dstChannel, dstMessage).
struct BridgeRoute
{
    uint8_t srcChannel  = 0U;
    uint32_t srcMessage = 0U;
    uint8_t srcPduIndex = 0U;
    uint8_t dstChannel  = 0U;
    uint32_t dstMessage = 0U;
};

/**
 * Builder + owner of a real routing::Router with its adapters.
 *
 * Global PDU ids are assigned in addChannel() order (channel, message, pdu);
 * routes are resolved at init(). Tables alias member storage, so a
 * RouterBridge must outlive its router use (same object — always true).
 */
template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE = 512U>
class RouterBridge
{
public:
    RouterBridge() = default;

    RouterBridge(RouterBridge const&)            = delete;
    RouterBridge& operator=(RouterBridge const&) = delete;

    bool addChannel(BridgeChannelConfig const& config);
    bool addRoute(BridgeRoute const& route);
    void clear();

    /**
     * Builds tables/adapters and inits the router.
     * readers/writers are indexed by channelIndex (nullptr = unused slot).
     */
    bool init(::etl::span<::io::IReader*> readers, ::etl::span<::io::IWriter*> writers);

    /// Pumps the router until idle (max 32 routes per call).
    bool run();

    size_t getChannelCount() const { return m_channels.size(); }
    size_t getRouteCount() const { return m_routes.size(); }
    uint32_t getErrorCount() const { return m_errorCount; }
    uint32_t getRoutedCount() const { return m_routedCount; }
    bool isInitialized() const { return m_initialized; }

private:
    struct ChannelStorage
    {
        uint8_t channelIndex       = 0U;
        uint32_t firstPduId        = 0U;
        std::vector<BridgeRxMessage> rxMessages;
        std::vector<BridgeTxMessage> txMessages;
        std::vector<::etl::be_uint32_t> rxMessageIds;
        std::vector<::etl::be_uint32_t> rxMessageLengths;
        std::vector<::etl::be_uint32_t> rxPduLengthOffsets;
        std::vector<::etl::be_uint32_t> rxPduLengths;
        std::vector<::etl::be_uint32_t> rxPduOffsets;
        std::vector<::etl::be_uint32_t> txMessageIds;
        std::vector<::etl::be_uint32_t> txMessageLengths;
        std::vector<::etl::be_uint32_t> txPduOffsets;
        ::routing::RxAdapterTable rxTable{};
        ::routing::TxAdapterTable txTable{};
        std::unique_ptr<::routing::LegacyRxAdapter<RX_MAX_PDU_SIZE>> rxAdapter;
        std::unique_ptr<::routing::LegacyTxAdapter> txAdapter;
    };

    bool resolveRoutes();
    void onRoutingError(::routing::ErrorHandler::StatusCode status, uint8_t channelId,
                        uint32_t messageId);

    std::vector<ChannelStorage> m_channels;
    std::vector<BridgeRoute> m_routes;
    ::routing::PduRoutingTable m_routingTable{};
    std::vector<uint8_t> m_destinations;
    std::vector<::etl::be_uint32_t> m_destinationOffsets;
    std::vector<::etl::be_uint32_t> m_outputMessageIds;
    ::routing::Router<MAX_CHANNELS> m_router;
    ::io::IReader* m_readers[MAX_CHANNELS] = {};
    ::io::IWriter* m_writers[MAX_CHANNELS] = {};
    uint32_t m_nextPduId     = 0U;
    uint32_t m_errorCount    = 0U;
    uint32_t m_routedCount   = 0U;
    bool m_initialized       = false;
};

// ---------------------------------------------------------------------------
// Template implementation (header-only, like routing::Router itself).
// ---------------------------------------------------------------------------

template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE>
bool RouterBridge<MAX_CHANNELS, RX_MAX_PDU_SIZE>::addChannel(BridgeChannelConfig const& config)
{
    if (m_initialized || (config.channelIndex >= MAX_CHANNELS))
    {
        return false;
    }
    for (auto const& existing : m_channels)
    {
        if (existing.channelIndex == config.channelIndex)
        {
            return false;
        }
    }
    ChannelStorage storage{};
    storage.channelIndex = config.channelIndex;
    storage.firstPduId   = m_nextPduId;
    storage.rxMessages   = config.rxMessages;
    storage.txMessages   = config.txMessages;
    for (auto const& message : config.rxMessages)
    {
        m_nextPduId += static_cast<uint32_t>(message.pdus.size());
    }
    m_channels.push_back(std::move(storage));
    return true;
}

template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE>
bool RouterBridge<MAX_CHANNELS, RX_MAX_PDU_SIZE>::addRoute(BridgeRoute const& route)
{
    if (m_initialized)
    {
        return false;
    }
    m_routes.push_back(route);
    return true;
}

template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE>
void RouterBridge<MAX_CHANNELS, RX_MAX_PDU_SIZE>::clear()
{
    m_channels.clear();
    m_routes.clear();
    m_routingTable         = ::routing::PduRoutingTable{};
    m_destinations.clear();
    m_destinationOffsets.clear();
    m_outputMessageIds.clear();
    m_nextPduId   = 0U;
    m_errorCount  = 0U;
    m_routedCount = 0U;
    m_initialized = false;
    for (uint8_t i = 0U; i < MAX_CHANNELS; ++i)
    {
        m_readers[i] = nullptr;
        m_writers[i] = nullptr;
    }
}

template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE>
bool RouterBridge<MAX_CHANNELS, RX_MAX_PDU_SIZE>::init(::etl::span<::io::IReader*> readers,
                                                       ::etl::span<::io::IWriter*> writers)
{
    if (m_initialized || m_channels.empty() || m_routes.empty())
    {
        return false;
    }
    if ((readers.size() < MAX_CHANNELS) || (writers.size() < MAX_CHANNELS))
    {
        return false;
    }
    if (!resolveRoutes())
    {
        return false;
    }
    for (auto& storage : m_channels)
    {
        if ((readers[storage.channelIndex] == nullptr)
            || (writers[storage.channelIndex] == nullptr))
        {
            return false;
        }
        storage.rxMessageIds.clear();
        storage.rxMessageLengths.clear();
        storage.rxPduLengthOffsets.clear();
        storage.rxPduLengths.clear();
        storage.rxPduOffsets.clear();
        uint32_t pduBase = 0U;
        for (auto const& message : storage.rxMessages)
        {
            storage.rxMessageIds.push_back(message.messageId);
            storage.rxMessageLengths.push_back(message.messageLength);
            storage.rxPduLengthOffsets.push_back(pduBase);
            for (auto const& pdu : message.pdus)
            {
                storage.rxPduLengths.push_back(pdu.length);
                storage.rxPduOffsets.push_back(pdu.offset);
                pduBase++;
            }
        }
        storage.rxPduLengthOffsets.push_back(pduBase);
        storage.rxTable.firstId           = storage.firstPduId;
        storage.rxTable.messageIds        = storage.rxMessageIds;
        storage.rxTable.messageLengths    = storage.rxMessageLengths;
        storage.rxTable.pduLengthsOffsets = storage.rxPduLengthOffsets;
        storage.rxTable.pduLengths        = storage.rxPduLengths;
        storage.rxTable.pduOffsets        = storage.rxPduOffsets;

        storage.txMessageIds.clear();
        storage.txMessageLengths.clear();
        storage.txPduOffsets.clear();
        for (auto const& message : storage.txMessages)
        {
            storage.txMessageIds.push_back(message.messageId);
            storage.txMessageLengths.push_back(message.messageLength);
            storage.txPduOffsets.push_back(message.pduOffset);
        }
        storage.txTable.messageIds     = storage.txMessageIds;
        storage.txTable.messageLengths = storage.txMessageLengths;
        storage.txTable.pduOffsets     = storage.txPduOffsets;

        typename ::routing::ErrorHandler::Function errorFn
            = ::routing::ErrorHandler::Function::create<RouterBridge,
                                                        &RouterBridge::onRoutingError>(*this);
        storage.rxAdapter = std::unique_ptr<::routing::LegacyRxAdapter<RX_MAX_PDU_SIZE>>(
            new ::routing::LegacyRxAdapter<RX_MAX_PDU_SIZE>(
                *readers[storage.channelIndex], storage.rxTable,
                ::routing::ErrorHandler(errorFn, storage.channelIndex)));
        storage.txAdapter = std::unique_ptr<::routing::LegacyTxAdapter>(
            new ::routing::LegacyTxAdapter(*writers[storage.channelIndex], storage.txTable,
                                           ::routing::ErrorHandler(errorFn, storage.channelIndex)));
        m_readers[storage.channelIndex] = storage.rxAdapter.get();
        m_writers[storage.channelIndex] = storage.txAdapter.get();
    }
    m_router.init(m_routingTable, ::etl::span<::io::IReader*>(m_readers, MAX_CHANNELS),
                  ::etl::span<::io::IWriter*>(m_writers, MAX_CHANNELS));
    m_initialized = true;
    return true;
}

template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE>
bool RouterBridge<MAX_CHANNELS, RX_MAX_PDU_SIZE>::resolveRoutes()
{
    // Per-global-PDU route lists.
    std::vector<std::vector<std::pair<uint8_t, uint32_t>>> perPdu(m_nextPduId);
    for (auto const& route : m_routes)
    {
        ChannelStorage const* src = nullptr;
        ChannelStorage const* dst = nullptr;
        for (auto const& storage : m_channels)
        {
            if (storage.channelIndex == route.srcChannel)
            {
                src = &storage;
            }
            if (storage.channelIndex == route.dstChannel)
            {
                dst = &storage;
            }
        }
        if ((src == nullptr) || (dst == nullptr))
        {
            return false;
        }
        uint32_t globalId  = src->firstPduId;
        bool srcFound      = false;
        for (auto const& message : src->rxMessages)
        {
            if (message.messageId == route.srcMessage)
            {
                if (route.srcPduIndex >= message.pdus.size())
                {
                    return false;
                }
                globalId += static_cast<uint32_t>(route.srcPduIndex);
                srcFound = true;
                break;
            }
            globalId += static_cast<uint32_t>(message.pdus.size());
        }
        if (!srcFound)
        {
            return false;
        }
        bool dstFound = false;
        for (auto const& message : dst->txMessages)
        {
            if (message.messageId == route.dstMessage)
            {
                dstFound = true;
                perPdu[globalId].push_back(
                    std::make_pair(route.dstChannel, message.messageId));
                break;
            }
        }
        if (!dstFound)
        {
            return false;
        }
    }
    m_destinations.clear();
    m_destinationOffsets.clear();
    m_outputMessageIds.clear();
    for (uint32_t pdu = 0U; pdu < m_nextPduId; ++pdu)
    {
        m_destinationOffsets.push_back(static_cast<uint32_t>(m_destinations.size()));
        for (auto const& destination : perPdu[pdu])
        {
            m_destinations.push_back(destination.first);
            m_outputMessageIds.push_back(destination.second);
        }
    }
    m_destinationOffsets.push_back(static_cast<uint32_t>(m_destinations.size()));
    m_routingTable.destinations       = m_destinations;
    m_routingTable.destinationOffsets = m_destinationOffsets;
    m_routingTable.outputMessageIds   = m_outputMessageIds;
    return true;
}

template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE>
bool RouterBridge<MAX_CHANNELS, RX_MAX_PDU_SIZE>::run()
{
    if (!m_initialized)
    {
        return false;
    }
    bool routed = false;
    for (uint32_t i = 0U; i < 32U; ++i)
    {
        if (!m_router.run())
        {
            break;
        }
        m_routedCount++;
        routed = true;
    }
    return routed;
}

template <uint8_t MAX_CHANNELS, size_t RX_MAX_PDU_SIZE>
void RouterBridge<MAX_CHANNELS, RX_MAX_PDU_SIZE>::onRoutingError(
    ::routing::ErrorHandler::StatusCode /* status */, uint8_t /* channelId */,
    uint32_t /* messageId */)
{
    m_errorCount++;
}

} // namespace gwbridge
