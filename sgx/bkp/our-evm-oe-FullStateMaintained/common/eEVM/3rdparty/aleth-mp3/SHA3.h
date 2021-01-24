// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include "FixedHash.h"
#include "vector_ref.h"

// #include <ethash/keccak.hpp>

#include "keccak/KeccakHash.h"

#include <string>

namespace dev
{
// SHA-3 convenience routines.

inline void _keccak_256(
    const unsigned char* input,
    unsigned int inputByteLen,
    unsigned char* output) {
    // Ethereum started using Keccak and called it SHA3 before it was finalised.
    // Standard SHA3-256 (the FIPS accepted version) uses padding 0x06, but
    // Ethereum's "Keccak-256" uses padding 0x01.
    // All other constants are copied from Keccak_HashInitialize_SHA3_256 in
    // KeccakHash.h.
    Keccak_HashInstance hi;
    Keccak_HashInitialize(&hi, 1088, 512, 256, 0x01);
    Keccak_HashUpdate(&hi, input, inputByteLen * std::numeric_limits<unsigned char>::digits);
    Keccak_HashFinal(&hi, output);
}

/// Calculate SHA3-256 hash of the given input and load it into the given output.
/// @returns false if o_output.size() != 32.
 bool sha3(bytesConstRef _input, bytesRef o_output) noexcept;
// bool _sha3(bytesConstRef _input, bytesRef o_output) noexcept {
//     if(32 != o_output.size()){
//         return false;
//     }
//     _keccak_256(_input.data(), _input.size(), o_output.data());
//     return true;
// }

/// Calculate SHA3-256 hash of the given input, returning as a 256-bit hash.
inline h256 sha3(bytesConstRef _input) noexcept {

    uint8_t h[32];
    _keccak_256(_input.data(), _input.size(), h);
    return h256{h, h256::ConstructFromPointer};
}

inline SecureFixedHash<32> sha3Secure(bytesConstRef _input) noexcept {
    SecureFixedHash<32> ret;
    sha3(_input, ret.writable().ref());
    return ret;
}

/// Calculate SHA3-256 hash of the given input, returning as a 256-bit hash.
inline h256 sha3(bytes const& _input) noexcept {
    // uint8_t h[32];
    // _keccak_256(bytesConstRef(&_input).data(), _input.size(), h);
    // return h256{h, h256::ConstructFromPointer};
    return sha3(bytesConstRef(&_input));
}

inline SecureFixedHash<32> sha3Secure(bytes const& _input) noexcept {
    return sha3Secure(bytesConstRef(&_input));
}

/// Calculate SHA3-256 hash of the given input (presented as a binary-filled string), returning as a 256-bit hash.
inline h256 sha3(std::string const& _input) noexcept {
    return sha3(bytesConstRef(_input));
}

inline SecureFixedHash<32> sha3Secure(std::string const& _input) noexcept {
    return sha3Secure(bytesConstRef(_input));
}

/// Keccak hash variant optimized for hashing 256-bit hashes.
inline h256 sha3(h256 const& _input) noexcept {
    // ethash::hash256 hash = ethash::keccak256_32(_input.data());

    // uint8_t h[32];
    // _keccak_256(_input.data(), _input.size, h);
    // return h256{h, h256::ConstructFromPointer};
    return sha3(_input.ref());
}

/// Calculate SHA3-256 hash of the given input (presented as a FixedHash), returns a 256-bit hash.
template <unsigned N>
inline h256 sha3(FixedHash<N> const& _input) noexcept {
    return sha3(_input.ref());
}

template <unsigned N>
inline SecureFixedHash<32> sha3Secure(FixedHash<N> const& _input) noexcept {
    return sha3Secure(_input.ref());
}

/// Fully secure variants are equivalent for sha3 and sha3Secure.
inline SecureFixedHash<32> sha3(bytesSec const& _input) noexcept {
    return sha3Secure(_input.ref());
}

inline SecureFixedHash<32> sha3Secure(bytesSec const& _input) noexcept {
    return sha3Secure(_input.ref());
}

template <unsigned N>
inline SecureFixedHash<32> sha3(SecureFixedHash<N> const& _input) noexcept {
    return sha3Secure(_input.ref());
}

template <unsigned N>
inline SecureFixedHash<32> sha3Secure(SecureFixedHash<N> const& _input) noexcept {
    return sha3Secure(_input.ref());
}

/// Calculate SHA3-256 hash of the given input, possibly interpreting it as nibbles, and return the hash as a string filled with binary data.
inline std::string sha3(std::string const& _input, bool _isNibbles) {
    return asString((_isNibbles ? sha3(fromHex(_input)) : sha3(bytesConstRef(&_input))).asBytes());
}

/// Calculate SHA3-256 MAC
inline void sha3mac(bytesConstRef _secret, bytesConstRef _plain, bytesRef _output) {
    sha3(_secret.toBytes() + _plain.toBytes()).ref().populate(_output);
}

extern h256 const EmptySHA3;

extern h256 const EmptyListSHA3;

}  // namespace dev
