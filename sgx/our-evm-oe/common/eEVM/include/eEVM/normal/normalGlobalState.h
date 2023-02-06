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
   * MP3 from Aleth is used as a state preserving object
   */
    class NormalGlobalState : public GlobalState<SimpleAccount, SimpleStorage> {
    public:
    private:
        Block* m_currentBlock;  // not used so far

        SecureTrieDB<h256, OverlayDB> m_accounts;  // full global state: all accounts (except storages)

        std::unordered_map<Address, SimpleStorage> m_storages;  // storages of all accounts

        void _dump_single_storage(Address addr, std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size) const;

        // Logging of DB entries in MP3
        bool m_db_logging = false;             // indicates whether adresses of new accounts should be logged
        unsigned m_db_logged_entries_cnt = 0;  // counter of DB lookups that are logged in MP3 during the capture

        // Logging of new/updated ASes in MP3 (it is enough to log addrs since one AS can be modified many times)
        std::unordered_set<eevm::Address>* m_as_updatedAndNewAddrs = NULL;  // NULL indicates whether the logging of new AS is in place or not (should be used only in enclave)

    public:
        NormalGlobalState(bool init = true)
          : m_accounts(
                new OverlayDB(std::move(
                    std::unique_ptr<db::DatabaseFace>(
                        new db::MemoryDB()))))  // MemoryDB is (currently) just a surrogate for the real persistant DB  (LevelDB)
        {
            if (init)
                m_accounts.init();  // create empty node and insert it into MP3
        };

        ~NormalGlobalState() = default;

        virtual void remove(const Address& addr) override;

        // state & storage getters
        inline SecureTrieDB<h256, OverlayDB>& getAccounts() { return m_accounts; }
        inline std::unordered_map<Address, SimpleStorage>& getStorages() { return m_storages; }
        inline unsigned getDbStorageItems() { return m_storages.size(); }
        inline size_t getStoragesDataSize()
        {
            size_t sumSize = 0;
            for (auto& p : m_storages) {
                sumSize += p.second.sizeB();
            }
            return sumSize;
        }
        inline SimpleStorage& getStorage(const Address& addr) { return m_storages.at(addr); }

        inline db::MemoryDB* persDB() { return dynamic_cast<db::MemoryDB*>(m_accounts.db()->db().get()); }
        inline OverlayDB* db() { return dynamic_cast<OverlayDB*>(m_accounts.db()); }
        inline void purgeStaleEntriesInDB() { m_accounts.db()->purge(); }
        inline const h256 root() { return m_accounts.root(); }

        inline const uint16_t cntFrags() { return 1; } // This MP3 has only one fragment

        inline void commitPersDB() { this->m_accounts.db()->commit(); }  // flushes state cache to persistant DB | should be called only in HOST, not enclave

        AccountState<SimpleAccount, SimpleStorage> get(const Address& addr) override;
        AccountState<SimpleAccount, SimpleStorage> create(const Address& addr, const uint256_t& balance, const Code& code) override;
        AccountState<SimpleAccount, SimpleStorage> update(const Address& addr, const StateEntry& p) override;

        bool exists(const Address& addr) override;
        size_t num_accounts();

        void dump_full_db(std::vector<uint8_t>& mp3_keys,
                          std::vector<uint8_t>& mp3_values,
                          std::vector<size_t>& values_sizes,
                          size_t& mp3_keys_size, size_t& values_sizes_size,
                          std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size);

        void dump_partial_db(std::set<Address>& txs,
                             std::vector<uint8_t>& db_data, std::set<h256>& db_keys,
                             std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes,
                             size_t& storages_sizes_size, std::vector<uint8_t>& acnts_storages);


        inline StateCacheDB::StorageStatsMP3DB getDbStorageStats()
        {
            return StateCacheDB::StorageStatsMP3DB(m_accounts.db()->m_stats);  // hope RVO works here
        }

        /**
         * For tests which require some initial state, allow manual insertion of pre-constructed accounts
         */
        void insert(const StateEntry& e);

        /**
         * @brief it starts/finishes logging of new and updated AS as well as storages during the execution of eEVM processor in E (so the host can update its MP3 w/o re-execution of txs)
         */
        inline void startASLogging(std::unordered_set<eevm::Address>* newAndUpdatedAddrs)
        {
            assert(NULL == m_as_updatedAndNewAddrs);
            m_as_updatedAndNewAddrs = newAndUpdatedAddrs;
        }

        inline void finishASLogging()
        {
            assert(NULL != m_as_updatedAndNewAddrs);
            m_as_updatedAndNewAddrs = NULL;
        }

        /**
         * @brief it starts/finish logging of DB entries of MP3
         */
        inline void startDBLookupLogging(const std::set<h256>* db_keys_existing, std::set<h256>* db_keys_new, std::vector<uint8_t>* db_data_aux, const uint16_t fragIdx)
        {
            assert(!m_db_logging);
            m_db_logging = true;
            m_accounts.startLookupLoggingMP3(db_keys_existing, db_keys_new, db_data_aux, &m_db_logged_entries_cnt);
        }

        inline unsigned finishDBLookupLogging(const uint16_t fragIdx = 0)
        {
            assert(m_db_logging);
            m_db_logging = false;
            m_accounts.finishLookupLoggingMP3();
            return m_db_logged_entries_cnt;
        }

        static int construct_full_state(NormalGlobalState** out_gs, const uint8_t* mp3_keys, size_t mp3_keys_size,
                                        const uint8_t* mp3_values, const size_t* values_sizes, size_t mp3_values_sizes_size,
                                        const uint8_t* storages, const size_t* storages_sizes, size_t storages_sizes_size);


        static int construct_partial_state(NormalGlobalState** out_gs, const uint8_t* gs_root_h,
                                           const uint8_t* db_data, size_t db_data_size,
                                           const uint8_t* db_data_aux, size_t db_data_aux_size,
                                           const uint8_t* storages, const size_t* storages_sizes,
                                           size_t storages_sizes_size, const uint8_t* acnts_storages);


        virtual const Block& get_current_block() override;
        virtual uint256_t get_block_hash(uint8_t offset) override;

        // friend void to_json(nlohmann::json&, const NormalGlobalState&);
        // friend void from_json(const nlohmann::json&, NormalGlobalState&);
        friend void from_json(const nlohmann::json&, SimpleAccount&);
    };

    // void to_json(nlohmann::json&, const NormalGlobalState&);
    // void from_json(const nlohmann::json&, NormalGlobalState&);
    // bool operator==(const NormalGlobalState&, const NormalGlobalState&);
}  // namespace eevm
