#include "modearbiter/ModeArbiter.h"

#include <ecumode/EcuMode.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
using namespace ::testing;

using EcuMode           = ::modearbiter::EcuMode;
using FrameFilterPolicy = ::modearbiter::FrameFilterPolicy;
using Manager           = ::modearbiter::ModeArbiter;

Manager& manager()
{
    return Manager::getInstance();
}
}

TEST(ModeArbiterTest, InitialModeUninit)
{
    manager().clear();
    EXPECT_TRUE(manager().getCurrentMode() == EcuMode::UNINIT);
    manager().clear();
}

TEST(ModeArbiterTest, PendingAppliedInMainFunction)
{
    manager().clear();
    manager().requestMode(EcuMode::NORMAL);
    EXPECT_TRUE(manager().getCurrentMode() == EcuMode::UNINIT);
    manager().mainFunction(5U);
    EXPECT_TRUE(manager().getCurrentMode() == EcuMode::NORMAL);
    manager().clear();
}

TEST(ModeArbiterTest, ModeChangeCallback)
{
    manager().clear();
    uint32_t calls = 0U;
    EcuMode seenFrom = EcuMode::SLEEP;
    EcuMode seenTo   = EcuMode::SLEEP;
    manager().setModeChangeCallback([&calls, &seenFrom, &seenTo](EcuMode from, EcuMode to)
    {
        calls++;
        seenFrom = from;
        seenTo   = to;
    });
    manager().requestMode(EcuMode::NORMAL);
    manager().mainFunction(1U);
    manager().mainFunction(2U);
    EXPECT_EQ(1U, calls);
    EXPECT_TRUE(seenFrom == EcuMode::UNINIT);
    EXPECT_TRUE(seenTo == EcuMode::NORMAL);
    manager().clear();
}

TEST(ModeArbiterTest, ActionHandlersRun)
{
    manager().clear();
    uint32_t handled = 0U;
    EcuMode seenMode = EcuMode::UNINIT;
    manager().registerActionHandler("STOP", [&handled, &seenMode](EcuMode mode)
    {
        handled++;
        seenMode = mode;
    });
    std::vector<std::string> actions;
    actions.push_back("STOP");
    actions.push_back("NEVER");
    manager().setModeActions(EcuMode::STANDBY, actions);
    manager().requestMode(EcuMode::STANDBY);
    manager().mainFunction(1U);
    EXPECT_EQ(1U, handled);
    EXPECT_TRUE(seenMode == EcuMode::STANDBY);
    manager().clear();
}

TEST(ModeArbiterTest, LoadConfig)
{
    manager().clear();
    std::string json = R"json({
        "modes": {"NORMAL": {"default": "ALLOW"}},
        "actions": {"NORMAL": ["STOP"]}
    })json";
    std::string error;
    EXPECT_TRUE(manager().loadConfig(json, error));
    EXPECT_TRUE(error.empty());
    manager().clear();
}

TEST(ModeArbiterTest, RulesEvaluate)
{
    manager().clear();
    manager().setCondition("cond", true);
    manager().addRule("r1", "cond", EcuMode::NORMAL);
    manager().evaluateRules();
    EXPECT_TRUE(manager().getCurrentMode() == EcuMode::UNINIT); // pending not applied
    manager().mainFunction(1U);
    EXPECT_TRUE(manager().getCurrentMode() == EcuMode::NORMAL);

    // lambda condition
    manager().clear();
    manager().addRule("r2", []() { return true; }, EcuMode::STANDBY);
    manager().mainFunction(2U);
    EXPECT_TRUE(manager().getCurrentMode() == EcuMode::STANDBY);
    manager().clear();
}

TEST(ModeArbiterTest, RuleNames)
{
    manager().clear();
    manager().addRule("a", []() { return false; }, EcuMode::NORMAL);
    EXPECT_EQ(1U, manager().getRuleCount());
    auto names = manager().getRuleNames();
    EXPECT_EQ(1U, names.size());
    EXPECT_EQ(std::string("a"), names[0]);
    manager().removeRule("a");
    EXPECT_EQ(0U, manager().getRuleCount());
    manager().clear();
}

TEST(ModeArbiterTest, InstanceUsable)
{
    modearbiter::ModeArbiter m;
    m.clear();
    m.requestMode(EcuMode::NORMAL);
    m.mainFunction(1U);
    EXPECT_TRUE(m.getCurrentMode() == EcuMode::NORMAL);
    m.clear();
}
