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
 * \ingroup ecumode
 */

#include <ecumode/CommStateManagerPhase5.h>

#include <cctype>
#include <map>
#include <sstream>

namespace ecumode
{
namespace
{

// Minimal self-contained JSON model (objects, arrays, strings, numbers,
// booleans, null). No third-party dependency.

struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;
};

class JsonParser
{
public:
    explicit JsonParser(std::string const& text) : m_text(text) {}

    bool parse(JsonValue& value, std::string& error)
    {
        skipWhitespace();
        if (!parseValue(value, error))
        {
            return false;
        }
        skipWhitespace();
        if (m_pos != m_text.size())
        {
            error = "unexpected trailing characters";
            return false;
        }
        return true;
    }

private:
    bool parseValue(JsonValue& value, std::string& error)
    {
        if (m_pos >= m_text.size())
        {
            error = "unexpected end of input";
            return false;
        }
        char const c = m_text[m_pos];
        if (c == '{')
        {
            return parseObject(value, error);
        }
        if (c == '[')
        {
            return parseArray(value, error);
        }
        if (c == '"')
        {
            value.type = JsonValue::Type::String;
            return parseString(value.stringValue, error);
        }
        if ((c == '-') || ((c >= '0') && (c <= '9')))
        {
            return parseNumber(value, error);
        }
        if (matchLiteral("true"))
        {
            value.type      = JsonValue::Type::Bool;
            value.boolValue = true;
            return true;
        }
        if (matchLiteral("false"))
        {
            value.type      = JsonValue::Type::Bool;
            value.boolValue = false;
            return true;
        }
        if (matchLiteral("null"))
        {
            value.type = JsonValue::Type::Null;
            return true;
        }
        error = "unexpected character";
        return false;
    }

    bool parseObject(JsonValue& value, std::string& error)
    {
        value.type = JsonValue::Type::Object;
        ++m_pos; // '{'
        skipWhitespace();
        if ((m_pos < m_text.size()) && (m_text[m_pos] == '}'))
        {
            ++m_pos;
            return true;
        }
        while (true)
        {
            skipWhitespace();
            if ((m_pos >= m_text.size()) || (m_text[m_pos] != '"'))
            {
                error = "expected string key in object";
                return false;
            }
            std::string key;
            if (!parseString(key, error))
            {
                return false;
            }
            skipWhitespace();
            if ((m_pos >= m_text.size()) || (m_text[m_pos] != ':'))
            {
                error = "expected ':' in object";
                return false;
            }
            ++m_pos; // ':'
            skipWhitespace();
            JsonValue member;
            if (!parseValue(member, error))
            {
                return false;
            }
            value.objectValue[key] = member;
            skipWhitespace();
            if (m_pos >= m_text.size())
            {
                error = "unterminated object";
                return false;
            }
            if (m_text[m_pos] == ',')
            {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == '}')
            {
                ++m_pos;
                return true;
            }
            error = "expected ',' or '}' in object";
            return false;
        }
    }

    bool parseArray(JsonValue& value, std::string& error)
    {
        value.type = JsonValue::Type::Array;
        ++m_pos; // '['
        skipWhitespace();
        if ((m_pos < m_text.size()) && (m_text[m_pos] == ']'))
        {
            ++m_pos;
            return true;
        }
        while (true)
        {
            skipWhitespace();
            JsonValue element;
            if (!parseValue(element, error))
            {
                return false;
            }
            value.arrayValue.push_back(element);
            skipWhitespace();
            if (m_pos >= m_text.size())
            {
                error = "unterminated array";
                return false;
            }
            if (m_text[m_pos] == ',')
            {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == ']')
            {
                ++m_pos;
                return true;
            }
            error = "expected ',' or ']' in array";
            return false;
        }
    }

    bool parseString(std::string& result, std::string& error)
    {
        ++m_pos; // opening '"'
        std::ostringstream out;
        while (m_pos < m_text.size())
        {
            char const c = m_text[m_pos];
            if (c == '"')
            {
                ++m_pos;
                result = out.str();
                return true;
            }
            if (c == '\\')
            {
                ++m_pos;
                if (m_pos >= m_text.size())
                {
                    break;
                }
                char const e = m_text[m_pos];
                switch (e)
                {
                    case '"': out << '"'; break;
                    case '\\': out << '\\'; break;
                    case '/': out << '/'; break;
                    case 'b': out << '\b'; break;
                    case 'f': out << '\f'; break;
                    case 'n': out << '\n'; break;
                    case 'r': out << '\r'; break;
                    case 't': out << '\t'; break;
                    case 'u':
                    {
                        if ((m_pos + 4U) >= m_text.size())
                        {
                            error = "invalid unicode escape";
                            return false;
                        }
                        unsigned long code = 0UL;
                        for (size_t i = 1U; i <= 4U; ++i)
                        {
                            char const h = m_text[m_pos + i];
                            code *= 16UL;
                            if ((h >= '0') && (h <= '9'))
                            {
                                code += static_cast<unsigned long>(h - '0');
                            }
                            else if ((h >= 'a') && (h <= 'f'))
                            {
                                code += static_cast<unsigned long>(h - 'a') + 10UL;
                            }
                            else if ((h >= 'A') && (h <= 'F'))
                            {
                                code += static_cast<unsigned long>(h - 'A') + 10UL;
                            }
                            else
                            {
                                error = "invalid unicode escape";
                                return false;
                            }
                        }
                        if (code < 0x80UL)
                        {
                            out << static_cast<char>(code);
                        }
                        else if (code < 0x800UL)
                        {
                            out << static_cast<char>(0xC0UL | (code >> 6U));
                            out << static_cast<char>(0x80UL | (code & 0x3FUL));
                        }
                        else
                        {
                            out << static_cast<char>(0xE0UL | (code >> 12U));
                            out << static_cast<char>(0x80UL | ((code >> 6U) & 0x3FUL));
                            out << static_cast<char>(0x80UL | (code & 0x3FUL));
                        }
                        m_pos += 4U;
                        break;
                    }
                    default:
                        error = "invalid escape sequence";
                        return false;
                }
                ++m_pos;
                continue;
            }
            out << c;
            ++m_pos;
        }
        error = "unterminated string";
        return false;
    }

    bool parseNumber(JsonValue& value, std::string& error)
    {
        size_t const start = m_pos;
        if (m_text[m_pos] == '-')
        {
            ++m_pos;
        }
        if (m_pos >= m_text.size())
        {
            error = "invalid number";
            return false;
        }
        if (m_text[m_pos] == '0')
        {
            ++m_pos;
        }
        else if ((m_text[m_pos] >= '1') && (m_text[m_pos] <= '9'))
        {
            while ((m_pos < m_text.size()) && (m_text[m_pos] >= '0') && (m_text[m_pos] <= '9'))
            {
                ++m_pos;
            }
        }
        else
        {
            error = "invalid number";
            return false;
        }
        if ((m_pos < m_text.size()) && (m_text[m_pos] == '.'))
        {
            ++m_pos;
            if ((m_pos >= m_text.size()) || (m_text[m_pos] < '0') || (m_text[m_pos] > '9'))
            {
                error = "invalid number";
                return false;
            }
            while ((m_pos < m_text.size()) && (m_text[m_pos] >= '0') && (m_text[m_pos] <= '9'))
            {
                ++m_pos;
            }
        }
        if ((m_pos < m_text.size()) && ((m_text[m_pos] == 'e') || (m_text[m_pos] == 'E')))
        {
            ++m_pos;
            if ((m_pos < m_text.size()) && ((m_text[m_pos] == '+') || (m_text[m_pos] == '-')))
            {
                ++m_pos;
            }
            if ((m_pos >= m_text.size()) || (m_text[m_pos] < '0') || (m_text[m_pos] > '9'))
            {
                error = "invalid number";
                return false;
            }
            while ((m_pos < m_text.size()) && (m_text[m_pos] >= '0') && (m_text[m_pos] <= '9'))
            {
                ++m_pos;
            }
        }
        std::istringstream in(m_text.substr(start, m_pos - start));
        double number = 0.0;
        in >> number;
        if (in.fail())
        {
            error = "invalid number";
            return false;
        }
        value.type        = JsonValue::Type::Number;
        value.numberValue = number;
        return true;
    }

    bool matchLiteral(char const* literal)
    {
        std::string const word(literal);
        if (m_text.compare(m_pos, word.size(), word) == 0)
        {
            m_pos += word.size();
            return true;
        }
        return false;
    }

    void skipWhitespace()
    {
        while ((m_pos < m_text.size())
               && (::isspace(static_cast<unsigned char>(m_text[m_pos])) != 0))
        {
            ++m_pos;
        }
    }

    std::string const& m_text;
    size_t m_pos = 0U;
};

bool
getUInt(JsonValue const& value, uint32_t& result)
{
    if (value.type != JsonValue::Type::Number)
    {
        return false;
    }
    if ((value.numberValue < 0.0) || (value.numberValue > 4294967295.0))
    {
        return false;
    }
    double const truncated = static_cast<double>(static_cast<uint32_t>(value.numberValue));
    if (truncated != value.numberValue)
    {
        return false;
    }
    result = static_cast<uint32_t>(value.numberValue);
    return true;
}

} // namespace

CommStateManagerPhase5&
CommStateManagerPhase5::getInstance()
{
    static CommStateManagerPhase5 instance;
    return instance;
}

void
CommStateManagerPhase5::requestEcuMode(EcuMode const mode)
{
    if (m_hasPending)
    {
        if (m_pendingMode == mode)
        {
            return;
        }
    }
    else if (m_currentMode == mode)
    {
        return;
    }
    m_pendingMode = mode;
    m_hasPending  = true;
}

EcuMode
CommStateManagerPhase5::getCurrentEcuMode() const
{
    return m_currentMode;
}

void
CommStateManagerPhase5::setDefaultPolicy(EcuMode const mode, FrameFilterPolicy const policy)
{
    m_defaultPolicies[mode] = policy;
}

void
CommStateManagerPhase5::addException(EcuMode const mode, uint32_t const frameId,
                                     uint8_t const channelType, FrameFilterPolicy const policy)
{
    m_exceptions[std::make_pair(mode, std::make_pair(frameId, channelType))] = policy;
}

void
CommStateManagerPhase5::clearPolicies()
{
    m_defaultPolicies.clear();
    m_exceptions.clear();
}

FrameFilterPolicy
CommStateManagerPhase5::getFrameFilterPolicy(uint32_t const frameId,
                                             uint8_t const channelType) const
{
    std::map<std::pair<EcuMode, ExceptionKey>, FrameFilterPolicy>::const_iterator const exIt =
        m_exceptions.find(std::make_pair(m_currentMode, std::make_pair(frameId, channelType)));
    if (exIt != m_exceptions.end())
    {
        return exIt->second;
    }
    std::map<EcuMode, FrameFilterPolicy>::const_iterator const defIt =
        m_defaultPolicies.find(m_currentMode);
    if (defIt != m_defaultPolicies.end())
    {
        return defIt->second;
    }
    return FrameFilterPolicy::ALLOW;
}

bool
CommStateManagerPhase5::isFrameAllowed(uint32_t const frameId, uint8_t const channelType,
                                       bool const isTx) const
{
    FrameFilterPolicy const policy = getFrameFilterPolicy(frameId, channelType);
    bool allowed                   = false;
    if (isTx)
    {
        allowed = (policy == FrameFilterPolicy::ALLOW) || (policy == FrameFilterPolicy::BLOCK_RX);
    }
    else
    {
        allowed = (policy == FrameFilterPolicy::ALLOW) || (policy == FrameFilterPolicy::BLOCK_TX);
    }
    if (!allowed)
    {
        ++m_blockCount;
    }
    return allowed;
}

void
CommStateManagerPhase5::setModeChangeCallback(ModeChangeCallback callback)
{
    m_modeChangeCallback = callback;
}

void
CommStateManagerPhase5::registerActionHandler(std::string const& name, ActionHandler handler)
{
    m_actionHandlers[name] = handler;
}

void
CommStateManagerPhase5::setModeActions(EcuMode const mode,
                                       std::vector<std::string> const& actions)
{
    m_modeActions[mode] = actions;
}

bool
CommStateManagerPhase5::loadConfig(std::string const& jsonText, std::string& error)
{
    JsonValue root;
    {
        JsonParser parser(jsonText);
        if (!parser.parse(root, error))
        {
            return false;
        }
    }
    if (root.type != JsonValue::Type::Object)
    {
        error = "root must be an object";
        return false;
    }

    // Parse into temporaries first so a malformed document leaves state unchanged.
    std::map<EcuMode, FrameFilterPolicy> newDefaults;
    std::map<std::pair<EcuMode, ExceptionKey>, FrameFilterPolicy> newExceptions;
    std::map<EcuMode, std::vector<std::string>> newActions;

    std::map<std::string, JsonValue>::const_iterator const modesIt = root.objectValue.find("modes");
    if (modesIt != root.objectValue.end())
    {
        if (modesIt->second.type != JsonValue::Type::Object)
        {
            error = "\"modes\" must be an object";
            return false;
        }
        for (std::map<std::string, JsonValue>::const_iterator modeIt =
                 modesIt->second.objectValue.begin();
             modeIt != modesIt->second.objectValue.end(); ++modeIt)
        {
            EcuMode mode = EcuMode::UNINIT;
            if (!parseEcuMode(modeIt->first, mode))
            {
                error = "unknown ECU mode \"" + modeIt->first + "\"";
                return false;
            }
            if (modeIt->second.type != JsonValue::Type::Object)
            {
                error = "mode \"" + modeIt->first + "\" must be an object";
                return false;
            }
            std::map<std::string, JsonValue>::const_iterator const defIt =
                modeIt->second.objectValue.find("default");
            if (defIt != modeIt->second.objectValue.end())
            {
                if (defIt->second.type != JsonValue::Type::String)
                {
                    error = "default policy of mode \"" + modeIt->first + "\" must be a string";
                    return false;
                }
                FrameFilterPolicy policy = FrameFilterPolicy::ALLOW;
                if (!parseFrameFilterPolicy(defIt->second.stringValue, policy))
                {
                    error = "unknown policy \"" + defIt->second.stringValue + "\"";
                    return false;
                }
                newDefaults[mode] = policy;
            }
            std::map<std::string, JsonValue>::const_iterator const excIt =
                modeIt->second.objectValue.find("exceptions");
            if (excIt != modeIt->second.objectValue.end())
            {
                if (excIt->second.type != JsonValue::Type::Array)
                {
                    error = "exceptions of mode \"" + modeIt->first + "\" must be an array";
                    return false;
                }
                for (size_t i = 0U; i < excIt->second.arrayValue.size(); ++i)
                {
                    JsonValue const& entry = excIt->second.arrayValue[i];
                    if (entry.type != JsonValue::Type::Object)
                    {
                        error = "exception entry of mode \"" + modeIt->first + "\" must be an object";
                        return false;
                    }
                    std::map<std::string, JsonValue>::const_iterator const frameIt =
                        entry.objectValue.find("frameId");
                    std::map<std::string, JsonValue>::const_iterator const channelIt =
                        entry.objectValue.find("channelType");
                    std::map<std::string, JsonValue>::const_iterator const policyIt =
                        entry.objectValue.find("policy");
                    if ((frameIt == entry.objectValue.end())
                        || (channelIt == entry.objectValue.end())
                        || (policyIt == entry.objectValue.end()))
                    {
                        error = "exception entry of mode \"" + modeIt->first
                            + "\" needs frameId, channelType and policy";
                        return false;
                    }
                    uint32_t frameId      = 0U;
                    uint32_t channelType  = 0U;
                    FrameFilterPolicy policy = FrameFilterPolicy::ALLOW;
                    if (!getUInt(frameIt->second, frameId))
                    {
                        error = "exception frameId of mode \"" + modeIt->first
                            + "\" must be a non-negative integer";
                        return false;
                    }
                    if (!getUInt(channelIt->second, channelType) || (channelType > 255U))
                    {
                        error = "exception channelType of mode \"" + modeIt->first
                            + "\" must be an integer in [0, 255]";
                        return false;
                    }
                    if ((policyIt->second.type != JsonValue::Type::String)
                        || (!parseFrameFilterPolicy(policyIt->second.stringValue, policy)))
                    {
                        error = "unknown exception policy of mode \"" + modeIt->first + "\"";
                        return false;
                    }
                    newExceptions[std::make_pair(
                        mode, std::make_pair(frameId, static_cast<uint8_t>(channelType)))] = policy;
                }
            }
        }
    }

    std::map<std::string, JsonValue>::const_iterator const actionsIt =
        root.objectValue.find("actions");
    if (actionsIt != root.objectValue.end())
    {
        if (actionsIt->second.type != JsonValue::Type::Object)
        {
            error = "\"actions\" must be an object";
            return false;
        }
        for (std::map<std::string, JsonValue>::const_iterator actionIt =
                 actionsIt->second.objectValue.begin();
             actionIt != actionsIt->second.objectValue.end(); ++actionIt)
        {
            EcuMode mode = EcuMode::UNINIT;
            if (!parseEcuMode(actionIt->first, mode))
            {
                error = "unknown ECU mode \"" + actionIt->first + "\"";
                return false;
            }
            if (actionIt->second.type != JsonValue::Type::Array)
            {
                error = "actions of mode \"" + actionIt->first + "\" must be an array";
                return false;
            }
            std::vector<std::string> names;
            for (size_t i = 0U; i < actionIt->second.arrayValue.size(); ++i)
            {
                JsonValue const& name = actionIt->second.arrayValue[i];
                if (name.type != JsonValue::Type::String)
                {
                    error = "action names of mode \"" + actionIt->first + "\" must be strings";
                    return false;
                }
                names.push_back(name.stringValue);
            }
            newActions[mode] = names;
        }
    }

    for (std::map<EcuMode, FrameFilterPolicy>::const_iterator it = newDefaults.begin();
         it != newDefaults.end(); ++it)
    {
        m_defaultPolicies[it->first] = it->second;
    }
    for (std::map<std::pair<EcuMode, ExceptionKey>, FrameFilterPolicy>::const_iterator it =
             newExceptions.begin();
         it != newExceptions.end(); ++it)
    {
        m_exceptions[it->first] = it->second;
    }
    for (std::map<EcuMode, std::vector<std::string>>::const_iterator it = newActions.begin();
         it != newActions.end(); ++it)
    {
        m_modeActions[it->first] = it->second;
    }
    return true;
}

void
CommStateManagerPhase5::clear()
{
    ::commgateway::CommStateManager::clear();
    m_currentMode = EcuMode::UNINIT;
    m_hasPending  = false;
    m_pendingMode = EcuMode::UNINIT;
    m_defaultPolicies.clear();
    m_exceptions.clear();
    m_modeActions.clear();
    m_actionHandlers.clear();
    m_modeChangeCallback = ModeChangeCallback();
    m_blockCount         = 0U;
}

void
CommStateManagerPhase5::mainFunction(uint32_t const nowMs)
{
    ::commgateway::CommStateManager::mainFunction(nowMs);
    if (!m_hasPending)
    {
        return;
    }
    EcuMode const from = m_currentMode;
    m_currentMode      = m_pendingMode;
    m_hasPending       = false;
    if (static_cast<bool>(m_modeChangeCallback))
    {
        m_modeChangeCallback(from, m_currentMode);
    }
    std::map<EcuMode, std::vector<std::string>>::const_iterator const actionIt =
        m_modeActions.find(m_currentMode);
    if (actionIt == m_modeActions.end())
    {
        return;
    }
    for (size_t i = 0U; i < actionIt->second.size(); ++i)
    {
        std::map<std::string, ActionHandler>::const_iterator const handlerIt =
            m_actionHandlers.find(actionIt->second[i]);
        if (handlerIt != m_actionHandlers.end())
        {
            if (static_cast<bool>(handlerIt->second))
            {
                handlerIt->second(m_currentMode);
            }
        }
    }
}

uint32_t
CommStateManagerPhase5::getBlockCount() const
{
    return m_blockCount;
}

} // namespace ecumode
