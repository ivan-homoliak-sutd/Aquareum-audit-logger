// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "account.h"
#include "aleth-mp3/database/StateCacheDB.h"
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
        _Storage& st;  // we can hold reference since to storage is c++ map object

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

        AccountState()
          : acc(), st(*(new _Storage()))  // IH: should be resolved nicer
        {
            // this should never happen, but template generation engine for unordered_map<.., AccountState<>> requires DEFINITION of defaut constructor
            throw std::logic_error("AccountState must not be constructed by default constructor.");
        }

        AccountState(const _Account&& a, _Storage& s)  // move ctor
          : acc(a), st(s)                              // TODO: is std::move OK here??
        {}

        AccountState(const _Account& a, _Storage& s)  // copy ctor
          : acc(a), st(s)
        {}

        AccountState(const AccountState& other)  // copy ctor
          : acc(other.acc), st(other.st)
        {}

        AccountState(const AccountState&& other)     // move ctor
          : acc(std::move(other.acc)), st(other.st)  // storage is never moved since it is just a ref
        {}

        AccountState& operator=(const AccountState& other) = delete;  // disable = operator
    };

    using SimpleAccountState = AccountState<SimpleAccount, SimpleStorage>;


    /**
   * Abstract interface for interacting with EVM world state
   */
    template <class _A, class _S>
    struct GlobalState {
    public:
        using StateEntry = std::pair<_A, _S>;  // SimpleStorage is just std::map

        virtual void remove(const Address& addr) = 0;

        virtual ~GlobalState() {}

        using GenericStateEntry = std::pair<_A, _S>;

        /**
     * Creates a new zero-initialized account under the given address if none exists
     */

        virtual AccountState<_A, _S> get(const Address& addr) = 0;

        virtual AccountState<_A, _S> create(const Address& addr, const uint256_t& balance, const Code& code) = 0;

        virtual AccountState<_A, _S> update(const Address& addr, const GenericStateEntry& p) = 0;

        virtual bool exists(const Address& addr) = 0;

        virtual const Block& get_current_block() = 0;

        virtual uint256_t get_block_hash(uint8_t offset) = 0;

        virtual const h256 root() = 0;

        virtual _S& getStorage(const Address& addr) = 0;

        virtual void purgeStaleEntriesInDB() = 0;

        virtual StateCacheDB::StorageStatsMP3DB getDbStorageStats() = 0;

        virtual unsigned getDbStorageItems() = 0;

        virtual size_t getStoragesDataSize() = 0;

        virtual void startASLogging(std::unordered_set<eevm::Address>* newAndUpdatedAddrs) = 0;

        virtual void startDBLookupLogging(const std::set<h256>* db_keys_existing, std::set<h256>* db_keys_new, std::vector<uint8_t>* db_data_aux, const uint16_t fragIdx = 0) = 0;

        virtual unsigned finishDBLookupLogging(const uint16_t fragIdx = 0) = 0;

        virtual void finishASLogging() = 0;

        virtual const uint16_t cntFrags() = 0;
    };

    using GlobalStateGeneric = GlobalState<SimpleAccount, SimpleStorage>;

}  // namespace eevm
