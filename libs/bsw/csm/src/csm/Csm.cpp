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

#include <algorithm>
#include <cstring>
#include <vector>

namespace csm
{

namespace
{

// AES S-box (standard).
static const std::uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

static std::uint8_t invSboxTable[256];
static bool invSboxReady = []() {
    for (int i = 0; i < 256; ++i)
    {
        invSboxTable[sbox[i]] = static_cast<std::uint8_t>(i);
    }
    return true;
}();

static std::uint8_t gmul(std::uint8_t a, std::uint8_t b)
{
    std::uint8_t p = 0U;
    for (int i = 0; i < 8; ++i)
    {
        if (b & 1U)
        {
            p ^= a;
        }
        bool hi = (a & 0x80U) != 0U;
        a = static_cast<std::uint8_t>(a << 1);
        if (hi)
        {
            a ^= 0x1BU;
        }
        b = static_cast<std::uint8_t>(b >> 1);
    }
    return p;
}

static void subBytes(std::uint8_t s[16])
{
    for (int i = 0; i < 16; ++i)
    {
        s[i] = sbox[s[i]];
    }
}

static void invSubBytes(std::uint8_t s[16])
{
    for (int i = 0; i < 16; ++i)
    {
        s[i] = invSboxTable[s[i]];
    }
}

static void shiftRows(std::uint8_t s[16])
{
    std::uint8_t t;
    // row 1
    t = s[1];
    s[1] = s[5];
    s[5] = s[9];
    s[9] = s[13];
    s[13] = t;
    // row 2
    t = s[2];
    s[2] = s[10];
    s[10] = t;
    t = s[6];
    s[6] = s[14];
    s[14] = t;
    // row 3
    t = s[3];
    s[3] = s[15];
    s[15] = s[11];
    s[11] = s[7];
    s[7] = t;
}

static void invShiftRows(std::uint8_t s[16])
{
    std::uint8_t t;
    t = s[13];
    s[13] = s[9];
    s[9] = s[5];
    s[5] = s[1];
    s[1] = t;
    t = s[2];
    s[2] = s[10];
    s[10] = t;
    t = s[6];
    s[6] = s[14];
    s[14] = t;
    t = s[3];
    s[3] = s[7];
    s[7] = s[11];
    s[11] = s[15];
    s[15] = t;
}

static void mixColumns(std::uint8_t s[16])
{
    for (int c = 0; c < 4; ++c)
    {
        std::uint8_t a0 = s[0 + 4 * c];
        std::uint8_t a1 = s[1 + 4 * c];
        std::uint8_t a2 = s[2 + 4 * c];
        std::uint8_t a3 = s[3 + 4 * c];
        s[0 + 4 * c] = gmul(a0, 2) ^ gmul(a1, 3) ^ a2 ^ a3;
        s[1 + 4 * c] = a0 ^ gmul(a1, 2) ^ gmul(a2, 3) ^ a3;
        s[2 + 4 * c] = a0 ^ a1 ^ gmul(a2, 2) ^ gmul(a3, 3);
        s[3 + 4 * c] = gmul(a0, 3) ^ a1 ^ a2 ^ gmul(a3, 2);
    }
}

static void invMixColumns(std::uint8_t s[16])
{
    for (int c = 0; c < 4; ++c)
    {
        std::uint8_t a0 = s[0 + 4 * c];
        std::uint8_t a1 = s[1 + 4 * c];
        std::uint8_t a2 = s[2 + 4 * c];
        std::uint8_t a3 = s[3 + 4 * c];
        s[0 + 4 * c] = gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
        s[1 + 4 * c] = gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
        s[2 + 4 * c] = gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11);
        s[3 + 4 * c] = gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14);
    }
}

static void addRoundKey(std::uint8_t s[16], std::uint8_t const* rk)
{
    for (int i = 0; i < 16; ++i)
    {
        s[i] ^= rk[i];
    }
}

static void keyExpansion(std::uint8_t const key[16], std::uint8_t roundKeys[176])
{
    static const std::uint8_t rcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};
    for (int i = 0; i < 16; ++i)
    {
        roundKeys[i] = key[i];
    }
    for (int i = 4; i < 44; ++i)
    {
        std::uint8_t temp[4];
        for (int j = 0; j < 4; ++j)
        {
            temp[j] = roundKeys[(i - 1) * 4 + j];
        }
        if (i % 4 == 0)
        {
            std::uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;
            for (int j = 0; j < 4; ++j)
            {
                temp[j] = sbox[temp[j]];
            }
            temp[0] ^= rcon[i / 4];
        }
        for (int j = 0; j < 4; ++j)
        {
            roundKeys[i * 4 + j] = roundKeys[(i - 4) * 4 + j] ^ temp[j];
        }
    }
}

static void aesEncryptBlock(std::uint8_t const in[16], std::uint8_t out[16], std::uint8_t const rk[176])
{
    std::uint8_t s[16];
    std::memcpy(s, in, 16);
    addRoundKey(s, rk);
    for (int r = 1; r <= 9; ++r)
    {
        subBytes(s);
        shiftRows(s);
        mixColumns(s);
        addRoundKey(s, rk + r * 16);
    }
    subBytes(s);
    shiftRows(s);
    addRoundKey(s, rk + 10 * 16);
    std::memcpy(out, s, 16);
}

static void aesDecryptBlock(std::uint8_t const in[16], std::uint8_t out[16], std::uint8_t const rk[176])
{
    std::uint8_t s[16];
    std::memcpy(s, in, 16);
    addRoundKey(s, rk + 10 * 16);
    for (int r = 9; r >= 1; --r)
    {
        invShiftRows(s);
        invSubBytes(s);
        addRoundKey(s, rk + r * 16);
        invMixColumns(s);
    }
    invShiftRows(s);
    invSubBytes(s);
    addRoundKey(s, rk);
    std::memcpy(out, s, 16);
}

// SHA-256.
static const std::uint32_t sha256K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

static std::uint32_t rotr(std::uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32U - n));
}

static void sha256Impl(std::uint8_t const* data, std::size_t length, std::uint8_t digest[32])
{
    std::uint32_t h[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                           0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

    // Pre-processing.
    std::size_t totalLen = length;
    std::size_t paddedLen = ((length + 9 + 63) / 64) * 64;
    std::vector<std::uint8_t> msg(paddedLen, 0U);
    std::memcpy(msg.data(), data, length);
    msg[length] = 0x80U;
    std::uint64_t bitLen = static_cast<std::uint64_t>(totalLen) * 8ULL;
    for (int i = 0; i < 8; ++i)
    {
        msg[paddedLen - 8 + i] = static_cast<std::uint8_t>((bitLen >> (56 - 8 * i)) & 0xFFU);
    }

    for (std::size_t chunk = 0; chunk < paddedLen; chunk += 64)
    {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<std::uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<std::uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(msg[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i)
        {
            std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i)
        {
            std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            std::uint32_t ch = (e & f) ^ (~e & g);
            std::uint32_t temp1 = hh + S1 + ch + sha256K[i] + w[i];
            std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = S0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    for (int i = 0; i < 8; ++i)
    {
        digest[i * 4] = static_cast<std::uint8_t>((h[i] >> 24) & 0xFFU);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((h[i] >> 16) & 0xFFU);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((h[i] >> 8) & 0xFFU);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(h[i] & 0xFFU);
    }
}

} // namespace

SoftwareCryptoService& SoftwareCryptoService::instance()
{
    static SoftwareCryptoService inst;
    return inst;
}

KeyHandle SoftwareCryptoService::importKey(KeyType type, std::uint8_t const* key, std::size_t length)
{
    KeyHandle handle{0U};
    if (key == nullptr)
    {
        if (type == KeyType::Aes128 && length == 0U)
        {
            // allow empty? no
        }
    }

    if (type == KeyType::Aes128)
    {
        if (length != 16U || key == nullptr)
        {
            return handle;
        }
    }
    else
    {
        if (key == nullptr && length != 0U)
        {
            return handle;
        }
    }

    KeySlot slot;
    slot.used = true;
    slot.id = nextId_++;
    slot.type = type;
    slot.bytes.assign(key, key + length);
    slots_.push_back(slot);
    handle.id = slot.id;
    return handle;
}

bool SoftwareCryptoService::destroyKey(KeyHandle const& key)
{
    for (auto& slot : slots_)
    {
        if (slot.used && slot.id == key.id)
        {
            slot.used = false;
            slot.bytes.clear();
            return true;
        }
    }
    return false;
}

SoftwareCryptoService::KeySlot* SoftwareCryptoService::find(KeyHandle const& key)
{
    for (auto& slot : slots_)
    {
        if (slot.used && slot.id == key.id)
        {
            return &slot;
        }
    }
    return nullptr;
}

SoftwareCryptoService::KeySlot const* SoftwareCryptoService::find(KeyHandle const& key) const
{
    for (auto const& slot : slots_)
    {
        if (slot.used && slot.id == key.id)
        {
            return &slot;
        }
    }
    return nullptr;
}

bool SoftwareCryptoService::aes128EcbEncrypt(KeyHandle const& key, std::uint8_t const* plaintext, std::size_t length,
                                            std::uint8_t* ciphertext)
{
    KeySlot* slot = find(key);
    if (slot == nullptr || slot->type != KeyType::Aes128 || plaintext == nullptr || ciphertext == nullptr ||
        length == 0U || (length % 16U) != 0U)
    {
        return false;
    }
    std::uint8_t rk[176];
    keyExpansion(slot->bytes.data(), rk);
    for (std::size_t off = 0; off < length; off += 16U)
    {
        aesEncryptBlock(plaintext + off, ciphertext + off, rk);
    }
    return true;
}

bool SoftwareCryptoService::aes128EcbDecrypt(KeyHandle const& key, std::uint8_t const* ciphertext, std::size_t length,
                                            std::uint8_t* plaintext)
{
    KeySlot* slot = find(key);
    if (slot == nullptr || slot->type != KeyType::Aes128 || ciphertext == nullptr || plaintext == nullptr ||
        length == 0U || (length % 16U) != 0U)
    {
        return false;
    }
    std::uint8_t rk[176];
    keyExpansion(slot->bytes.data(), rk);
    for (std::size_t off = 0; off < length; off += 16U)
    {
        aesDecryptBlock(ciphertext + off, plaintext + off, rk);
    }
    return true;
}

bool SoftwareCryptoService::sha256(std::uint8_t const* data, std::size_t length, std::uint8_t digest[32])
{
    if (digest == nullptr)
    {
        return false;
    }
    if (data == nullptr && length != 0U)
    {
        return false;
    }
    sha256Impl(data, length, digest);
    return true;
}

bool SoftwareCryptoService::hmacSha256(KeyHandle const& key, std::uint8_t const* data, std::size_t length,
                                       std::uint8_t mac[32])
{
    KeySlot const* slot = find(key);
    if (slot == nullptr || slot->type != KeyType::HmacSha256 || mac == nullptr)
    {
        return false;
    }
    if (data == nullptr && length != 0U)
    {
        return false;
    }

    std::uint8_t keyBlock[64];
    std::memset(keyBlock, 0, 64);
    if (slot->bytes.size() > 64U)
    {
        sha256Impl(slot->bytes.data(), slot->bytes.size(), keyBlock); // first 32 bytes
    }
    else
    {
        std::memcpy(keyBlock, slot->bytes.data(), slot->bytes.size());
    }

    std::uint8_t ipad[64];
    std::uint8_t opad[64];
    for (int i = 0; i < 64; ++i)
    {
        ipad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x36U);
        opad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x5CU);
    }

    std::vector<std::uint8_t> buf;
    buf.reserve(64U + length);
    buf.insert(buf.end(), ipad, ipad + 64);
    if (length > 0U)
    {
        buf.insert(buf.end(), data, data + length);
    }
    std::uint8_t inner[32];
    sha256Impl(buf.data(), buf.size(), inner);

    std::vector<std::uint8_t> buf2;
    buf2.reserve(64U + 32U);
    buf2.insert(buf2.end(), opad, opad + 64);
    buf2.insert(buf2.end(), inner, inner + 32);
    sha256Impl(buf2.data(), buf2.size(), mac);
    return true;
}

bool SoftwareCryptoService::macVerify(KeyHandle const& key, std::uint8_t const* data, std::size_t length,
                                     std::uint8_t const* mac, std::size_t macLength)
{
    if (mac == nullptr || macLength != 32U)
    {
        return false;
    }
    std::uint8_t computed[32];
    if (!hmacSha256(key, data, length, computed))
    {
        return false;
    }
    std::uint8_t diff = 0U;
    for (std::size_t i = 0; i < 32U; ++i)
    {
        diff |= static_cast<std::uint8_t>(computed[i] ^ mac[i]);
    }
    return diff == 0U;
}

} // namespace csm