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

        _Account acc;  // we cannot hold reference since MP# does not contain c++ object that can be modified by reference
        _Storage& st;  // we can hols reference due to storage is c++ map object

        // template <
        //     typename T,
        //     typename U,
        //     typename = std::enable_if_t<std::is_base_of<_Account, T>::value>,
        //     typename = std::enable_if_t<std::is_base_of<_Storage, U>::value>>
        // AccountState(std::pair<T, U>& p)
        //   : acc(p.first), st(p.second) {}

        // AccountState(const _Account& a, const _Storage& s)
        //   : acc(a), st(s)
        // {}

        AccountState(const _Account&& a, _Storage& s)  // move ctor
          : acc(std::move(a)), st(s)
        {}

        AccountState(const AccountState& other)  // copy ctor
          : acc(other.acc), st(other.st)
        {}

        AccountState(const AccountState&& other)     // move ctor
          : acc(std::move(other.acc)), st(other.st)  // storage is never moved since it is just ref
        {}

        AccountState& operator=(const AccountState& other) = delete;  // disable = operator
    };

    using SimpleAccountState = AccountState<SimpleAccount, SimpleStorage>;


    /**
   * Abstract interface for interacting with EVM world state
   */
    template <class _A, class _S>
    struct GlobalState {
        virtual void remove(const Address& addr) = 0;

        virtual ~GlobalState() {}

        using GenericStateEntry = std::pair<_A, _S>;

        /**
     * Creates a new zero-initialized account under the given address if none exists
     */

        virtual AccountState<_A, _S> get(const Address& addr) = 0;

        virtual AccountState<_A, _S> create(const Address& addr, const uint256_t& balance, const Code& code) = 0;

        virtual AccountState<_A, _S> update(const Address& addr, const GenericStateEntry& p) = 0;

        virtual const Block& get_current_block() = 0;
        virtual uint256_t get_block_hash(uint8_t offset) = 0;
    };

}  // namespace eevm
