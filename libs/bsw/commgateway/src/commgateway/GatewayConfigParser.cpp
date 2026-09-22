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
#include "commgateway/GatewayConfigParser.h"

#include "commgateway/CommStateManager.h"
#include "commgateway/SignalGateway.h"
#include "commgateway/TimeoutMonitor.h"

#include <cstdlib>
#include <map>
#include <vector>

namespace commgateway
{
namespace
{

struct JsonValue
{
    enum class Type
    {
        NUL,
        BOOL,
        NUM,
        STR,
        ARR,
        OBJ
    };

    Type type = Type::NUL;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser
{
public:
    explicit JsonParser(std::string const& text) : m_text(text) {}

    bool parse(JsonValue& out, std::string& error)
    {
        skipWs();
        if (!parseValue(out, error))
        {
            return false;
        }
        skipWs();
        if (m_pos != m_text.size())
        {
            error = "trailing characters after JSON document";
            return false;
        }
        return true;
    }

private:
    void skipWs()
    {
        while ((m_pos < m_text.size())
               && ((m_text[m_pos] == ' ') || (m_text[m_pos] == '\t') || (m_text[m_pos] == '\n')
                   || (m_text[m_pos] == '\r')))
        {
            m_pos++;
        }
    }

    bool parseValue(JsonValue& out, std::string& error)
    {
        if (m_pos >= m_text.size())
        {
            error = "unexpected end of input";
            return false;
        }
        char c = m_text[m_pos];
        if (c == '{')
        {
            return parseObject(out, error);
        }
        if (c == '[')
        {
            return parseArray(out, error);
        }
        if (c == '"')
        {
            return parseString(out, error);
        }
        if ((c == '-') || ((c >= '0') && (c <= '9')))
        {
            return parseNumber(out, error);
        }
        return parseLiteral(out, error);
    }

    bool parseObject(JsonValue& out, std::string& error)
    {
        out.type = JsonValue::Type::OBJ;
        m_pos++; // {
        skipWs();
        if ((m_pos < m_text.size()) && (m_text[m_pos] == '}'))
        {
            m_pos++;
            return true;
        }
        while (true)
        {
            skipWs();
            JsonValue key{};
            if (!parseString(key, error))
            {
                return false;
            }
            skipWs();
            if ((m_pos >= m_text.size()) || (m_text[m_pos] != ':'))
            {
                error = "expected ':' in object";
                return false;
            }
            m_pos++;
            skipWs();
            JsonValue value{};
            if (!parseValue(value, error))
            {
                return false;
            }
            out.object[key.str] = value;
            skipWs();
            if (m_pos >= m_text.size())
            {
                error = "unterminated object";
                return false;
            }
            if (m_text[m_pos] == ',')
            {
                m_pos++;
                continue;
            }
            if (m_text[m_pos] == '}')
            {
                m_pos++;
                return true;
            }
            error = "expected ',' or '}' in object";
            return false;
        }
    }

    bool parseArray(JsonValue& out, std::string& error)
    {
        out.type = JsonValue::Type::ARR;
        m_pos++; // [
        skipWs();
        if ((m_pos < m_text.size()) && (m_text[m_pos] == ']'))
        {
            m_pos++;
            return true;
        }
        while (true)
        {
            skipWs();
            JsonValue value{};
            if (!parseValue(value, error))
            {
                return false;
            }
            out.array.push_back(value);
            skipWs();
            if (m_pos >= m_text.size())
            {
                error = "unterminated array";
                return false;
            }
            if (m_text[m_pos] == ',')
            {
                m_pos++;
                continue;
            }
            if (m_text[m_pos] == ']')
            {
                m_pos++;
                return true;
            }
            error = "expected ',' or ']' in array";
            return false;
        }
    }

    bool parseString(JsonValue& out, std::string& error)
    {
        out.type = JsonValue::Type::STR;
        m_pos++; // opening quote
        while (m_pos < m_text.size())
        {
            char c = m_text[m_pos++];
            if (c == '"')
            {
                return true;
            }
            if (c == '\\')
            {
                if (m_pos >= m_text.size())
                {
                    break;
                }
                char e = m_text[m_pos++];
                switch (e)
                {
                    case '"': out.str += '"'; break;
                    case '\\': out.str += '\\'; break;
                    case 'n': out.str += '\n'; break;
                    case 't': out.str += '\t'; break;
                    default: out.str += e; break;
                }
                continue;
            }
            out.str += c;
        }
        error = "unterminated string";
        return false;
    }

    bool parseNumber(JsonValue& out, std::string& error)
    {
        char const* begin = m_text.c_str() + m_pos;
        char* end         = nullptr;
        out.number        = std::strtod(begin, &end);
        if (end == begin)
        {
            error = "invalid number";
            return false;
        }
        m_pos   = static_cast<size_t>(end - m_text.c_str());
        out.type = JsonValue::Type::NUM;
        return true;
    }

    bool parseLiteral(JsonValue& out, std::string& error)
    {
        if (m_text.compare(m_pos, 4, "true") == 0)
        {
            out.type    = JsonValue::Type::BOOL;
            out.boolean = true;
            m_pos += 4;
            return true;
        }
        if (m_text.compare(m_pos, 5, "false") == 0)
        {
            out.type    = JsonValue::Type::BOOL;
            out.boolean = false;
            m_pos += 5;
            return true;
        }
        if (m_text.compare(m_pos, 4, "null") == 0)
        {
            out.type = JsonValue::Type::NUL;
            m_pos += 4;
            return true;
        }
        error = "invalid value";
        return false;
    }

    std::string const& m_text;
    size_t m_pos = 0U;
};

JsonValue const* findMember(JsonValue const& obj, char const* key)
{
    if (obj.type != JsonValue::Type::OBJ)
    {
        return nullptr;
    }
    auto it = obj.object.find(key);
    return (it != obj.object.end()) ? &it->second : nullptr;
}

bool asUint(JsonValue const& value, uint32_t& out)
{
    if (value.type != JsonValue::Type::NUM)
    {
        return false;
    }
    out = static_cast<uint32_t>(value.number);
    return true;
}

bool parseRoutingRule(JsonValue const& entry, SignalGateway& gateway, TimeoutMonitor& timeouts,
                      std::string& error)
{
    JsonValue const* name = findMember(entry, "signalName");
    if ((name == nullptr) || (name->type != JsonValue::Type::STR))
    {
        error = "signalRouting entry without signalName";
        return false;
    }
    SignalRoutingRule rule{};
    rule.signalName = name->str;

    JsonValue const* destinations = findMember(entry, "destinations");
    if (destinations != nullptr)
    {
        if (destinations->type != JsonValue::Type::ARR)
        {
            error = "destinations of '" + rule.signalName + "' is not an array";
            return false;
        }
        for (JsonValue const& dest : destinations->array)
        {
            JsonValue const* type = findMember(dest, "channelType");
            JsonValue const* id   = findMember(dest, "channelId");
            uint32_t channelType  = 0U;
            uint32_t channelId    = 0U;
            if ((type == nullptr) || !asUint(*type, channelType) || (id == nullptr)
                || !asUint(*id, channelId))
            {
                error = "destination of '" + rule.signalName + "' needs channelType/channelId";
                return false;
            }
            rule.destinations.push_back(
                std::make_pair(static_cast<uint8_t>(channelType), channelId));
        }
    }

    JsonValue const* transform = findMember(entry, "transform");
    if (transform != nullptr)
    {
        if (transform->type != JsonValue::Type::OBJ)
        {
            error = "transform of '" + rule.signalName + "' is not an object";
            return false;
        }
        double scale  = 1.0;
        double offset = 0.0;
        JsonValue const* scaleValue  = findMember(*transform, "scale");
        JsonValue const* offsetValue = findMember(*transform, "offset");
        if ((scaleValue != nullptr) && (scaleValue->type == JsonValue::Type::NUM))
        {
            scale = scaleValue->number;
        }
        if ((offsetValue != nullptr) && (offsetValue->type == JsonValue::Type::NUM))
        {
            offset = offsetValue->number;
        }
        rule.transform = [scale, offset](double v) { return (v * scale) + offset; };
    }

    JsonValue const* interval = findMember(entry, "minIntervalMs");
    if ((interval != nullptr) && !asUint(*interval, rule.minIntervalMs))
    {
        error = "minIntervalMs of '" + rule.signalName + "' is not a number";
        return false;
    }

    gateway.registerRoutingRule(rule);

    JsonValue const* timeout = findMember(entry, "timeoutMs");
    if (timeout != nullptr)
    {
        uint32_t timeoutMs = 0U;
        if (!asUint(*timeout, timeoutMs))
        {
            error = "timeoutMs of '" + rule.signalName + "' is not a number";
            return false;
        }
        timeouts.registerSignalTimeout(rule.signalName, timeoutMs, nullptr);
    }
    return true;
}

bool parseFrameTimeout(JsonValue const& entry, TimeoutMonitor& timeouts, std::string& error)
{
    JsonValue const* type    = findMember(entry, "channelType");
    JsonValue const* id      = findMember(entry, "channelId");
    JsonValue const* frame   = findMember(entry, "frameId");
    JsonValue const* timeout = findMember(entry, "timeoutMs");
    uint32_t channelType = 0U;
    uint32_t channelId   = 0U;
    uint32_t frameId     = 0U;
    uint32_t timeoutMs   = 0U;
    if ((type == nullptr) || !asUint(*type, channelType) || (id == nullptr)
        || !asUint(*id, channelId) || (frame == nullptr) || !asUint(*frame, frameId)
        || (timeout == nullptr) || !asUint(*timeout, timeoutMs))
    {
        error = "frameTimeouts entry needs channelType/channelId/frameId/timeoutMs";
        return false;
    }
    timeouts.registerFrameTimeout(static_cast<uint8_t>(channelId), frameId, timeoutMs, nullptr);
    return true;
}

bool parseChannelState(JsonValue const& entry, CommStateManager& states, std::string& error)
{
    JsonValue const* type = findMember(entry, "channelType");
    JsonValue const* id   = findMember(entry, "channelId");
    uint32_t channelType  = 0U;
    uint32_t channelId    = 0U;
    if ((type == nullptr) || !asUint(*type, channelType) || (id == nullptr)
        || !asUint(*id, channelId))
    {
        error = "channelStates entry needs channelType/channelId";
        return false;
    }
    states.registerChannel(static_cast<uint8_t>(channelType), static_cast<uint8_t>(channelId));
    JsonValue const* recovery = findMember(entry, "autoRecovery");
    JsonValue const* recoveryTime = findMember(entry, "recoveryTimeMs");
    uint32_t recoveryTimeMs       = 1000U;
    if ((recoveryTime != nullptr) && !asUint(*recoveryTime, recoveryTimeMs))
    {
        error = "recoveryTimeMs is not a number";
        return false;
    }
    bool autoRecovery = (recovery != nullptr) && (recovery->type == JsonValue::Type::BOOL)
                        && recovery->boolean;
    states.setRecoveryConfig(static_cast<uint8_t>(channelType), static_cast<uint8_t>(channelId),
                             autoRecovery, recoveryTimeMs);
    return true;
}

} // namespace

bool GatewayConfigParser::parse(std::string const& jsonText, SignalGateway& gateway,
                                TimeoutMonitor& timeoutMonitor, CommStateManager& stateManager,
                                std::string& error)
{
    JsonParser parser(jsonText);
    JsonValue root{};
    if (!parser.parse(root, error))
    {
        return false;
    }
    if (root.type != JsonValue::Type::OBJ)
    {
        error = "root element is not an object";
        return false;
    }

    JsonValue const* routing = findMember(root, "signalRouting");
    if (routing != nullptr)
    {
        if (routing->type != JsonValue::Type::ARR)
        {
            error = "signalRouting is not an array";
            return false;
        }
        for (JsonValue const& entry : routing->array)
        {
            if (!parseRoutingRule(entry, gateway, timeoutMonitor, error))
            {
                return false;
            }
        }
    }

    JsonValue const* frameTimeouts = findMember(root, "frameTimeouts");
    if (frameTimeouts != nullptr)
    {
        if (frameTimeouts->type != JsonValue::Type::ARR)
        {
            error = "frameTimeouts is not an array";
            return false;
        }
        for (JsonValue const& entry : frameTimeouts->array)
        {
            if (!parseFrameTimeout(entry, timeoutMonitor, error))
            {
                return false;
            }
        }
    }

    JsonValue const* channelStates = findMember(root, "channelStates");
    if (channelStates != nullptr)
    {
        if (channelStates->type != JsonValue::Type::ARR)
        {
            error = "channelStates is not an array";
            return false;
        }
        for (JsonValue const& entry : channelStates->array)
        {
            if (!parseChannelState(entry, stateManager, error))
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace commgateway
