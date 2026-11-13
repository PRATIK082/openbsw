/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <csm/Csm.h>

#include <cstring>

#include <gtest/gtest.h>

namespace
{

csm::SoftwareCryptoService& svc()
{
    return csm::SoftwareCryptoService::instance();
}

} // namespace

TEST(CsmAesTest, KnownVectorEcb)
{
    std::uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                            0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    std::uint8_t pt[16] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                           0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};
    std::uint8_t expected[16] = {0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
                                  0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97};
    csm::KeyHandle kh = svc().importKey(csm::KeyType::Aes128, key, 16U);
    std::uint8_t ct[16] = {};
    ASSERT_TRUE(svc().aes128EcbEncrypt(kh, pt, 16U, ct));
    for (std::size_t i = 0; i < 16U; ++i)
    {
        EXPECT_EQ(ct[i], expected[i]);
    }
    std::uint8_t dec[16] = {};
    ASSERT_TRUE(svc().aes128EcbDecrypt(kh, ct, 16U, dec));
    for (std::size_t i = 0; i < 16U; ++i)
    {
        EXPECT_EQ(dec[i], pt[i]);
    }
}

TEST(CsmAesTest, RoundTripMultiBlock)
{
    std::uint8_t key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    std::uint8_t pt[32];
    for (std::size_t i = 0; i < 32U; ++i)
    {
        pt[i] = static_cast<std::uint8_t>(i * 7U + 3U);
    }
    csm::KeyHandle kh = svc().importKey(csm::KeyType::Aes128, key, 16U);
    std::uint8_t ct[32] = {};
    std::uint8_t dec[32] = {};
    ASSERT_TRUE(svc().aes128EcbEncrypt(kh, pt, 32U, ct));
    ASSERT_TRUE(svc().aes128EcbDecrypt(kh, ct, 32U, dec));
    EXPECT_EQ(std::memcmp(pt, dec, 32U), 0);
}

TEST(CsmSha256Test, KnownVector)
{
    char const* msg = "abc";
    std::uint8_t digest[32];
    ASSERT_TRUE(svc().sha256(reinterpret_cast<std::uint8_t const*>(msg), 3U, digest));
    std::uint8_t expected[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                                 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    for (std::size_t i = 0; i < 32U; ++i)
    {
        EXPECT_EQ(digest[i], expected[i]);
    }
}

TEST(CsmHmacTest, KnownVectorRfc4231Case1)
{
    std::uint8_t key[20];
    for (std::size_t i = 0; i < 20U; ++i)
    {
        key[i] = 0x0bU;
    }
    char const* msg = "Hi There";
    std::uint8_t mac[32];
    csm::KeyHandle kh = svc().importKey(csm::KeyType::HmacSha256, key, 20U);
    ASSERT_TRUE(svc().hmacSha256(kh, reinterpret_cast<std::uint8_t const*>(msg), 8U, mac));
    std::uint8_t expected[32] = {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
                                 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
                                 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
                                 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
    for (std::size_t i = 0; i < 32U; ++i)
    {
        EXPECT_EQ(mac[i], expected[i]);
    }
}

TEST(CsmMacVerifyTest, VerifyTrueAndFalse)
{
    std::uint8_t key[4] = {0x01, 0x02, 0x03, 0x04};
    char const* msg = "message";
    csm::KeyHandle kh = svc().importKey(csm::KeyType::HmacSha256, key, 4U);
    std::uint8_t mac[32];
    ASSERT_TRUE(svc().hmacSha256(kh, reinterpret_cast<std::uint8_t const*>(msg), 7U, mac));
    EXPECT_TRUE(svc().macVerify(kh, reinterpret_cast<std::uint8_t const*>(msg), 7U, mac, 32U));
    mac[0] ^= 0x01U;
    EXPECT_FALSE(svc().macVerify(kh, reinterpret_cast<std::uint8_t const*>(msg), 7U, mac, 32U));
    EXPECT_FALSE(svc().macVerify(kh, reinterpret_cast<std::uint8_t const*>(msg), 7U, mac, 31U));
}

TEST(CsmKeyHandleTest, InvalidKeyReturnsFalse)
{
    csm::KeyHandle bad{9999U};
    std::uint8_t data[16] = {};
    std::uint8_t out[16] = {};
    EXPECT_FALSE(svc().aes128EcbEncrypt(bad, data, 16U, out));
}