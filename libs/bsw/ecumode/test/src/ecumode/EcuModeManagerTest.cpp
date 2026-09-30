/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "ecumode/CommStateManagerPhase5.h"
#include "ecumode/EcuMode.h"

#include <gmock/gmock.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
using namespace ::testing;

using EcuMode           = ::ecumode::EcuMode;
using FrameFilterPolicy = ::ecumode::FrameFilterPolicy;
using Manager           = ::ecumode::CommStateManagerPhase5;

/**
 * \desc: Fixture resetting the singleton before and after each test.
 */
class EcuModeManagerTest : public Test
{
protected:
    void SetUp() override { manager().clear(); }

    void TearDown() override { manager().clear(); }

    static Manager& manager() { return Manager::getInstance(); }

    // Moves the manager into the given mode (applies the pending request).
    static void enterMode(Manager& mgr, EcuMode mode)
    {
        mgr.requestEcuMode(mode);
        mgr.mainFunction(1U);
    }

    // Inline equivalent of config/ecumode_rules.json (kept in sync manually).
    static std::string const& repoConfigJson()
    {
        static std::string const json = R"json({
    "modes": {
        "NORMAL": {
            "default": "ALLOW"
        },
        "STANDBY": {
            "default": "BLOCK_RX",
            "exceptions": [
                {"frameId": 2016, "channelType": 0, "policy": "ALLOW"},
                {"frameId": 2047, "channelType": 0, "policy": "ALLOW"}
            ]
        },
        "SLEEP": {
            "default": "BLOCK_BOTH",
            "exceptions": [
                {"frameId": 0, "channelType": 0, "policy": "ALLOW"}
            ]
        },
        "DIAGNOSTIC": {
            "default": "BLOCK_RX",
            "exceptions": [
                {"frameId": 2016, "channelType": 0, "policy": "ALLOW"},
                {"frameId": 2047, "channelType": 0, "policy": "ALLOW"},
                {"frameId": 1280, "channelType": 0, "policy": "ALLOW"}
            ]
        },
        "TRANSPORT": {
            "default": "BLOCK_BOTH",
            "exceptions": [
                {"frameId": 2016, "channelType": 0, "policy": "ALLOW"}
            ]
        },
        "MANUFACTURING": {
            "default": "ALLOW"
        }
    },
    "actions": {
        "NORMAL": ["START_SIGNAL_GATEWAY"],
        "STANDBY": ["STOP_SIGNAL_GATEWAY"],
        "SLEEP": ["STOP_SIGNAL_GATEWAY", "SUSPEND_COMMUNICATION"],
        "DIAGNOSTIC": ["STOP_SIGNAL_GATEWAY", "ENABLE_DIAGNOSTIC_SESSION"],
        "TRANSPORT": ["STOP_SIGNAL_GATEWAY", "ENABLE_TRANSPORT_MODE"],
        "MANUFACTURING": ["START_SIGNAL_GATEWAY", "ENABLE_MANUFACTURING_MODE"]
    }
})json";
        return json;
    }
};

/**
 * \desc: Fresh manager starts in UNINIT and allows all frames by default.
 */
TEST_F(EcuModeManagerTest, initial_mode_uninit_and_default_allow)
{
    Manager& mgr = manager();
    EXPECT_TRUE(mgr.getCurrentEcuMode() == EcuMode::UNINIT);

    FrameFilterPolicy policy = mgr.getFrameFilterPolicy(256U, 0U);
    EXPECT_TRUE(policy == FrameFilterPolicy::ALLOW);

    EXPECT_TRUE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(256U, 0U, true));
    EXPECT_EQ(0U, mgr.getBlockCount());
}

/**
 * \desc: Requesting the current mode is a no-op (no pending, no callback).
 */
TEST_F(EcuModeManagerTest, request_same_mode_is_noop)
{
    Manager& mgr           = manager();
    uint32_t callbackCalls = 0U;
    mgr.setModeChangeCallback([&callbackCalls](EcuMode, EcuMode) { callbackCalls++; });

    mgr.requestEcuMode(EcuMode::UNINIT); // already UNINIT
    mgr.mainFunction(10U);

    EXPECT_TRUE(mgr.getCurrentEcuMode() == EcuMode::UNINIT);
    EXPECT_EQ(0U, callbackCalls);
}

/**
 * \desc: A requested mode takes effect only after mainFunction(t).
 */
TEST_F(EcuModeManagerTest, pending_mode_applied_only_after_main_function)
{
    Manager& mgr = manager();
    mgr.requestEcuMode(EcuMode::NORMAL);

    EXPECT_TRUE(mgr.getCurrentEcuMode() == EcuMode::UNINIT); // not applied yet

    mgr.mainFunction(5U);
    EXPECT_TRUE(mgr.getCurrentEcuMode() == EcuMode::NORMAL);
}

/**
 * \desc: The mode-change callback fires exactly once with (from, to).
 */
TEST_F(EcuModeManagerTest, mode_change_callback_fires_once_with_from_to)
{
    Manager& mgr     = manager();
    uint32_t calls   = 0U;
    EcuMode seenFrom = EcuMode::SLEEP;
    EcuMode seenTo   = EcuMode::SLEEP;
    mgr.setModeChangeCallback(
        [&calls, &seenFrom, &seenTo](EcuMode from, EcuMode to)
        {
            calls++;
            seenFrom = from;
            seenTo   = to;
        });

    mgr.requestEcuMode(EcuMode::NORMAL);
    mgr.mainFunction(1U);
    mgr.mainFunction(2U); // no pending: must not fire again

    EXPECT_EQ(1U, calls);
    EXPECT_TRUE(seenFrom == EcuMode::UNINIT);
    EXPECT_TRUE(seenTo == EcuMode::NORMAL);
}

/**
 * \desc: STANDBY blocks ordinary RX but lets diagnostic frames through.
 */
TEST_F(EcuModeManagerTest, standby_rx_filtering)
{
    Manager& mgr = manager();
    mgr.setDefaultPolicy(EcuMode::STANDBY, FrameFilterPolicy::BLOCK_RX);
    mgr.addException(EcuMode::STANDBY, 2016U, 0U, FrameFilterPolicy::ALLOW);
    mgr.addException(EcuMode::STANDBY, 2047U, 0U, FrameFilterPolicy::ALLOW);
    enterMode(mgr, EcuMode::STANDBY);

    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(2016U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(2047U, 0U, false));
}

/**
 * \desc: SLEEP blocks traffic except the wake-up frame (id 0).
 */
TEST_F(EcuModeManagerTest, sleep_blocks_all_except_wakeup)
{
    Manager& mgr = manager();
    mgr.setDefaultPolicy(EcuMode::SLEEP, FrameFilterPolicy::BLOCK_BOTH);
    mgr.addException(EcuMode::SLEEP, 0U, 0U, FrameFilterPolicy::ALLOW);
    enterMode(mgr, EcuMode::SLEEP);

    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, true));
    EXPECT_FALSE(mgr.isFrameAllowed(2016U, 0U, false));
    EXPECT_FALSE(mgr.isFrameAllowed(2016U, 0U, true));
    EXPECT_TRUE(mgr.isFrameAllowed(0U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(0U, 0U, true));
}

/**
 * \desc: With a TX-blocking DIAGNOSTIC default, ordinary TX is denied but
 * \desc: the diagnostic exception frame passes.
 *
 * NOTE: the repo JSON ships DIAGNOSTIC with default BLOCK_RX (RX-focused),
 * under which TX of 256 would be allowed. This test configures BLOCK_TX
 * explicitly to cover the TX-blocking semantic.
 */
TEST_F(EcuModeManagerTest, diagnostic_tx_filtering)
{
    Manager& mgr = manager();
    mgr.setDefaultPolicy(EcuMode::DIAGNOSTIC, FrameFilterPolicy::BLOCK_TX);
    mgr.addException(EcuMode::DIAGNOSTIC, 2016U, 0U, FrameFilterPolicy::ALLOW);
    enterMode(mgr, EcuMode::DIAGNOSTIC);

    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, true));
    EXPECT_TRUE(mgr.isFrameAllowed(2016U, 0U, true));
}

/**
 * \desc: BLOCK_TX denies TX but allows RX; BLOCK_RX does the opposite.
 */
TEST_F(EcuModeManagerTest, block_tx_and_block_rx_semantics)
{
    Manager& mgr = manager();

    mgr.setDefaultPolicy(EcuMode::NORMAL, FrameFilterPolicy::BLOCK_TX);
    enterMode(mgr, EcuMode::NORMAL);
    EXPECT_TRUE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, true));

    mgr.setDefaultPolicy(EcuMode::NORMAL, FrameFilterPolicy::BLOCK_RX);
    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(256U, 0U, true));
}

/**
 * \desc: The block counter increments once per denied frame.
 */
TEST_F(EcuModeManagerTest, block_counter_increments_on_denials)
{
    Manager& mgr = manager();
    mgr.setDefaultPolicy(EcuMode::SLEEP, FrameFilterPolicy::BLOCK_BOTH);
    mgr.addException(EcuMode::SLEEP, 0U, 0U, FrameFilterPolicy::ALLOW);
    enterMode(mgr, EcuMode::SLEEP);

    EXPECT_EQ(0U, mgr.getBlockCount());
    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_EQ(1U, mgr.getBlockCount());
    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, true));
    EXPECT_EQ(2U, mgr.getBlockCount());
    EXPECT_TRUE(mgr.isFrameAllowed(0U, 0U, false)); // allowed: no increment
    EXPECT_EQ(2U, mgr.getBlockCount());
}

/**
 * \desc: Registered action handlers run on entering their mode; unknown
 * \desc: action names are skipped without failure.
 */
TEST_F(EcuModeManagerTest, action_handlers_run_and_unknown_skipped)
{
    Manager& mgr     = manager();
    uint32_t handled = 0U;
    EcuMode seenMode = EcuMode::UNINIT;
    mgr.registerActionHandler(
        "STOP_SIGNAL_GATEWAY",
        [&handled, &seenMode](EcuMode mode)
        {
            handled++;
            seenMode = mode;
        });

    std::vector<std::string> actions;
    actions.push_back("STOP_SIGNAL_GATEWAY");
    actions.push_back("NEVER_REGISTERED_NAME");
    mgr.setModeActions(EcuMode::STANDBY, actions);

    enterMode(mgr, EcuMode::STANDBY);

    EXPECT_EQ(1U, handled);
    EXPECT_TRUE(seenMode == EcuMode::STANDBY);
}

/**
 * \desc: Loading the repo config populates STANDBY/SLEEP/DIAGNOSTIC policies.
 */
TEST_F(EcuModeManagerTest, load_config_populates_policies)
{
    Manager& mgr = manager();
    std::string error;
    bool ok = mgr.loadConfig(repoConfigJson(), error);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(error.empty());

    enterMode(mgr, EcuMode::STANDBY);
    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(2016U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(2047U, 0U, false));

    enterMode(mgr, EcuMode::SLEEP);
    EXPECT_FALSE(mgr.isFrameAllowed(256U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(0U, 0U, false));

    enterMode(mgr, EcuMode::DIAGNOSTIC);
    EXPECT_TRUE(mgr.isFrameAllowed(2016U, 0U, false));
    EXPECT_TRUE(mgr.isFrameAllowed(1280U, 0U, false));
}

/**
 * \desc: Loading the repo config wires mode actions to registered handlers.
 */
TEST_F(EcuModeManagerTest, load_config_populates_actions)
{
    Manager& mgr = manager();
    std::string error;
    bool ok = mgr.loadConfig(repoConfigJson(), error);
    EXPECT_TRUE(ok);

    uint32_t handled = 0U;
    mgr.registerActionHandler("STOP_SIGNAL_GATEWAY", [&handled](EcuMode) { handled++; });

    enterMode(mgr, EcuMode::STANDBY); // action list names STOP_SIGNAL_GATEWAY
    EXPECT_EQ(1U, handled);
}

/**
 * \desc: Malformed JSON fails with an error and leaves state unchanged.
 */
TEST_F(EcuModeManagerTest, malformed_json_leaves_state_unchanged)
{
    Manager& mgr = manager();
    mgr.setDefaultPolicy(EcuMode::NORMAL, FrameFilterPolicy::ALLOW);
    enterMode(mgr, EcuMode::NORMAL);
    FrameFilterPolicy before = mgr.getFrameFilterPolicy(256U, 0U);
    EXPECT_TRUE(before == FrameFilterPolicy::ALLOW);

    std::string error;
    bool ok = mgr.loadConfig("{ not valid json", error);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(error.empty());

    EXPECT_TRUE(mgr.getCurrentEcuMode() == EcuMode::NORMAL);
    FrameFilterPolicy after = mgr.getFrameFilterPolicy(256U, 0U);
    EXPECT_TRUE(after == before);
    EXPECT_TRUE(mgr.isFrameAllowed(256U, 0U, false));
}

/**
 * \desc: Unknown mode/policy names fail validation without touching state.
 */
TEST_F(EcuModeManagerTest, unknown_names_leave_state_unchanged)
{
    Manager& mgr = manager();
    mgr.setDefaultPolicy(EcuMode::NORMAL, FrameFilterPolicy::ALLOW);
    enterMode(mgr, EcuMode::NORMAL);

    std::string error;
    std::string const badMode = "{\"modes\": {\"BOGUS\": {\"default\": \"ALLOW\"}}}";
    bool ok                   = mgr.loadConfig(badMode, error);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(error.empty());

    std::string error2;
    std::string const badPolicy = "{\"modes\": {\"NORMAL\": {\"default\": \"BOGUS\"}}}";
    bool ok2                    = mgr.loadConfig(badPolicy, error2);
    EXPECT_FALSE(ok2);
    EXPECT_FALSE(error2.empty());

    FrameFilterPolicy after = mgr.getFrameFilterPolicy(256U, 0U);
    EXPECT_TRUE(after == FrameFilterPolicy::ALLOW);
}

/**
 * \desc: parseEcuMode accepts known names and rejects unknown text.
 */
TEST_F(EcuModeManagerTest, parse_ecu_mode_names)
{
    EcuMode mode = EcuMode::UNINIT;
    EXPECT_TRUE(::ecumode::parseEcuMode("NORMAL", mode));
    EXPECT_TRUE(mode == EcuMode::NORMAL);
    EXPECT_TRUE(::ecumode::parseEcuMode("STANDBY", mode));
    EXPECT_TRUE(mode == EcuMode::STANDBY);
    EXPECT_FALSE(::ecumode::parseEcuMode("BOGUS", mode));
}

/**
 * \desc: parseFrameFilterPolicy accepts known names and rejects unknown text.
 */
TEST_F(EcuModeManagerTest, parse_frame_filter_policy_names)
{
    FrameFilterPolicy policy = FrameFilterPolicy::ALLOW;
    EXPECT_TRUE(::ecumode::parseFrameFilterPolicy("NORMAL", policy) == false); // mode, not policy
    EXPECT_TRUE(::ecumode::parseFrameFilterPolicy("ALLOW", policy));
    EXPECT_TRUE(policy == FrameFilterPolicy::ALLOW);
    EXPECT_TRUE(::ecumode::parseFrameFilterPolicy("BLOCK_RX", policy));
    EXPECT_TRUE(policy == FrameFilterPolicy::BLOCK_RX);
    EXPECT_FALSE(::ecumode::parseFrameFilterPolicy("BOGUS", policy));
}

/**
 * \desc: Local forwarder lambdas consulting isFrameAllowed prove the
 * \desc: gateway wiring pattern (drop RX / block TX) without touching
 * \desc: FrameGateway/SignalGateway production code.
 */
TEST_F(EcuModeManagerTest, gateway_forwarding_pattern)
{
    Manager& mgr = manager();
    mgr.setDefaultPolicy(EcuMode::STANDBY, FrameFilterPolicy::BLOCK_RX);
    mgr.addException(EcuMode::STANDBY, 2016U, 0U, FrameFilterPolicy::ALLOW);
    enterMode(mgr, EcuMode::STANDBY);

    uint32_t forwardedRx = 0U;
    uint32_t droppedRx   = 0U;
    uint32_t forwardedTx = 0U;
    uint32_t blockedTx   = 0U;

    auto forwardRx = [&mgr, &forwardedRx, &droppedRx](uint32_t frameId)
    {
        if (!mgr.isFrameAllowed(frameId, 0U, false))
        {
            droppedRx++;
            return;
        }
        forwardedRx++;
    };
    auto forwardTx = [&mgr, &forwardedTx, &blockedTx](uint32_t frameId)
    {
        if (!mgr.isFrameAllowed(frameId, 0U, true))
        {
            blockedTx++;
            return;
        }
        forwardedTx++;
    };

    forwardRx(256U);  // ordinary RX in STANDBY: dropped
    forwardRx(2016U); // diagnostic exception RX: forwarded
    forwardTx(256U);  // BLOCK_RX allows TX: forwarded
    forwardTx(2016U); // forwarded

    EXPECT_EQ(1U, forwardedRx);
    EXPECT_EQ(1U, droppedRx);
    EXPECT_EQ(2U, forwardedTx);
    EXPECT_EQ(0U, blockedTx);
}

} // namespace
