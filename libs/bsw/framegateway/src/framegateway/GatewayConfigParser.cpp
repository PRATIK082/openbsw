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
 * \ingroup framegateway
 */
#include "framegateway/GatewayConfigParser.h"

#include "framegateway/DiagLink.h"
#include "framegateway/FrameGateway.h"
#include "framegateway/GatewayPolicy.h"
#include "framegateway/XcpServer.h"
#include "framegateway/XcpSymbolTable.h"

#include <cstdlib>
#include <map>
#include <vector>

namespace framegateway
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
        m_pos++;
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
        m_pos++;
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
        m_pos++;
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
        m_pos    = static_cast<size_t>(end - m_text.c_str());
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

bool parseAction(std::string const& text, GatewayAction& out)
{
    if (text == "ROUTE_ONLY")
    {
        out = GatewayAction::ROUTE_ONLY;
    }
    else if (text == "EXTRACT_AND_SIGNAL")
    {
        out = GatewayAction::EXTRACT_AND_SIGNAL;
    }
    else if (text == "BOTH")
    {
        out = GatewayAction::BOTH;
    }
    else
    {
        return false;
    }
    return true;
}

bool parsePolicyAction(std::string const& text, PolicyAction& out)
{
    if (text == "ALLOW")
    {
        out = PolicyAction::ALLOW;
    }
    else if (text == "DENY")
    {
        out = PolicyAction::DENY;
    }
    else if (text == "TRANSFORM")
    {
        out = PolicyAction::TRANSFORM;
    }
    else if (text == "RATE_LIMIT")
    {
        out = PolicyAction::RATE_LIMIT;
    }
    else if (text == "LOG_ONLY")
    {
        out = PolicyAction::LOG_ONLY;
    }
    else if (text == "REDIRECT")
    {
        out = PolicyAction::REDIRECT;
    }
    else
    {
        return false;
    }
    return true;
}

/// Builds a data[0] source filter from "source == 0xNN" / "source != 0xNN".
bool parseSourceFilter(std::string const& text,
                       std::function<bool(uint8_t const*, uint16_t)>& out, std::string& error)
{
    bool isEqual = (text.find("==") != std::string::npos);
    bool isNotEqual = (text.find("!=") != std::string::npos);
    if (!isEqual && !isNotEqual)
    {
        error = "unsupported filter expression '" + text + "'";
        return false;
    }
    size_t const pos = text.find("0x");
    if (pos == std::string::npos)
    {
        error = "filter needs a 0xNN value: '" + text + "'";
        return false;
    }
    uint8_t const value = static_cast<uint8_t>(std::strtoul(text.c_str() + pos, nullptr, 16));
    if (isEqual)
    {
        out = [value](uint8_t const* data, uint16_t len) {
            return (len > 0U) && (data[0] == value);
        };
    }
    else
    {
        out = [value](uint8_t const* data, uint16_t len) {
            return (len > 0U) && (data[0] != value);
        };
    }
    return true;
}

bool parseFrameConfig(JsonValue const& entry, FrameConfig& out, std::string& error)
{
    uint32_t frameId = 0U, channelType = 0U, channelId = 0U, frameLength = 8U;
    JsonValue const* id     = findMember(entry, "frameId");
    JsonValue const* type   = findMember(entry, "channelType");
    JsonValue const* chId   = findMember(entry, "channelId");
    JsonValue const* length = findMember(entry, "frameLength");
    JsonValue const* action = findMember(entry, "action");
    if ((id == nullptr) || !asUint(*id, frameId))
    {
        error = "frameConfigs entry needs frameId";
        return false;
    }
    if ((type != nullptr) && !asUint(*type, channelType))
    {
        error = "channelType is not a number";
        return false;
    }
    if ((chId != nullptr) && !asUint(*chId, channelId))
    {
        error = "channelId is not a number";
        return false;
    }
    if ((length != nullptr) && !asUint(*length, frameLength))
    {
        error = "frameLength is not a number";
        return false;
    }
    out.frameId      = frameId;
    out.channelType  = static_cast<uint8_t>(channelType);
    out.channelId    = static_cast<uint8_t>(channelId);
    out.frameLength  = static_cast<uint16_t>(frameLength);
    if ((action != nullptr))
    {
        if ((action->type != JsonValue::Type::STR) || !parseAction(action->str, out.action))
        {
            error = "unknown action for frame";
            return false;
        }
    }
    JsonValue const* pdus = findMember(entry, "pdus");
    if (pdus != nullptr)
    {
        if (pdus->type != JsonValue::Type::ARR)
        {
            error = "pdus is not an array";
            return false;
        }
        for (JsonValue const& pdu : pdus->array)
        {
            PduInFrame layout{};
            uint32_t pduId = 0U, offset = 0U, pduLength = 0U, varLen = 0U, lenOff = 0U;
            JsonValue const* pid = findMember(pdu, "pduId");
            JsonValue const* off = findMember(pdu, "startByteOffset");
            JsonValue const* len = findMember(pdu, "length");
            if ((pid == nullptr) || !asUint(*pid, pduId) || (off == nullptr)
                || !asUint(*off, offset) || (len == nullptr) || !asUint(*len, pduLength))
            {
                error = "pdu needs pduId/startByteOffset/length";
                return false;
            }
            JsonValue const* varEntry = findMember(pdu, "isVariableLength");
            if ((varEntry != nullptr) && (varEntry->type == JsonValue::Type::BOOL))
            {
                varLen = varEntry->boolean ? 1U : 0U;
            }
            JsonValue const* lenOffEntry = findMember(pdu, "lengthFieldOffset");
            if ((lenOffEntry != nullptr) && !asUint(*lenOffEntry, lenOff))
            {
                error = "lengthFieldOffset is not a number";
                return false;
            }
            layout.pduId            = pduId;
            layout.startByteOffset  = static_cast<uint16_t>(offset);
            layout.length           = static_cast<uint16_t>(pduLength);
            layout.isVariableLength = (varLen != 0U);
            layout.lengthFieldOffset = static_cast<uint8_t>(lenOff);
            out.pdus.push_back(layout);
        }
    }
    return true;
}

bool parseDiagSession(JsonValue const& entry, DiagSession& out, std::string& error)
{
    uint32_t type = 0U, chId = 0U, req = 0U, resp = 0U, offset = 0U, timeout = 5000U;
    JsonValue const* t   = findMember(entry, "channelType");
    JsonValue const* c   = findMember(entry, "channelId");
    JsonValue const* reqEntry  = findMember(entry, "requestFrameId");
    JsonValue const* respEntry = findMember(entry, "responseFrameId");
    if ((reqEntry == nullptr) || !asUint(*reqEntry, req) || (respEntry == nullptr)
        || !asUint(*respEntry, resp))
    {
        error = "diagnosticSessions entry needs requestFrameId/responseFrameId";
        return false;
    }
    if ((t != nullptr) && !asUint(*t, type))
    {
        error = "channelType is not a number";
        return false;
    }
    if ((c != nullptr) && !asUint(*c, chId))
    {
        error = "channelId is not a number";
        return false;
    }
    JsonValue const* off = findMember(entry, "pduOffset");
    if ((off != nullptr) && !asUint(*off, offset))
    {
        error = "pduOffset is not a number";
        return false;
    }
    JsonValue const* to = findMember(entry, "sessionTimeoutMs");
    if ((to != nullptr) && !asUint(*to, timeout))
    {
        error = "sessionTimeoutMs is not a number";
        return false;
    }
    out.channelType       = static_cast<uint8_t>(type);
    out.channelId         = static_cast<uint8_t>(chId);
    out.requestFrameId    = req;
    out.responseFrameId   = resp;
    out.pduOffset         = static_cast<uint16_t>(offset);
    out.sessionTimeoutMs  = timeout;
    return true;
}

bool parseXcpSymbol(JsonValue const& entry, XcpSymbol& out, std::string& error)
{
    JsonValue const* name = findMember(entry, "name");
    JsonValue const* addr = findMember(entry, "address");
    JsonValue const* len  = findMember(entry, "length");
    JsonValue const* type = findMember(entry, "dataType");
    if ((name == nullptr) || (name->type != JsonValue::Type::STR) || (addr == nullptr)
        || (len == nullptr) || (type == nullptr) || (type->type != JsonValue::Type::STR))
    {
        error = "xcpSymbols entry needs name/address/length/dataType";
        return false;
    }
    uint32_t address = 0U, length = 0U;
    if (!asUint(*addr, address) || !asUint(*len, length)
        || !parseXcpDataType(type->str, out.dataType))
    {
        error = "xcpSymbols entry has invalid address/length/dataType";
        return false;
    }
    out.name    = name->str;
    out.address = address;
    out.length  = static_cast<uint16_t>(length);
    JsonValue const* cal = findMember(entry, "isCalibration");
    out.isCalibration = (cal != nullptr) && (cal->type == JsonValue::Type::BOOL) && cal->boolean;
    JsonValue const* minEntry = findMember(entry, "min");
    JsonValue const* maxEntry = findMember(entry, "max");
    if ((minEntry != nullptr) && (minEntry->type == JsonValue::Type::NUM))
    {
        out.min = minEntry->number;
    }
    if ((maxEntry != nullptr) && (maxEntry->type == JsonValue::Type::NUM))
    {
        out.max = maxEntry->number;
    }
    JsonValue const* unit = findMember(entry, "unit");
    if ((unit != nullptr) && (unit->type == JsonValue::Type::STR))
    {
        out.unit = unit->str;
    }
    return true;
}

bool parseGatewayPolicy(JsonValue const& entry, GatewayPolicy& out, std::string& error)
{
    uint32_t frameId = 0U, type = 0U, chId = 0U, rate = 0U;
    JsonValue const* id  = findMember(entry, "frameId");
    JsonValue const* act = findMember(entry, "action");
    if ((id == nullptr) || !asUint(*id, frameId) || (act == nullptr)
        || (act->type != JsonValue::Type::STR) || !parsePolicyAction(act->str, out.action))
    {
        error = "gatewayPolicies entry needs frameId and a valid action";
        return false;
    }
    JsonValue const* t = findMember(entry, "channelType");
    JsonValue const* c = findMember(entry, "channelId");
    if ((t != nullptr) && !asUint(*t, type))
    {
        error = "channelType is not a number";
        return false;
    }
    if ((c != nullptr) && !asUint(*c, chId))
    {
        error = "channelId is not a number";
        return false;
    }
    out.frameId     = frameId;
    out.channelType = static_cast<uint8_t>(type);
    out.channelId   = static_cast<uint8_t>(chId);
    JsonValue const* filter = findMember(entry, "filter");
    if ((filter != nullptr) && (filter->type == JsonValue::Type::STR))
    {
        if (!parseSourceFilter(filter->str, out.filter, error))
        {
            return false;
        }
    }
    JsonValue const* rateEntry = findMember(entry, "maxRatePerSec");
    if ((rateEntry != nullptr) && !asUint(*rateEntry, rate))
    {
        error = "maxRatePerSec is not a number";
        return false;
    }
    out.maxRatePerSec = rate;
    return true;
}

} // namespace

bool GatewayConfigParser::parse(std::string const& jsonText, FrameGateway& frameGateway,
                                DiagLink& diagLink, XcpServer& xcpServer, PolicyEngine& policyEngine,
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

    JsonValue const* frames = findMember(root, "frameConfigs");
    if (frames != nullptr)
    {
        if (frames->type != JsonValue::Type::ARR)
        {
            error = "frameConfigs is not an array";
            return false;
        }
        std::vector<FrameConfig> table;
        for (JsonValue const& entry : frames->array)
        {
            FrameConfig frame{};
            if (!parseFrameConfig(entry, frame, error))
            {
                return false;
            }
            table.push_back(frame);
        }
        frameGateway.init(table);
    }

    JsonValue const* sessions = findMember(root, "diagnosticSessions");
    if (sessions != nullptr)
    {
        if (sessions->type != JsonValue::Type::ARR)
        {
            error = "diagnosticSessions is not an array";
            return false;
        }
        std::vector<DiagSession> table;
        for (JsonValue const& entry : sessions->array)
        {
            DiagSession session{};
            if (!parseDiagSession(entry, session, error))
            {
                return false;
            }
            table.push_back(session);
        }
        diagLink.init(table);
    }

    JsonValue const* symbols = findMember(root, "xcpSymbols");
    if (symbols != nullptr)
    {
        if (symbols->type != JsonValue::Type::ARR)
        {
            error = "xcpSymbols is not an array";
            return false;
        }
        std::vector<XcpSymbol> table;
        for (JsonValue const& entry : symbols->array)
        {
            XcpSymbol symbol{};
            if (!parseXcpSymbol(entry, symbol, error))
            {
                return false;
            }
            table.push_back(symbol);
        }
        xcpServer.init(table, {});
    }

    JsonValue const* policies = findMember(root, "gatewayPolicies");
    if (policies != nullptr)
    {
        if (policies->type != JsonValue::Type::ARR)
        {
            error = "gatewayPolicies is not an array";
            return false;
        }
        for (JsonValue const& entry : policies->array)
        {
            GatewayPolicy policy{};
            if (!parseGatewayPolicy(entry, policy, error))
            {
                return false;
            }
            policyEngine.addPolicy(policy);
        }
    }
    return true;
}

} // namespace framegateway
