// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/exception.h"
#include "eEVM/simple/simpleglobalstate.h"
#include "eEVM/tracing.h"

#include "eEVM/bigint.h"

#include "aleth-mp3/FixedHash.h"
#include "aleth-mp3/TrieCommon.h"
#include <fmt/format_header_only.h>

using namespace dev;

namespace eevm
{
    void NormalGlobalState::remove(const Address& addr)
    {
        m_accounts.remove(h256(addr));
    }

    // It does NOT creates a new account state if it does not exist! This is different from original eEVM proposal, so the processor might fail in some cases.
    SimpleAccountState NormalGlobalState::get(const Address& addr)
    {
        TRACE_ME("get addr: %s ", to_hex_string(addr).c_str());
        if (!m_accounts.contains(h256(addr))) {
            INTERNAL_EXCEPTION(fmt::format("Requested account {} does not exist.", to_hex_string(addr).c_str()));
        }

        std::string acnt_json = m_accounts.at(h256(addr));

        // populate account object
        auto j = nlohmann::json::parse(acnt_json);
        SimpleAccount a;
        from_json(j, a);
        assert(a.get_address() == addr);

        // fetch account's data from storage map
        SimpleStorage& s = m_storages.at(addr);      // TODO: resolve non-existing
        return SimpleAccountState(std::move(a), s);  // IH: hope c++ RVO works here
    }

    SimpleAccountState NormalGlobalState::create(const Address& addr, const uint256_t& balance, const Code& code)
    {
        TRACE_ME("create account: %s ", to_hex_string(addr).c_str());
        insert(std::make_pair(SimpleAccount(addr, balance, code), SimpleStorage()));
        assert(m_accounts.contains(h256(addr)));
        return get(addr);
    }

    bool NormalGlobalState::exists(const Address& addr) { return m_accounts.contains(h256(addr)); }
    size_t NormalGlobalState::num_accounts() { return (dynamic_cast<db::MemoryDB*>(m_accounts.db()->db().get()))->size(); }
    const Block& NormalGlobalState::get_current_block() { return currentBlock; }
    uint256_t NormalGlobalState::get_block_hash(uint8_t offset) { return 0u; /* IH: cool */ }

    SimpleAccountState NormalGlobalState::update(const Address& addr, const StateEntry& p)
    {
        TRACE_ME("update account: %s ", to_hex_string(addr).c_str());

        // the updated entry does not need to be removed !!!

        // a) standard removal of node in MP3
        // m_accounts.remove(h256(addr));  // TODO: IH replace remove for direct delecting from DB by forceKillNode. There is no need to update the MP3

        // b) removal only from DB
        // std::string rlpStrOld = m_accounts.at(h256(addr));
        // m_accounts.killNodeWrapper(dev::RLP(rlpStrOld));

        insert(p);
        assert(m_accounts.contains(h256(addr)));
        return get(addr);
    }

    void NormalGlobalState::insert(const StateEntry& _p)
    {
        // auto _p = const_cast<StateEntry&>(p);
        auto addr = _p.first.get_address();

        std::vector<uint8_t> value;
        m_accounts.insert(h256(addr), _p.first.asJsonBytes(value));
        assert(m_accounts.contains(h256(addr)));

        m_storages[addr] = _p.second;  // IH: TODO this could be omitted by some explicit bool flag indicating a change/not in storage has occured
    }

    /**
     * It dumps 'partial' global state of MP3 related to all transactions in txs. It uses iteration trails of MP3 to build this partial state.
     * The results is stored into 'data'
     */
    void NormalGlobalState::dump_partial_db(std::vector<Address>& addrs_to_process,
                                            bytes& db_data, std::set<h256>& db_keys,
                                            std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes,
                                            size_t& storages_sizes_size, std::vector<uint8_t>& acnts_storages)
    {
        size_t sum_size_data = 0;
        size_t size_data_before = db_data.size();

        for (auto& addr : addrs_to_process) {
            assert(exists(addr));
            auto it = m_accounts.lower_bound(addr);  // get iterator to the current account in MP3

            // pass all nodes in the trail of the iterator and store unique ones
            auto& trail = it.get_trail();
            for (auto& node : trail) {
                RLP rlp = RLP(node.rlp);
                h256 h = sha3(rlp.data());

                if (db_keys.end() == db_keys.find(h)) {                                   // check for duplicity
                    db_data.insert(db_data.end(), rlp.data().begin(), rlp.data().end());  // insert only RLP of db entry (hash can be computed later)
                    sum_size_data += rlp.data().size();
                    db_keys.insert(h);
                }
            }

            // dump full storage of the recepient account
            this->_dump_single_storage(addr, storages, storages_sizes, storages_sizes_size);
            uint8_t addr_as_be[ADDR_SIZE_B];
            to_big_endian(addr, addr_as_be);
            acnts_storages.insert(acnts_storages.end(), addr_as_be, addr_as_be + ADDR_SIZE_B);
        }
        assert(sum_size_data == db_data.size() - size_data_before);
    }


    /**
     * It dumps the full MP3 state of accounts and their storages into several references.
     * It iterates MP3 entries through MP3's iterator (thus only leaf nodes are considered)
     */
    void NormalGlobalState::dump_full_db(std::vector<uint8_t>& mp3_keys,
                                         std::vector<uint8_t>& mp3_values,
                                         std::vector<size_t>& values_sizes,
                                         size_t& mp3_keys_size, size_t& values_sizes_size,
                                         std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size)
    {
        TRACE_ME("Dumping full DB of global state");
        values_sizes_size = 0, storages_sizes_size = 0;
        size_t summed_keys_size = 0;
        unsigned cnt_entries = 0;

        // int i = 0;
        for (auto const& e : m_accounts) {  // std::pair<bytesConstRef, bytesConstRef>
            auto addr = e.first;
            auto val = e.second;

            // std::cout << "\t account[" << i++ << "] addr = " << addr << "value = " << escaped(val.toString(), false) << "\n";
            mp3_keys.insert(mp3_keys.end(), addr.begin(), addr.end());    // insert the full content of value
            mp3_values.insert(mp3_values.end(), val.begin(), val.end());  // insert the full content of key
            values_sizes.push_back(val.size());

            summed_keys_size += addr.size;
            values_sizes_size += sizeof(size_t);

            // dump also storage of each account
            _dump_single_storage(addr, storages, storages_sizes, storages_sizes_size);
            cnt_entries++;
        }
        mp3_keys_size = cnt_entries * 32;
        assert(mp3_keys_size == summed_keys_size);
        // print_sep();
    }


    void NormalGlobalState::_dump_single_storage(Address addr, std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size) const
    {
        // TRACE_ME("dumping storage of addr = %s", to_hex_string(addr).c_str());
        auto const& cur_storage = m_storages.at(addr);  // if 'addr' does not exists, just raise exception

        size_t cur_storage_size = cur_storage.toBytes(storages);  // updates 'storages' vector

        storages_sizes.push_back(cur_storage_size);
        storages_sizes_size += sizeof(size_t);  // account for the size variable
    }

    ////////////////////////////// Static Methods //////////////////////////////

    /**
     * Constructs partial NormalGlobalState object from parameters passed. (called from enclave)
     * Note that also integrity of copied storages is verified here, since they are passed to as [user_check]
     */
    static int construct_partial_state(NormalGlobalState** gs, const uint8_t* gs_root_h,
                                       const uint8_t* db_data, size_t db_data_size,
                                       const uint8_t* db_data_aux, size_t db_data_aux_size,
                                       const uint8_t* storages, const size_t* storages_sizes,
                                       size_t storages_sizes_size, const uint8_t* accnts_of_storages)
    {
        TRACE_ME("Constructing partial state");

        *gs = new NormalGlobalState();
        auto& acnts = (*gs)->getAccounts();
        auto& strgs = (*gs)->getStorages();
        h256 root = h256(gs_root_h, h256::ConstructFromPointer);
        acnts.setRoot(root, Verification::Skip);  // set root of MP3 forcely (if it is different from the last known in E, then E exits)

        size_t sum_data_size = 0, sum_aux_size = 0;
        uint inserted_db_data_entries = 0;

        // 1) insert all passed RLP DB entries
        while (sum_data_size < db_data_size) {
            // construct full RLP of DB entry by parsing its length first
            auto len_size = RLP(db_data, 1).lengthSize();
            RLP rlp_len = RLP(db_data, 1 + len_size);
            size_t full_rlp_size = 1 + len_size + rlp_len.length();

            // construct key & value of DB entry
            RLP full_rlp = RLP(db_data, full_rlp_size);
            h256 h = sha3(full_rlp.data());

            // insert DB entry directly into DB without touching MP3 API
            (*gs)->db()->insert(h, (full_rlp.data()));
            sum_data_size += full_rlp_size;
            inserted_db_data_entries++;
        }
        TRACE_ME("inserted_db_data_entries = %ld", inserted_db_data_entries);
        assert(sum_data_size == db_data_size);

        // 2) verify whether DB entry with the claimed root value exists after filling DB
        if (std::string() == (*gs)->db()->lookup(root)) {
            TRACE_ME("Passed root %s does not exist in DB.", root.hex().c_str());
            return 2;
        }

        // 3) insert all passed storages
        size_t ptr_storages = 0;  // indicates the current possition in storages
        size_t ptr_addrs = 0;     // indicates the current possition in accnts_of_storages
        for (uint i = 0; i < storages_sizes_size / sizeof(size_t); i++) {
            Address addr = from_big_endian(&(accnts_of_storages[ptr_addrs]));
            SimpleStorage* s = SimpleStorage::fromBytes(&storages[ptr_storages], storages_sizes[i]);
            strgs.insert(std::make_pair(std::move(addr), std::move(*s)));

            // 3a) verify integrity of storages
            if (s->hash() != (*gs)->get(addr).acc.get_stHash()) {
                TRACE_ME("Mismatch of storage hash for addr = %s", to_hex_string(addr).c_str());
                return 1;
            }
            ptr_storages += storages_sizes[i];
            ptr_addrs += ADDR_SIZE_B;
        }

        // 4) insert all passed auxiliary RLP DB entries
        inserted_db_data_entries = 0;
        while (sum_aux_size < db_data_aux_size) {
            // construct full RLP of DB entry by parsing its length first
            auto len_size = RLP(db_data_aux, 1).lengthSize();
            RLP rlp_len = RLP(db_data_aux, 1 + len_size);
            size_t full_rlp_size = 1 + len_size + rlp_len.length();

            // construct key & value of DB entry
            RLP full_rlp = RLP(db_data_aux, full_rlp_size);
            h256 h = sha3(full_rlp.data());

            // insert DB entry directly into DB without touching MP3 API
            acnts.db().insert(h, (full_rlp.data()));
            sum_aux_size += full_rlp_size;
            inserted_db_data_entries++;
        }
        TRACE_ME("inserted_db_aux_entries = %ld", inserted_db_data_entries);
        assert(sum_aux_size == db_data_aux_size);
        return 0;
    }

    /**
     * Constructs full NormalGlobalState object from parameters passed. (called from enclave)
     */
    int NormalGlobalState::construct_full_state(NormalGlobalState** gs,
                                                const uint8_t* mp3_keys, size_t mp3_keys_size,
                                                const uint8_t* mp3_values, const size_t* values_sizes, size_t mp3_values_sizes_size,
                                                const uint8_t* storages, const size_t* storages_sizes, size_t storages_sizes_size)
    {
        TRACE_ME("Constructing full state");
        assert(mp3_keys_size / ADDR_SIZE_B == mp3_values_sizes_size / sizeof(size_t));

        *gs = new NormalGlobalState();
        auto& acnts = (*gs)->getAccounts();
        auto& strgs = (*gs)->getStorages();

        size_t ptr_mp3_values = 0;  // indicates the current possition in mp3_values
        size_t ptr_storages = 0;    // indicates the current possition in storages

        // 1) insert account states one by one to global MP3
        for (size_t i = 0; i < mp3_values_sizes_size / sizeof(size_t); i++) {
            // TRACE_ME("[%ld]", i);
            auto* key = new h256(&(mp3_keys[i * ADDR_SIZE_B]), h256::ConstructFromPointer);  // ctor of h256 allocates memory
            uint8_t* val = new uint8_t[values_sizes[i]];                                     // manually allocating enclave memory since 'mp3_values' is in host memory
            memcpy(val, &mp3_values[ptr_mp3_values], values_sizes[i]);
            auto val_ref = bytesConstRef(val, values_sizes[i]);

            // std::cerr << " inserting entry: " << key << " => " << escaped(val_ref.toString(), false) << "\n";
            acnts.insert(*key, val_ref);
            ptr_mp3_values += values_sizes[i];

            // 2) insert storage of the current account state
            SimpleStorage* s = SimpleStorage::fromBytes(&storages[ptr_storages], storages_sizes[i]);
            strgs.insert(std::make_pair(std::move(*key), std::move(*s)));

            ptr_storages += storages_sizes[i];
            delete val;
        }
        // print_sep();
        return 0;
    }

    // void to_json(nlohmann::json& j, const NormalGlobalState& s)
    // {
    //     j["block"] = s.currentBlock;
    //     auto o = nlohmann::json::array();

    //     // items of iterator for MP3 are std::pair<bytesConstRef, bytesConstRef> // maybe interepret second?
    //     for (const auto& p : s.m_accounts) {
    //         // o.push_back({to_hex_string(p.first), p.second});
    //         o.push_back({to_hex_string(p.first), to_hex_string(p.second.begin(), p.second.end())});
    //     }
    //     j["accounts"] = o;
    // }

    // void from_json(const nlohmann::json& j, NormalGlobalState& s)
    // {
    //     if (j.find("block") != j.end()) {
    //         s.currentBlock = j["block"];
    //     }

    //     for (const auto& it : j["accounts"].items()) {
    //         const auto& v = it.value();
    //         // a.m_accounts.insert(make_pair(to_uint256(v[0]), v[1]));
    //         s.m_accounts.insert(to_uint256(v[0]), bytesConstRef(v[1]));

    //         // TODO: persist storages later
    //         // s.m_storages[to_uint256(v[0])] = v[1];
    //     }
    // }

}  // namespace eevm
