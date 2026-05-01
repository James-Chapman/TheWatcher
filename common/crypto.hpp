#pragma once

#include <array>
#include <iomanip>
#include <sodium.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <zmq.h>

namespace thewatcher::crypto
{

// z85-encoded Curve25519 keypair (format understood by ZMQ CURVE).
struct CurveKeyPair
{
    std::string public_key_z85; // 40 chars
    std::string secret_key_z85; // 40 chars
};

inline void init()
{
    if (sodium_init() < 0)
        throw std::runtime_error("libsodium initialisation failed");
}

// Generate a new ZMQ CURVE keypair via libsodium Curve25519.
inline CurveKeyPair generate_curve_keypair()
{
    // ZMQ CURVE uses X25519 (Curve25519 DH); crypto_box_keypair gives us the
    // same key material that zmq_curve_keypair would produce internally.
    std::array<unsigned char, 32> pk{}, sk{};
    crypto_box_keypair(pk.data(), sk.data());

    CurveKeyPair kp;
    kp.public_key_z85.resize(41);
    kp.secret_key_z85.resize(41);

    if (!zmq_z85_encode(kp.public_key_z85.data(), pk.data(), 32))
        throw std::runtime_error("z85 encode of public key failed");
    if (!zmq_z85_encode(kp.secret_key_z85.data(), sk.data(), 32))
        throw std::runtime_error("z85 encode of secret key failed");

    kp.public_key_z85.resize(40);
    kp.secret_key_z85.resize(40);
    return kp;
}

// Decode a z85 key to its raw 32-byte representation.
inline std::array<unsigned char, 32> z85_decode(const std::string& z85)
{
    if (z85.size() != 40)
        throw std::invalid_argument("CURVE z85 key must be exactly 40 characters");
    std::array<unsigned char, 32> raw{};
    if (!zmq_z85_decode(raw.data(), z85.c_str()))
        throw std::runtime_error("z85 decode failed");
    return raw;
}

// Encode a raw 32-byte key to z85.
inline std::string z85_encode(const std::array<unsigned char, 32>& raw)
{
    std::string z85(41, '\0');
    if (!zmq_z85_encode(z85.data(), raw.data(), 32))
        throw std::runtime_error("z85 encode failed");
    z85.resize(40);
    return z85;
}

inline std::string server_public_key_fingerprint(const std::string& public_key_z85)
{
    init();
    std::array<unsigned char, crypto_generichash_BYTES> digest{};
    crypto_generichash(digest.data(), digest.size(), reinterpret_cast<const unsigned char*>(public_key_z85.data()),
                       public_key_z85.size(), nullptr, 0);

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : digest)
        out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}

} // namespace thewatcher::crypto
