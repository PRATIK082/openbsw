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
 * \ingroup csm
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace csm
{

/// Handle to a key stored by a crypto backend.
struct KeyHandle
{
    std::uint32_t id = 0U;

    bool operator==(KeyHandle const& other) const { return id == other.id; }
    bool operator!=(KeyHandle const& other) const { return id != other.id; }
};

/// Key types supported by the software reference backend.
enum class KeyType
{
    Aes128,
    HmacSha256,
};

/// Abstract crypto service interface.
/// \req SYS-CRYPTO-001 AES-128 ECB encrypt/decrypt using a key handle.
/// \req SYS-CRYPTO-002 SHA-256 digest computation.
/// \req SYS-CRYPTO-003 HMAC-SHA256 computation.
/// \req SYS-CRYPTO-004 Constant-time MAC verification.
class ICryptoService
{
public:
    virtual ~ICryptoService() = default;

    virtual bool aes128EcbEncrypt(KeyHandle const& key, std::uint8_t const* plaintext, std::size_t length,
                                  std::uint8_t* ciphertext) = 0;
    virtual bool aes128EcbDecrypt(KeyHandle const& key, std::uint8_t const* ciphertext, std::size_t length,
                                  std::uint8_t* plaintext) = 0;
    virtual bool sha256(std::uint8_t const* data, std::size_t length, std::uint8_t digest[32]) = 0;
    virtual bool hmacSha256(KeyHandle const& key, std::uint8_t const* data, std::size_t length,
                            std::uint8_t mac[32]) = 0;
    virtual bool macVerify(KeyHandle const& key, std::uint8_t const* data, std::size_t length,
                           std::uint8_t const* mac, std::size_t macLength) = 0;
};

/// Software reference backend (NOT production grade).
/// \req SYS-CRYPTO-005 Software-backed crypto primitives for testing/reference.
class SoftwareCryptoService : public ICryptoService
{
public:
    static SoftwareCryptoService& instance();

    KeyHandle importKey(KeyType type, std::uint8_t const* key, std::size_t length);
    bool destroyKey(KeyHandle const& key);

    bool aes128EcbEncrypt(KeyHandle const& key, std::uint8_t const* plaintext, std::size_t length,
                          std::uint8_t* ciphertext) override;
    bool aes128EcbDecrypt(KeyHandle const& key, std::uint8_t const* ciphertext, std::size_t length,
                          std::uint8_t* plaintext) override;
    bool sha256(std::uint8_t const* data, std::size_t length, std::uint8_t digest[32]) override;
    bool hmacSha256(KeyHandle const& key, std::uint8_t const* data, std::size_t length,
                    std::uint8_t mac[32]) override;
    bool macVerify(KeyHandle const& key, std::uint8_t const* data, std::size_t length,
                   std::uint8_t const* mac, std::size_t macLength) override;

private:
    SoftwareCryptoService() = default;

    struct KeySlot
    {
        bool used = false;
        std::uint32_t id = 0U;
        KeyType type = KeyType::Aes128;
        std::vector<std::uint8_t> bytes;
    };

    std::vector<KeySlot> slots_;
    std::uint32_t nextId_ = 1U;

    KeySlot* find(KeyHandle const& key);
    KeySlot const* find(KeyHandle const& key) const;
};

} // namespace csm