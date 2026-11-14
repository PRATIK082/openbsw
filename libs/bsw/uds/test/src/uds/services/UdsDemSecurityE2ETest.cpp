/********************************************************************************
 * Copyright (c) 2026 BMW AG
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * End-to-end (POSIX) wiring test for UDS <-> Dem/Persistence/Security.
 * Covers 0x14, 0x19, 0x27 and 0x85 round trips using a StrictMock FaultManager.
 */
#include "uds/services/cleardiagnosticinformation/ClearDiagnosticInformation.h"
#include "uds/services/readdtcinformation/ReadDTCInformation.h"
#include "uds/services/controldtcsetting/ControlDTCSetting.h"
#include "uds/services/securityaccess/SecurityAccess.h"

#include "uds/connection/IncomingDiagConnectionMock.h"
#include "uds/session/ApplicationDefaultSession.h"
#include "uds/session/ApplicationExtendedSession.h"
#include "uds/session/DiagSessionManagerMock.h"

#include "faultmanager/FaultManager.h"

#include <transport/TransportMessageWithBuffer.h>

#include <etl/span.h>
#include <etl/vector.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
using namespace ::uds;
using namespace ::testing;
using namespace ::transport::test;

class MinimalSecurityAccess : public SecurityAccess
{
public:
    explicit MinimalSecurityAccess(DiagSession::DiagSessionMask mask) : SecurityAccess(mask), _seedIssued(false) {}

    bool seedIssued() const { return _seedIssued; }

protected:
    DiagReturnCode::Type process(
        IncomingDiagConnection& connection,
        uint8_t const* const request,
        uint16_t const requestLength) override
    {
        if (requestLength < 1U)
        {
            return DiagReturnCode::ISO_INVALID_FORMAT;
        }

        uint8_t const subFunction = request[0];

        if (subFunction == 0x01U) // requestSeed
        {
            for (size_t i = 0U; i < fSeed.size(); ++i)
            {
                fSeed[i] = static_cast<uint8_t>(i + 1U);
            }
            _seedIssued = true;
            fSecurityAccessType = 0x01U;

            PositiveResponse& response = connection.releaseRequestGetResponse();
            (void)response.appendUint8(0x01U);
            for (size_t i = 0U; i < fSeed.size(); ++i)
            {
                (void)response.appendUint8(fSeed[i]);
            }
            (void)connection.sendPositiveResponseInternal(response.getLength(), *this);
            return DiagReturnCode::OK;
        }

        if (subFunction == 0x02U) // sendKey
        {
            if (!_seedIssued)
            {
                return DiagReturnCode::ISO_REQUEST_SEQUENCE_ERROR;
            }
            if (requestLength < 3U)
            {
                return DiagReturnCode::ISO_INVALID_FORMAT;
            }

            uint16_t const receivedKey
                = (static_cast<uint16_t>(request[1]) << 8U) | static_cast<uint16_t>(request[2]);
            uint16_t const expectedKey = static_cast<uint16_t>((fSeed[0] << 8U) | fSeed[1]);

            if (receivedKey == expectedKey)
            {
                fAuthenticatedEcu = 0x10U;
                _seedIssued       = false;

                PositiveResponse& response = connection.releaseRequestGetResponse();
                (void)response.appendUint8(0x02U);
                (void)connection.sendPositiveResponseInternal(response.getLength(), *this);
                return DiagReturnCode::OK;
            }

            _seedIssued = false;
            return DiagReturnCode::ISO_INVALID_KEY;
        }

        return DiagReturnCode::ISO_SUBFUNCTION_NOT_SUPPORTED;
    }

private:
    bool _seedIssued;
};

class MockFaultManager : public faultmanager::FaultManager
{
public:
    MOCK_METHOD(void, clearAllDtcs, (), (override));
    MOCK_METHOD(void, setDtcRecordingSuppressed, (bool suppressed), (override));
    MOCK_METHOD(bool, isDtcRecordingSuppressed, (), (const, override));
    MOCK_METHOD(
        ::etl::vector<faultmanager::DtcWithStatus, faultmanager::MAX_DTCS>,
        getDtcsWithStatus,
        (),
        (const, override));

};

class UdsDemSecurityE2ETest : public ::testing::Test
{
public:
    UdsDemSecurityE2ETest()
    : fClear()
    , fRead()
    , fControl()
    , fSecurity(DiagSession::ALL_SESSIONS())
    , fIncomingDiagConnection(::async::CONTEXT_INVALID)
    , fSessionManager()
    {}

    virtual void SetUp()
    {
        fClear.setDefaultDiagSessionManager(fSessionManager);
        fRead.setDefaultDiagSessionManager(fSessionManager);
        fControl.setDefaultDiagSessionManager(fSessionManager);
        fSecurity.setDefaultDiagSessionManager(fSessionManager);
    }

protected:
    ClearDiagnosticInformation fClear;
    ReadDTCInformation fRead;
    ControlDTCSetting fControl;
    MinimalSecurityAccess fSecurity;
    StrictMock<IncomingDiagConnectionMock> fIncomingDiagConnection;
    StrictMock<DiagSessionManagerMock> fSessionManager;
};

TEST_F(UdsDemSecurityE2ETest, clear_dtc_invokes_dem_clear_all)
{
    StrictMock<MockFaultManager> dem;
    fClear.setFaultManager(&dem);

    uint8_t request[] = {0x14U, 0xFFU, 0xFFU, 0xFFU};
    TransportMessageWithBuffer pRequest(0xF1U, 0x10U, request, 0U);
    fIncomingDiagConnection.requestMessage = pRequest.get();

    EXPECT_CALL(fSessionManager, getActiveSession())
        .WillRepeatedly(ReturnRef(DiagSession::APPLICATION_DEFAULT_SESSION()));
    EXPECT_CALL(fSessionManager, acceptedJob(_, _, _, _))
        .WillRepeatedly(Return(uds::DiagReturnCode::OK));

    EXPECT_CALL(dem, clearAllDtcs()).Times(1);

    EXPECT_EQ(
        DiagReturnCode::OK,
        fClear.execute(fIncomingDiagConnection, request, sizeof(request)));
}

TEST_F(UdsDemSecurityE2ETest, read_dtc_invokes_dem_get_dtcs_with_status)
{
    StrictMock<MockFaultManager> dem;
    fRead.setFaultManager(&dem);

    ::etl::vector<faultmanager::DtcWithStatus, faultmanager::MAX_DTCS> dtcs;
    faultmanager::DtcWithStatus d;
    d.dtc    = 0x123456U;
    d.status = 0x2FU;
    dtcs.push_back(d);

    uint8_t request[] = {0x19U, 0x02U, 0xFFU};
    TransportMessageWithBuffer pRequest(0xF1U, 0x10U, request, 0U);
    fIncomingDiagConnection.requestMessage = pRequest.get();

    EXPECT_CALL(fSessionManager, getActiveSession())
        .WillRepeatedly(ReturnRef(DiagSession::APPLICATION_DEFAULT_SESSION()));
    EXPECT_CALL(fSessionManager, acceptedJob(_, _, _, _))
        .WillRepeatedly(Return(uds::DiagReturnCode::OK));

    EXPECT_CALL(dem, getDtcsWithStatus()).WillOnce(Return(dtcs));

    EXPECT_EQ(
        DiagReturnCode::OK,
        fRead.execute(fIncomingDiagConnection, request, sizeof(request)));
}

TEST_F(UdsDemSecurityE2ETest, control_dtc_setting_invokes_dem_suppression)
{
    StrictMock<MockFaultManager> dem;
    fControl.setFaultManager(&dem);

    // OFF -> suppressed true
    {
        uint8_t request[] = {0x85U, 0x02U};
        TransportMessageWithBuffer pRequest(0xF1U, 0x10U, request, 0U);
        fIncomingDiagConnection.requestMessage = pRequest.get();

        EXPECT_CALL(fSessionManager, getActiveSession())
            .WillRepeatedly(ReturnRef(DiagSession::APPLICATION_EXTENDED_SESSION()));
        EXPECT_CALL(fSessionManager, acceptedJob(_, _, _, _))
            .WillRepeatedly(Return(uds::DiagReturnCode::OK));

        EXPECT_CALL(dem, setDtcRecordingSuppressed(true)).Times(1);

        EXPECT_EQ(
            DiagReturnCode::OK,
            fControl.execute(fIncomingDiagConnection, request, sizeof(request)));
    }

    // ON -> suppressed false
    {
        uint8_t request[] = {0x85U, 0x01U};
        TransportMessageWithBuffer pRequest(0xF1U, 0x10U, request, 0U);
        fIncomingDiagConnection.requestMessage = pRequest.get();

        EXPECT_CALL(fSessionManager, getActiveSession())
            .WillRepeatedly(ReturnRef(DiagSession::APPLICATION_EXTENDED_SESSION()));
        EXPECT_CALL(fSessionManager, acceptedJob(_, _, _, _))
            .WillRepeatedly(Return(uds::DiagReturnCode::OK));

        EXPECT_CALL(dem, setDtcRecordingSuppressed(false)).Times(1);

        EXPECT_EQ(
            DiagReturnCode::OK,
            fControl.execute(fIncomingDiagConnection, request, sizeof(request)));
    }
}

TEST_F(UdsDemSecurityE2ETest, security_access_seed_key_round_trip)
{
    uint8_t requestSeed[] = {0x27U, 0x01U};
    TransportMessageWithBuffer pRequest1(0xF1U, 0x10U, requestSeed, 0U);
    fIncomingDiagConnection.requestMessage = pRequest1.get();

    EXPECT_CALL(fSessionManager, getActiveSession())
        .WillRepeatedly(ReturnRef(DiagSession::APPLICATION_DEFAULT_SESSION()));
    EXPECT_CALL(fSessionManager, acceptedJob(_, _, _, _))
        .WillRepeatedly(Return(uds::DiagReturnCode::OK));

    EXPECT_EQ(
        DiagReturnCode::OK,
        fSecurity.execute(fIncomingDiagConnection, requestSeed, sizeof(requestSeed)));
    EXPECT_TRUE(fSecurity.seedIssued());

    // sendKey with correct key derived from seed bytes 0x01 0x02
    uint8_t requestKey[] = {0x27U, 0x02U, 0x01U, 0x02U};
    TransportMessageWithBuffer pRequest2(0xF1U, 0x10U, requestKey, 0U);
    fIncomingDiagConnection.requestMessage = pRequest2.get();

    EXPECT_CALL(fSessionManager, getActiveSession())
        .WillRepeatedly(ReturnRef(DiagSession::APPLICATION_DEFAULT_SESSION()));
    EXPECT_CALL(fSessionManager, acceptedJob(_, _, _, _))
        .WillRepeatedly(Return(uds::DiagReturnCode::OK));

    EXPECT_EQ(
        DiagReturnCode::OK,
        fSecurity.execute(fIncomingDiagConnection, requestKey, sizeof(requestKey)));
}

} // namespace
```

Wait SecurityAccess service auth: Service::execute checks getDiagAuthenticator().isAuthenticated(connection.sourceAddress). Default authenticator? IDiagAuthenticator default. For SecurityAccess itself is authenticator, but as Service base uses getDefaultDiagAuthenticator, not itself. Need check getDiagAuthenticator default. Could be a default authenticator that always true. Existing SecurityAccessTest doesn't call execute. Our execute may fail auth if default authenticator denies. Let's check getDefaultDiagAuthenticator.<|end|><|start|>-start<|channel|>analysis <|constrain|>xml<|message|><rfl:parameter name="command">
cd E:/My_Project/My_Github/openbsw-n1-uds; grep -rn "getDefaultDiagAuthenticator" libs/bsw/uds/src libs/bsw/uds/include