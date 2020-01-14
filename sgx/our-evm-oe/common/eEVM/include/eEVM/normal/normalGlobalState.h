// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "eEVM/globalstate.h"
#include "eEVM/simple/simpleaccount.h"
#include "eEVM/simple/simplestorage.h"
#include "eEVM/transaction.h"

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
    class NormalGlobalState : public GlobalState<SimpleAccount, SimpleStorage> {
    public:
        using StateEntry = std::pair<SimpleAccount, SimpleStorage>;  // SimpleStorage is just std::map


    private:
        Block currentBlock;  // not used so far

        SecureTrieDB<h256, OverlayDB> m_accounts;  // full global state: all accounts (except storages)

        std::unordered_map<Address, SimpleStorage> m_storages;  // storages of all accounts

        void _dump_single_storage(Address addr, std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size) const;

        // std::vector<Address>* m_log_created_accounts = NULL;
        bool m_accnt_logging = false;  // indicates whether adresses of new accounts should be logged

    public:
        NormalGlobalState(bool init = true)
          : m_accounts(
                new OverlayDB(std::move(
                    std::unique_ptr<db::DatabaseFace>(
                        new db::MemoryDB()))))  // MemoryDB is just a surrogate for the real persistant DB
        {
            if (init)
                m_accounts.init();  // create empty node into MP3
        };

        ~NormalGlobalState() = default;

        virtual void remove(const Address& addr) override;

        // state & storage getters
        inline SecureTrieDB<h256, OverlayDB>& getAccounts() { return m_accounts; }
        inline std::unordered_map<Address, SimpleStorage>& getStorages() { return m_storages; }

        inline db::MemoryDB* persDB() { return dynamic_cast<db::MemoryDB*>(m_accounts.db()->db().get()); }
        inline OverlayDB* db() { return dynamic_cast<OverlayDB*>(m_accounts.db()); }
        inline const h256& root() { return m_accounts.root(); }

        inline void commitPersDB() { this->m_accounts.db()->commit(); }  // flushes state cache to persistant DB

        AccountState<SimpleAccount, SimpleStorage> get(const Address& addr) override;
        AccountState<SimpleAccount, SimpleStorage> create(const Address& addr, const uint256_t& balance, const Code& code) override;
        AccountState<SimpleAccount, SimpleStorage> update(const Address& addr, const StateEntry& p) override;

        bool exists(const Address& addr) override;
        size_t num_accounts();

        virtual const Block& get_current_block() override;
        virtual uint256_t get_block_hash(uint8_t offset) override;

        void dump_full_db(std::vector<uint8_t>& mp3_keys,
                          std::vector<uint8_t>& mp3_values,
                          std::vector<size_t>& values_sizes,
                          size_t& mp3_keys_size, size_t& values_sizes_size,
                          std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size);

        void dump_partial_db(std::vector<Address>& txs,
                             bytes& db_data, std::set<h256>& db_keys,
                             std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes,
                             size_t& storages_sizes_size, std::vector<uint8_t>& acnts_storages);

        /**
         * For tests which require some initial state, allow manual insertion of pre-constructed accounts
         */
        void insert(const StateEntry& e);

        inline void startInsertLogging(std::set<h256>* db_keys, std::vector<uint8_t>* db_data_aux)
        {
            assert(!m_accnt_logging);
            m_accnt_logging = true;
            m_accounts.startInsertLogging(db_keys, db_data_aux);
        }

        inline void finishInsertLogging()
        {
            assert(m_accnt_logging);
            m_accounts.finishInsertLogging();
            m_accnt_logging = false;
        }

        static int construct_full_state(NormalGlobalState** out_gs, const uint8_t* mp3_keys, size_t mp3_keys_size,
                                        const uint8_t* mp3_values, const size_t* values_sizes, size_t mp3_values_sizes_size,
                                        const uint8_t* storages, const size_t* storages_sizes, size_t storages_sizes_size);


        static int construct_partial_state(NormalGlobalState** out_gs, const uint8_t* gs_root_h,
                                           const uint8_t* db_data, size_t db_data_size,
                                           const uint8_t* db_data_aux, size_t db_data_aux_size,
                                           const uint8_t* storages, const size_t* storages_sizes,
                                           size_t storages_sizes_size, std::vector<uint8_t>& acnts_storages);


        // friend void to_json(nlohmann::json&, const NormalGlobalState&);
        // friend void from_json(const nlohmann::json&, NormalGlobalState&);
        friend void from_json(const nlohmann::json&, SimpleAccount&);
    };

    // void to_json(nlohmann::json&, const NormalGlobalState&);
    // void from_json(const nlohmann::json&, NormalGlobalState&);
    // bool operator==(const NormalGlobalState&, const NormalGlobalState&);
}  // namespace eevm
