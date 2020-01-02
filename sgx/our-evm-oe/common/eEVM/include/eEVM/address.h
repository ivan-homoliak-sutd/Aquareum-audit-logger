// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "bigint.h"

#include "aleth-mp3/FixedHash.h"


namespace eevm
{
    // NOTE: Addresses will only use the low 160-bits, but it is simpler to use
    // overloads to serialise/pass these as any other 256-bit value. This is how
    // they are stored in EVM bytecode/memory.
    using Address = uint256_t;

    const size_t ADDR_SIZE_B = 32;
    const size_t ADDR_ETH_SIZE_B = 20;

    struct addr_as_hash {
        /// Make a hash of the object's data.
        size_t operator()(Address const& _value) const
        {
            return dev::h256::hash()(dev::h256(_value));
        }
    };

    inline void addr_u256_to_eth160b(const Address& addr, uint8_t* out)
    {
        uint8_t tmp[ADDR_SIZE_B];
        intx::be::unsafe::store(tmp, addr);
        memcpy(out, tmp + (ADDR_SIZE_B - ADDR_ETH_SIZE_B), ADDR_ETH_SIZE_B);
    }
}  // namespace eevm


namespace std
{
    template <>
    struct hash<eevm::Address> : eevm::addr_as_hash {
    };
}  // namespace std
