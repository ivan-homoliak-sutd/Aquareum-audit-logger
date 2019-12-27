// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "eEVM/globalstate.h"
#include "eEVM/simple/simpleaccount.h"
#include "eEVM/simple/simplestorage.h"

#include "aleth-mp3/Common.h"
#include "aleth-mp3/database/MemoryDB.h"
#include "aleth-mp3/database/OverlayDB.h"
#include "aleth-mp3/database/SecureTrieDB.h"

using namespace dev;

namespace eevm
{
    /**
   * MP3 from Aleth is used as state preserving object
   */
    class NormalGlobalState : public GlobalState {
    public:
        using StateEntry = std::pair<SimpleAccount, SimpleStorage>;  // SimpleStorage is just std::map


    private:
        Block currentBlock; // not used so far

        SecureTrieDB<h256, OverlayDB> m_accounts;  // full global state: all accounts (except storages)

        std::unordered_map<Address, SimpleStorage> m_storages;  // storages of all accounts

        void _dump_single_storage(Address addr, std::vector<uint8_t>* storages, std::vector<size_t>* storages_sizes, size_t& storages_sizes_size) const;

    public:
        NormalGlobalState()
          : m_accounts(
                new OverlayDB(std::move(
                    std::unique_ptr<db::DatabaseFace>(new db::MemoryDB()))))
        {
            m_accounts.init();  // create empty node into MP3
        };

        virtual void remove(const Address& addr) override;

        inline db::MemoryDB* db() { return dynamic_cast<db::MemoryDB*>(m_accounts.db()->db().get()); }
        inline const h256& root() { return m_accounts.root(); }


        // inline std::unordered_map<Address, SimpleStorage> & storages() const { return m_storages; }

        AccountState get(const Address& addr) override;
        AccountState create(const Address& addr, const uint256_t& balance, const Code& code) override;

        bool exists(const Address& addr);
        size_t num_accounts();

        virtual const Block& get_current_block() override;
        virtual uint256_t get_block_hash(uint8_t offset) override;

        void dump_full_db(std::vector<std::string>* db_keys,
                          std::vector<std::string>* db_values,
                          std::vector<size_t>* values_sizes,
                          size_t& db_keys_size, size_t& values_sizes_size,
                          std::vector<uint8_t>* storages, std::vector<size_t>* storages_sizes, size_t& storages_sizes_size);

        /**
         * For tests which require some initial state, allow manual insertion of pre-constructed accounts
         */
        void insert(const StateEntry& e);

        static int construct_full_state(NormalGlobalState* out_gs, const uint8_t* db_keys, size_t db_keys_size,
                                        const uint8_t* db_values, const size_t* values_sizes, size_t db_values_sizes_size,
                                        uint8_t* const storages, const size_t* storages_sizes, size_t storages_sizes_size);

        friend void to_json(nlohmann::json&, const NormalGlobalState&);
        friend void from_json(const nlohmann::json&, NormalGlobalState&);
    };

    void to_json(nlohmann::json&, const NormalGlobalState&);
    void from_json(const nlohmann::json&, NormalGlobalState&);
    // bool operator==(const NormalGlobalState&, const NormalGlobalState&);
}  // namespace eevm
