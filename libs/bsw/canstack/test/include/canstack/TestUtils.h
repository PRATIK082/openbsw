/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef CANSTACK_TEST_UTILS_H
#define CANSTACK_TEST_UTILS_H

#include "canstack/CanChannel.h"
#include "canstack/CanHwStub.h"
#include "canstack/SignalDb.h"

#include <cstdint>
#include <string>
#include <vector>

namespace canstack
{
namespace testutils
{

/// Small demo database mirroring the module documentation: one frame with two
/// signals on channel 0.
SignalTableEntry const DEMO_SIGNALS[]
    = {{"EngineSpeed", 0x100U, 7U, 16U, true, 0.25, 0.0, 0.0, 8000.0, "rpm", 0U, false, -1, false},
       {"VehicleSpeed", 0x100U, 23U, 8U, true, 0.5, 0.0, 0.0, 250.0, "kph", 0U, false, -1, false},
       {"TargetSpeed", 0x200U, 0U, 16U, false, 0.1, 0.0, 0.0, 1000.0, "kph", 0U, false, -1, false},
       {"BatteryCurrent", 0x300U, 0U, 10U, false, 0.1, 0.0, -50.0, 50.0, "A", 0U, false, -1,
        true}};

FrameTableEntry const DEMO_FRAMES[]
    = {{0x100U, "EngineData", 8U, 0U}, {0x200U, "TargetData", 8U, 0U}, {0x300U, "BatteryData", 8U, 0U}};

size_t const DEMO_SIGNAL_COUNT = sizeof(DEMO_SIGNALS) / sizeof(DEMO_SIGNALS[0]);
size_t const DEMO_FRAME_COUNT  = sizeof(DEMO_FRAMES) / sizeof(DEMO_FRAMES[0]);

/// One channel wired to a loopback stub; lets tests receive what they send.
struct LoopbackChannel
{
    CanHwStub hw;
    CanChannelConfig config;
    CanChannel channel;

    explicit LoopbackChannel(uint8_t channelId, uint32_t baudrate = 500000U)
    : hw(), config(), channel()
    {
        hw.setLoopback(true);
        config.channelId   = channelId;
        config.hwDriver     = &hw;
        config.baudrate    = baudrate;
        config.maxMailboxes = 2U;
        (void)channel.init(config);
    }
};

/// Records (channelId, frameId, dlc, data) tuples like an upper layer would.
class FrameSink
{
public:
    struct Entry
    {
        uint8_t channelId;
        uint32_t frameId;
        uint8_t dlc;
        std::vector<uint8_t> data;
    };

    void connect(CanChannel& channel)
    {
        channel.registerUpperLayerCallback([this](
                                               uint8_t channelId, uint32_t frameId, uint8_t dlc,
                                               uint8_t const* data) {
            Entry entry;
            entry.channelId = channelId;
            entry.frameId   = frameId;
            entry.dlc       = dlc;
            for (uint8_t i = 0U; i < dlc; ++i)
            {
                entry.data.push_back(data[i]);
            }
            m_entries.push_back(entry);
        });
    }

    std::vector<Entry> const& entries() const { return m_entries; }

    void clear() { m_entries.clear(); }

private:
    std::vector<Entry> m_entries;
};

} // namespace testutils
} // namespace canstack

#endif // CANSTACK_TEST_UTILS_H
