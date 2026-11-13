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

#include "uds/authentication/DefaultDiagAuthenticator.h"
#include "uds/authentication/IDiagAuthenticator.h"
#include "uds/base/Service.h"

#include <etl/array.h>

namespace faultmanager
{
class FaultManager;
} // namespace faultmanager

namespace uds
{
/**
 * UDS service SecurityAccess (0x27).
 *
 * \req SYS-UDS-004 Security access (0x27) software seed/key check.
 * \trace TEST: SecurityAccessTest (libs/bsw/uds/test)
 */
class SecurityAccess
: public uds::IDiagAuthenticator
, public Service
{
public:
    explicit SecurityAccess(DiagSession::DiagSessionMask mask);

    /**
     * \see IDiagAuthenticator::isAuthenticated()
     */
    bool isAuthenticated(uint16_t address) const override;
    /**
     * \see IDiagAuthenticator::getNotAuthenticatedReturnCode()
     */
    DiagReturnCode::Type getNotAuthenticatedReturnCode() const override;

    void resetAuthentication();

    /**
     * \return securityaccesstype
     */
    uint8_t getSecurityAccessType() const;

    /// Wire to the Dem (FaultManager) (kept for API consistency with other UDS services).
    void setFaultManager(faultmanager::FaultManager* dem) { _dem = dem; }
    faultmanager::FaultManager* getFaultManager() const { return _dem; }

protected:
    static uint8_t const RANDOM_LENGTH    = 8U;
    static uint8_t const TESTER_ID_LENGTH = 4U;
    ::etl::array<uint8_t, RANDOM_LENGTH> fSeed;
    ::etl::array<uint8_t, TESTER_ID_LENGTH> fTesterId;
    uint16_t fCurrentEcuId;
    uint16_t fAuthenticatedEcu;
    uint8_t fSecurityAccessType;

    faultmanager::FaultManager* _dem{nullptr};
};

} // namespace uds
