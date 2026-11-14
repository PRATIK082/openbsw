/********************************************************************************
 * Copyright (c) 2024 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#pragma once

#include "uds/base/Service.h"

namespace faultmanager
{
class FaultManager;
} // namespace faultmanager

namespace uds
{
/**
 * UDS service ControlDTCSetting (0x85).
 *
 * \req SYS-UDS-003 Control DTC setting (0x85) routed to Dem.
 * \trace TEST: ControlDTCSettingTest (libs/bsw/uds/test)
 */
class ControlDTCSetting : public Service
{
public:
    enum DtcMode
    {
        CONTROL_DTC_SETTING_ON  = 0x01,
        CONTROL_DTC_SETTING_OFF = 0x02
    };

    explicit ControlDTCSetting();

    void setFaultManager(faultmanager::FaultManager* dem) { _dem = dem; }
    faultmanager::FaultManager* getFaultManager() const { return _dem; }

private:
    faultmanager::FaultManager* _dem{nullptr};
    static uint8_t const EXPECTED_REQUEST_LENGTH = 1U;
    static uint8_t const RESPONSE_LENGTH         = 1U;

    DiagReturnCode::Type process(
        IncomingDiagConnection& connection,
        uint8_t const* request,
        uint16_t requestLength) override;
};

} // namespace uds
