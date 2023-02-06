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

#include "merkle-tree.h"

using namespace dev;

namespace eevm
{
    /**
   * MP3 from Aleth is used as a state preserving object
   */
    class FragmentedGlobalState : public GlobalState<SimpleAccount, SimpleStorage> {
    public:
        // using StateEntry = std::pair<SimpleAccount, SimpleStorage>;  // SimpleStorage is just std::map
        const static uint16_t DEFAULT_FRAGS_CNT = 256;

    private:
        Block* m_currentBlock;  // not used so far

        const uint16_t m_cnt_frags = 0;  // the number of fragmented MP3s

        // the full global state fragmented into 'cntFrags' MP3 structures: containing all accounts splitted according to the 1st Byte of their addresses for simplifiing the concurrent access (except storages)
        std::vector<SecureTrieDB<h256, OverlayDB>> m_frag_accounts;

        std::vector<std::unordered_map<Address, SimpleStorage>> m_frag_storages;  // storages of all accounts (fragmented)

        void _dump_single_storage(Address addr, std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size, const uint16_t fragIdx) const;

        // Logging of DB entries in MP3
        bool m_db_logging[DEFAULT_FRAGS_CNT] = {false};             // indicates whether adresses of new accounts should be logged
        unsigned m_db_logged_entries_cnt[DEFAULT_FRAGS_CNT] = {0};  // counter of DB lookups that are logged in MP3 during the capture

        // Logging of new/updated ASes in MP3 (it is enough to log addrs since one AS can be modified many times)
        std::unordered_set<eevm::Address>* m_as_updatedAndNewAddrs;  // NULL indicates whether the logging of new AS is in place or not (should be used only in enclave)


    public:
        FragmentedGlobalState(const uint16_t cntFrags, bool init = true)
          : m_cnt_frags(cntFrags)
        {
            if (cntFrags != FragmentedGlobalState::DEFAULT_FRAGS_CNT) {
                throw std::logic_error("Only default number of fragments is currently supported.");
            }

            for (auto i = 0; i < cntFrags; i++) {
                auto db =
                    new OverlayDB(std::move(
                        std::unique_ptr<db::DatabaseFace>(
                            new db::MemoryDB())));  // MemoryDB is (currently) just a surrogate for the real persistant DB  (LevelDB)

                m_frag_accounts.push_back(std::move(db));
                m_frag_accounts.back().setFragIdx(i);

                if (init)
                    m_frag_accounts[i].init();  // create empty node and insert it into MP3 of each fragment
            }
            m_as_updatedAndNewAddrs = NULL;
        };

        ~FragmentedGlobalState() = default;

        virtual void remove(const Address& addr) override;


        // state & storage getters
        inline const uint16_t cntFrags()
        {
            return m_cnt_frags;
        }
        inline std::vector<SecureTrieDB<h256, OverlayDB>>& getFrags()
        {
            return m_frag_accounts;
        }
        inline SecureTrieDB<h256, OverlayDB>& getAccounts(uint16_t fragIdx)
        {
            return m_frag_accounts[fragIdx];
        }
        inline std::unordered_map<Address, SimpleStorage>& getStorages(uint16_t fragIdx)
        {
            return m_frag_storages[fragIdx];
        }
        inline SimpleStorage& getStorage(const Address& addr)
        {
            auto addrAsHash = h256(addr);
            uint16_t fragIdx = addrAsHash[0];
            return m_frag_storages[fragIdx].at(addr);
        }
        inline unsigned getDbStorageItems()
        {
            unsigned sum = 0;
            for (size_t i = 0; i < DEFAULT_FRAGS_CNT; i++) {
                sum += m_frag_storages[i].size();
            }
            return sum;
        }
        inline size_t getStoragesDataSize()
        {
            unsigned sum = 0;
            for (size_t i = 0; i < DEFAULT_FRAGS_CNT; i++) {
                sum += getStoragesDataSize(i);
            }
            return sum;
        }
        inline size_t getStoragesDataSize(uint16_t fragIdx)
        {
            size_t sumSize = 0;
            for (auto& p : m_frag_storages[fragIdx]) {
                sumSize += p.second.sizeB();
            }
            return sumSize;
        }

        inline StateCacheDB::StorageStatsMP3DB getDbStorageStats()
        {
            auto fullstats = StateCacheDB::StorageStatsMP3DB();

            for (size_t i = 0; i < DEFAULT_FRAGS_CNT; i++) {
                auto& sti = m_frag_accounts[i].db()->m_stats;
                fullstats.size_main_data += sti.size_main_data;
                fullstats.size_aux_data += sti.size_aux_data;
                fullstats.size_main_keys += sti.size_main_keys;
                fullstats.size_aux_keys += sti.size_aux_keys;
                fullstats.size_main_stale += sti.size_main_stale;
            }

            return fullstats;  // hope RVO works here
        }

        inline void purgeStaleEntriesInDB()
        {
            for (size_t i = 0; i < DEFAULT_FRAGS_CNT; i++) {
                m_frag_accounts[i].db()->purge();
            }
        }

        inline db::MemoryDB* persDB(uint16_t fragIdx)
        {
            return dynamic_cast<db::MemoryDB*>(m_frag_accounts[fragIdx].db()->db().get());
        }
        inline OverlayDB* db(uint16_t fragIdx)
        {
            return dynamic_cast<OverlayDB*>(m_frag_accounts[fragIdx].db());
        }
        inline const h256& rootOfFragMP3(uint16_t fragIdx)
        {
            return m_frag_accounts[fragIdx].root();
        }
        inline const h256 root()
        {
            // build a temporary Merkle tree and return its root

            auto merkleTree = MerkleTreeArray();

            for (size_t i = 0; i < m_cnt_frags; i++) {
                merkleTree.add(m_frag_accounts[i].root());
            }

            return std::move(merkleTree.computeRoot());
        }

        inline void commitPersDB()
        {
            // flushes state cache to persistant DB | should be called only in HOST, not enclave
            for (size_t i = 0; i < m_cnt_frags; i++) {
                this->m_frag_accounts[i].db()->commit();
            }
        }

        size_t num_frag_accounts(const uint16_t fragIdx);

        // void dump_full_db_of_frag(std::vector<uint8_t>& mp3_keys,
        //   std::vector<uint8_t>& mp3_values,
        //   std::vector<size_t>& values_sizes,
        //   size_t& mp3_keys_size, size_t& values_sizes_size,
        //   std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size, const uint16_t fragIdx);

        void dump_full_db(std::vector<uint8_t>& mp3_keys,
                          std::vector<uint8_t>& mp3_values,
                          std::vector<size_t>& values_sizes,
                          size_t& mp3_keys_size, size_t& values_sizes_size,
                          std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size);

        void dump_partial_db(std::set<Address>& txs,
                             std::vector<uint8_t>& db_data, std::set<h256>& db_keys,
                             std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes,
                             size_t& storages_sizes_size, std::vector<uint8_t>& acnts_storages);

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
            assert(!m_db_logging[fragIdx]);
            m_db_logging[fragIdx] = true;
            m_frag_accounts[fragIdx].startLookupLoggingMP3(db_keys_existing, db_keys_new, db_data_aux, &m_db_logged_entries_cnt[fragIdx]);
        }

        inline unsigned finishDBLookupLogging(const uint16_t fragIdx)
        {
            assert(m_db_logging[fragIdx]);
            m_db_logging[fragIdx] = false;
            m_frag_accounts[fragIdx].finishLookupLoggingMP3();
            return m_db_logged_entries_cnt[fragIdx];
        }

        static int construct_full_state_of_frag(FragmentedGlobalState** out_gs, const uint8_t* mp3_keys, size_t mp3_keys_size,
                                                const uint8_t* mp3_values, const size_t* values_sizes, size_t mp3_values_sizes_size,
                                                const uint8_t* storages, const size_t* storages_sizes, size_t storages_sizes_size, const uint16_t frag_Idx);

        static int construct_full_state(FragmentedGlobalState** out_gs, const uint8_t* mp3_keys, size_t mp3_keys_size,
                                        const uint8_t* mp3_values, const size_t* values_sizes, size_t mp3_values_sizes_size,
                                        const uint8_t* storages, const size_t* storages_sizes, size_t storages_sizes_size);


        static int construct_partial_state(FragmentedGlobalState** out_gs,
                                           const uint8_t* gs_root_frag_h,
                                           const uint8_t* db_data, const size_t db_data_size,
                                           const uint8_t* db_data_aux, const size_t db_data_aux_size,
                                           const uint8_t* storages, const size_t* storages_sizes,
                                           const size_t storages_sizes_size, const uint8_t* acnts_storages);


        // friend void to_json(nlohmann::json&, const NormalGlobalState&);
        // friend void from_json(const nlohmann::json&, NormalGlobalState&);
        friend void from_json(const nlohmann::json&, SimpleAccount&);

        // Interface methods
        AccountState<SimpleAccount, SimpleStorage> get(const Address& addr) override;
        AccountState<SimpleAccount, SimpleStorage> create(const Address& addr, const uint256_t& balance, const Code& code) override;
        AccountState<SimpleAccount, SimpleStorage> update(const Address& addr, const StateEntry& p) override;

        bool exists(const Address& addr) override;

        virtual const Block& get_current_block() override;
        virtual uint256_t get_block_hash(uint8_t offset) override;
    };

    // void to_json(nlohmann::json&, const NormalGlobalState&);
    // void from_json(const nlohmann::json&, NormalGlobalState&);
    // bool operator==(const NormalGlobalState&, const NormalGlobalState&);
}  // namespace eevm
