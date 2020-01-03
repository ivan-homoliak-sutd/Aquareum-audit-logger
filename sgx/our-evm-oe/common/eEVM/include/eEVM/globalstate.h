// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "account.h"
#include "block.h"
#include "simple/simpleaccount.h"
#include "simple/simplestorage.h"
#include "storage.h"

#include <map>

namespace eevm
{
    /**
   * An account and its storage
   */
    template <class _A, class _S>
    struct AccountState {
        using _Account = _A;
        using _Storage = _S;

        _Account acc;
        _Storage st;

        template <
            typename T,
            typename U,
            typename = std::enable_if_t<std::is_base_of<_Account, T>::value>,
            typename = std::enable_if_t<std::is_base_of<_Storage, U>::value> >
        AccountState(std::pair<T, U>& p)
          : acc(p.first), st(p.second) {}

        AccountState() {}
        constexpr AccountState(const AccountState& other)  // copy ctor
          : acc(other.acc), st(other.st)
        {}
        constexpr explicit AccountState(AccountState&& other) = default;  // movable ctor
        AccountState& operator=(const AccountState&) = default;


        AccountState(_Account& acc, _Storage& st)
          : acc(acc), st(st) {}
    };

    using SimpleAccountState = AccountState<SimpleAccount, SimpleStorage>;


    /**
   * Abstract interface for interacting with EVM world state
   */
    template <class _A, class _S>
    struct GlobalState {
        virtual void remove(const Address& addr) = 0;

        virtual ~GlobalState() {}

        /**
     * Creates a new zero-initialized account under the given address if none exists
     */

        virtual AccountState<_A, _S> get(const Address& addr, bool insert = true) = 0;

        virtual AccountState<_A, _S> create(const Address& addr, const uint256_t& balance, const Code& code) = 0;

        virtual const Block& get_current_block() = 0;
        virtual uint256_t get_block_hash(uint8_t offset) = 0;
    };

}  // namespace eevm
