// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/normal/fragmentedGlobalState.h"
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
    // IH: TODO - resolve these
    const Block& FragmentedGlobalState::get_current_block() { throw std::logic_error("Not implemented."); }

    uint256_t FragmentedGlobalState::get_block_hash(uint8_t offset) { throw std::logic_error("Not implemented."); }

    void FragmentedGlobalState::remove(const Address& addr)
    {
        auto addrAsHash = h256(addr);
        uint16_t fragIdx = addrAsHash[0];
        TRACE_ME("remove account: MP3 frag idx = %d ", fragIdx);
        m_frag_accounts[fragIdx].remove(addrAsHash);
    }

    // It does NOT create a new account state if it does not exist!
    // This is different from original eEVM proposal, so the processor might fail in some eEVM test cases.
    SimpleAccountState FragmentedGlobalState::get(const Address& addr)
    {
        TRACE_ME("get addr: %s ", address_to_hex_string(addr).c_str());
        auto addrAsHash = h256(addr);
        uint16_t fragIdx = addrAsHash[0];
        TRACE_ME("get addr: MP3 frag idx = %d ", fragIdx);

        if (!m_frag_accounts[fragIdx].contains(addrAsHash)) {
            INTERNAL_EXCEPTION(fmt::format("Requested account {} does not exist.", to_hex_string(addr).c_str()));
        }

        std::string acnt_json = m_frag_accounts[fragIdx].at(addrAsHash);

        // populate account object
        auto j = nlohmann::json::parse(acnt_json);
        SimpleAccount a;
        from_json(j, a);
        assert(a.get_address() == addr);

        // fetch account's data from storage map
        assert(m_frag_storages[fragIdx].end() != m_frag_storages[fragIdx].find(addr));
        SimpleStorage& s = m_frag_storages[fragIdx].at(addr);  // TODO: resolve non-existing
        return SimpleAccountState(std::move(a), s);            // IH: hope c++ RVO works here
    }

    SimpleAccountState FragmentedGlobalState::create(const Address& addr, const uint256_t& balance, const Code& code)
    {
        TRACE_ME("create account: %s ", address_to_hex_string(addr).c_str());
        insert(std::make_pair(SimpleAccount(addr, balance, code), SimpleStorage()));

        auto addrAsHash = h256(addr);
        uint16_t fragIdx = addrAsHash[0];
        TRACE_ME("create account: MP3 frag idx = %d ", fragIdx);

        if (!m_frag_accounts[fragIdx].contains(addrAsHash)) {  // IH: used assert to make it faster before
            INTERNAL_EXCEPTION(fmt::format("The account was not created."));
        }

        SimpleAccountState newAS = get(addr);
        if (NULL != m_as_updatedAndNewAddrs[fragIdx]) {
            m_as_updatedAndNewAddrs[fragIdx]->insert(addr);
        }
        return newAS;
    }

    bool FragmentedGlobalState::exists(const Address& addr)
    {
        auto addrAsHash = h256(addr);
        uint16_t fragIdx = addrAsHash[0];
        TRACE_ME("exists account: MP3 frag idx = %d ", fragIdx);

        return m_frag_accounts[fragIdx].contains(addrAsHash);
    }
    size_t FragmentedGlobalState::num_frag_accounts(const uint16_t fragIdx)
    {
        return (dynamic_cast<db::MemoryDB*>(m_frag_accounts[fragIdx].db()->db().get()))->size();
    }

    SimpleAccountState FragmentedGlobalState::update(const Address& addr, const StateEntry& p)
    {
        TRACE_ME("update account: %s ", address_to_hex_string(addr).c_str());

        auto addrAsHash = h256(addr);
        uint16_t fragIdx = addrAsHash[0];
        TRACE_ME("update account: MP3 frag idx = %d ", fragIdx);

        // the updated entry does not need to be removed !!!

        // a) standard removal of node in MP3
        // m_frag_accounts[fragIdx].remove(h256(addr));

        // b) removal only from DB
        // std::string rlpStrOld = m_frag_accounts[fragIdx].at(h256(addr));
        // m_frag_accounts[fragIdx].killNodeWrapper(dev::RLP(rlpStrOld));

        insert(p);
        // TRACE_ME("x");
        assert(m_frag_accounts[fragIdx].contains(addrAsHash));

        // get new AC and log it (if AS logging enabled)
        SimpleAccountState newAS = get(addr);
        if (NULL != m_as_updatedAndNewAddrs[fragIdx]) {
            m_as_updatedAndNewAddrs[fragIdx]->insert(addr);
        }
        return newAS;
    }

    void FragmentedGlobalState::insert(const StateEntry& _p)
    {
        // auto _p = const_cast<StateEntry&>(p);
        auto addr = _p.first.get_address();
        auto addrAsHash = h256(addr);
        uint16_t fragIdx = addrAsHash[0];
        TRACE_ME("insert state entry: MP3 frag idx = %d ", fragIdx);

        std::vector<uint8_t> value;
        _p.first.asJsonBytes(value);
        m_frag_accounts[fragIdx].insert(addrAsHash, value);  // IH: here is a BUG ?
        // assert(m_frag_accounts[fragIdx].contains(addrAsHash));
        if (!m_frag_accounts[fragIdx].contains(addrAsHash))
            INTERNAL_EXCEPTION(fmt::format("Inserted state entry {} does not exist.", to_hex_string(addr).c_str()));

        m_frag_storages[fragIdx][addr] = _p.second;  // IH: TODO this could be omitted by some explicit bool flag indicating a change/not in storage has occured
    }

    /**
     * It dumps 'partial' global state of MP3 related to all addresses in addrs_to_process. It uses iteration trails of MP3 to build this partial state.
     * The results is stored into 'data'
     */
    void FragmentedGlobalState::dump_partial_db(std::set<Address>& addrs_to_process,
                                                std::vector<uint8_t>& db_data, std::set<h256>& db_keys,
                                                std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes,
                                                size_t& storages_sizes_size, std::vector<uint8_t>& acnts_storages)
    {
        TRACE_ME("Dumping PARTIAL DB of MP3...");
        size_t sum_size_data = 0;
        // unsigned i = 0;

#ifndef NDEBUG
        size_t size_data_before = db_data.size();
#endif

        // the standard case
        for (auto& addr : addrs_to_process) {
            assert(exists(addr));
            auto addrAsHash = h256(addr);
            uint16_t fragIdx = addrAsHash[0];

            // 1) dump root node of a particular MP3; TODO: DROP later since E will cache it
            h256 cur_root_hash = m_frag_accounts[fragIdx].root();
            TRACE_ME("\t\t cur_root_hash = %s", cur_root_hash.hex().c_str());
            if (EmptyTrie == cur_root_hash) {  //empty MP3
                auto r = dev::rlp("");
                RLP rlp_empty = RLP(r);
                h256 h = sha3(rlp_empty.data());
                if (db_keys.end() == db_keys.find(h)) {                                                  // check for duplicity
                    db_data.insert(db_data.end(), (fragIdx <= UINT8_MAX) ? (uint8_t)fragIdx : fragIdx);  // 1st B (or 2) is special and indicate fragIdx
                    db_data.insert(db_data.end(), rlp_empty.data().begin(), rlp_empty.data().end());     // insert only RLP of db entry (hash can be made later)
                    sum_size_data += rlp_empty.data().size() + (fragIdx <= UINT8_MAX) ? 1 : sizeof(fragIdx);
                    db_keys.insert(cur_root_hash);
                    TRACE_ME("\t\t EmptyTrie, root node = %s", RLP2MP3String(rlp_empty).c_str());
                }
            } else {
                // handle the initialized root, which is not included in the trail for some reason (maybe optimization... since it is the same for each node)
                auto root_value = db(fragIdx)->lookup(cur_root_hash);
                RLP root_rlp = RLP(root_value);
                TRACE_ME("root_rlp.len = %ld", root_rlp.itemCount());

                h256 h = sha3(root_rlp.data());
                if (db_keys.end() == db_keys.find(h)) {                                                  // check for duplicity
                    db_data.insert(db_data.end(), (fragIdx <= UINT8_MAX) ? (uint8_t)fragIdx : fragIdx);  // 1st B (or 2) is special and indicate fragIdx
                    db_data.insert(db_data.end(), root_rlp.data().begin(), root_rlp.data().end());       // insert only RLP of db entry (hash can be made later)
                    sum_size_data += root_rlp.data().size() + (fragIdx <= UINT8_MAX) ? 1 : sizeof(fragIdx);
                    db_keys.insert(cur_root_hash);
                }
                TRACE_ME("\t\t Initialized Root: %s", RLP2MP3String(root_rlp).c_str());
            }

            // 2) dump the remaining nodes of db in MP3 trail of each address
            std::vector<std::string> node_trail;                                          // RLPs of DB nodes
            node_trail.reserve(10);                                                       // should be enough
            if (0 != m_frag_accounts[fragIdx].buildTrail(addrAsHash.ref(), &node_trail))  // build trail of current account of MP3
                INTERNAL_EXCEPTION("\t\t Error when building trail.");

            TRACE_ME("\t\t size of trail = %ld ", node_trail.size());
            TRACE_ME("\t\t trail[0] = %s ", RLP2MP3String(RLP(node_trail[0])).c_str());

            // TODO: this should be optimized by batching, returing shared trail - pass all accounts to MP3 and let MP3 to fill only unique elements to trail

            // pass all nodes in the trail and store unique ones
            for (auto& str_node : node_trail) {
                RLP rlp_node = RLP(str_node);
                h256 h = sha3(rlp_node.data());
                TRACE_ME("\t\t node in trail: %s", RLP2MP3String(rlp_node).c_str());

                if (db_keys.end() == db_keys.find(h)) {                                                  // check for duplicity
                    db_data.insert(db_data.end(), (fragIdx <= UINT8_MAX) ? (uint8_t)fragIdx : fragIdx);  // 1st B (or 2) is special and indicate fragIdx
                    db_data.insert(db_data.end(), rlp_node.data().begin(), rlp_node.data().end());       // insert only RLP of db entry (hash can be made later)
                    sum_size_data += rlp_node.data().size() + (fragIdx <= UINT8_MAX) ? 1 : sizeof(fragIdx);
                    db_keys.insert(h);
                }
            }

            // 3) dump full storage of the sender or recepient account
            this->_dump_single_storage(addr, storages, storages_sizes, storages_sizes_size, fragIdx);
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
    void FragmentedGlobalState::dump_full_db_of_frag(std::vector<uint8_t>& mp3_keys,
                                                     std::vector<uint8_t>& mp3_values,
                                                     std::vector<size_t>& values_sizes,
                                                     size_t& mp3_keys_size, size_t& values_sizes_size,
                                                     std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size, const uint16_t fragIdx)
    {
        TRACE_ME("Dumping full DB of global state");
        values_sizes_size = 0, storages_sizes_size = 0;
        size_t summed_keys_size = 0;
        unsigned cnt_entries = 0;

        // int i = 0;
        for (auto const& e : m_frag_accounts[fragIdx]) {  // std::pair<bytesConstRef, bytesConstRef>
            auto addr = e.first;
            auto val = e.second;

            // std::cout << "\t account[" << i++ << "] addr = " << addr << "value = " << escaped(val.toString(), false) << "\n";
            mp3_keys.insert(mp3_keys.end(), addr.begin(), addr.end());    // insert the full content of value
            mp3_values.insert(mp3_values.end(), val.begin(), val.end());  // insert the full content of key
            values_sizes.push_back(val.size());

            summed_keys_size += addr.size;
            values_sizes_size += sizeof(size_t);

            // dump also storage of each account
            _dump_single_storage(addr, storages, storages_sizes, storages_sizes_size, fragIdx);
            cnt_entries++;
        }
        mp3_keys_size = cnt_entries * 32;
        assert(mp3_keys_size == summed_keys_size);
        // print_sep();
    }


    void FragmentedGlobalState::_dump_single_storage(Address addr, std::vector<uint8_t>& storages, std::vector<size_t>& storages_sizes, size_t& storages_sizes_size, const uint16_t fragIdx) const
    {
        TRACE_ME("dumping storage of addr = %s", address_to_hex_string(addr).c_str());
        auto const& cur_storage = m_frag_storages[fragIdx].at(addr);  // if 'addr' does not exists, just raise exception

        size_t cur_storage_size = cur_storage.toBytes(storages);  // updates 'storages' vector

        storages_sizes.push_back(cur_storage_size);
        storages_sizes_size += sizeof(size_t);  // account for the size variable
    }

    ////////////////////////////// Static Methods //////////////////////////////

    /**
     * Constructs partial FragmentedGlobalState object from parameters passed. (called from enclave)
     * Note that also integrity of copied storages is verified here, since they are passed to E as [user_check]
     */
    int FragmentedGlobalState::construct_partial_state(FragmentedGlobalState** gs, const uint8_t* gs_root_h,
                                                       const uint8_t* db_data, size_t db_data_size,
                                                       const uint8_t* db_data_aux, size_t db_data_aux_size,
                                                       const uint8_t* storages, const size_t* storages_sizes,
                                                       size_t storages_sizes_size, const uint8_t* accnts_of_storages)
    {
        TRACE_ME("Constructing partial state");

        assert(false);
        *gs = new FragmentedGlobalState(FragmentedGlobalState::DEFAULT_FRAGS_CNT, false);  // TODO IH: we need a construction of full GS, wrapping this !!!

        // auto addrAsHash = h256(addr);
        // uint16_t fragIdx = addrAsHash[0];

        auto& acnts = (*gs)->getAccounts(fragIdx);  // IMPORTANT: in this function is not valid to access addresses that were not created yet! Only accounts from db_data are valid.
        h256 root = h256(gs_root_h, h256::ConstructFromPointer);
        acnts.setRoot(root, Verification::Skip);  // set root of MP3 forcely (if it is different from the last known in E, then E exits)

        size_t sum_data_size = 0, sum_aux_size = 0;
        unsigned inserted_db_data_entries = 0;
        unsigned i = 0;

        // 1) insert all passed RLP DB entries
        TRACE_ME("Inserting db_data to DB");
        while (sum_data_size < db_data_size) {
            // TRACE_ME("[%d] Data account is:", i);

            // construct the full RLP of DB entry by parsing its length first
            RLP rlp_oneB = RLP(db_data + sum_data_size, 1, RLP::LaissezFaire);
            auto len_size = rlp_oneB.lengthSize();
            // TRACE_ME("len_size = %d ", len_size);

            RLP rlp_len = RLP(db_data + sum_data_size, 1 + len_size, RLP::LaissezFaire);
            size_t full_rlp_size = 1 + len_size + rlp_len.length();

            // construct key & value of DB entry
            RLP full_rlp = RLP(db_data + sum_data_size, full_rlp_size);
            h256 h = sha3(full_rlp.data());

            // TRACE_ME("\t %s", RLP2MP3String(full_rlp).c_str());

            // insert DB entry directly into DB without touching MP3 API
            (*gs)->db(fragIdx)->insert(h, (full_rlp.data()));
            sum_data_size += full_rlp_size;
            inserted_db_data_entries++;
            i++;
        }
        // TRACE_ME("inserted_db_data_entries = %d\n", inserted_db_data_entries);
        assert(sum_data_size == db_data_size);

        // 2) insert all passed auxiliary RLP DB entries
        TRACE_ME("Inserting aux data to DB");
        inserted_db_data_entries = 0, i = 0;
        while (sum_aux_size < db_data_aux_size) {
            TRACE_ME("[%d] AUX: node is:", i);
            // construct full RLP of DB entry by parsing its length first
            RLP rlp_oneB = RLP(db_data_aux + sum_aux_size, 1, RLP::LaissezFaire);
            TRACE_ME("1");
            auto len_size = rlp_oneB.lengthSize();
            TRACE_ME("len_size = %d ", len_size);
            RLP rlp_len = RLP(db_data_aux + sum_aux_size, 1 + len_size, RLP::LaissezFaire);
            TRACE_ME("2");
            size_t full_rlp_size = 1 + len_size + rlp_len.length();
            TRACE_ME("3");

            // construct key & value of DB entry
            RLP full_rlp = RLP(db_data_aux + sum_aux_size, full_rlp_size);
            TRACE_ME("4");
            h256 h = sha3(full_rlp.data());
            // TRACE_ME("\t %s", RLP2MP3String(full_rlp).c_str());

            // insert DB entry directly into DB without touching MP3 API
            (*gs)->db(fragIdx)->insert(h, (full_rlp.data()));
            sum_aux_size += full_rlp_size;
            inserted_db_data_entries++;
            i++;
        }
        // TRACE_ME("inserted_db_aux_entries = %d\n", inserted_db_data_entries);
        assert(sum_aux_size == db_data_aux_size);

        // 3) insert all passed storages
        TRACE_ME("Inserting storages");
        size_t ptr_storages = 0;  // indicates the current possition in storages
        size_t ptr_addrs = 0;     // indicates the current possition in accnts_of_storages
        for (unsigned i = 0; i < storages_sizes_size / sizeof(size_t); i++) {
            Address addr = from_big_endian(&(accnts_of_storages[ptr_addrs]));
            SimpleStorage* s = SimpleStorage::fromBytes(&storages[ptr_storages], storages_sizes[i]);
            // TRACE_ME("[%d] Imported storage is %s, with size %ld and hash = %s", i, s->toString().c_str(), storages_sizes[i], to_hex_string(s->hash()).c_str());

            // 3a) insert storage entry
            auto computed_hash = s->hash();
            uint16_t fragIdx = h256(addr)[0];
            auto& strgs = (*gs)->getStorages(fragIdx);
            strgs.insert(std::make_pair(std::move(addr), std::move(*s)));

            // 3b) verify integrity of storages
            auto fetched = (*gs)->get(addr).acc.get_stHash();  // this can be done only after storage entry was inserted !!!
            if (computed_hash != fetched) {                    // if accnt or storage does not exits => throw
                TRACE_ME("Mismatch of storage hash for addr = %s. Expected = %s, got %s",
                         address_to_hex_string(addr).c_str(),
                         to_hex_string(computed_hash).c_str(),
                         to_hex_string(fetched).c_str());
                return 1;
            }

            ptr_storages += storages_sizes[i];
            ptr_addrs += ADDR_SIZE_B;
        }

        // 4) verify whether DB entry with the claimed root value exists after filling DB
        if (0 == (*gs)->db(fragIdx)->lookup(root).size()) {
            TRACE_ME("Passed root %s does not exist in DB.", root.hex().c_str());
            return 2;
        }

        // TRACE_ME("Root of MP3 after importing DB is: %s", acnts.root().hex().c_str());
        // print_sep();
        return 0;
    }

    /**
     * Constructs full FragmentedGlobalState object from parameters passed. (called from enclave)
     */
    int FragmentedGlobalState::construct_full_state_of_frag(FragmentedGlobalState** gs,
                                                            const uint8_t* mp3_keys, size_t mp3_keys_size,
                                                            const uint8_t* mp3_values, const size_t* values_sizes, size_t mp3_values_sizes_size,
                                                            const uint8_t* storages, const size_t* storages_sizes, size_t storages_sizes_size, const uint16_t fragIdx)
    {
        TRACE_ME("Constructing full state");
        assert(mp3_keys_size / ADDR_SIZE_B == mp3_values_sizes_size / sizeof(size_t));


        assert(false);
        *gs = new FragmentedGlobalState(FragmentedGlobalState::DEFAULT_FRAGS_CNT);  // TODO IH: we need a construction of full GS, wrapping this !!!


        auto& acnts = (*gs)->getAccounts(fragIdx);
        auto& strgs = (*gs)->getStorages(fragIdx);

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

    // void to_json(nlohmann::json& j, const FragmentedGlobalState& s)
    // {
    //     j["block"] = s.currentBlock;
    //     auto o = nlohmann::json::array();

    //     // items of iterator for MP3 are std::pair<bytesConstRef, bytesConstRef> // maybe interepret second?
    //     for (const auto& p : s.m_frag_accounts[fragIdx]) {
    //         // o.push_back({to_hex_string(p.first), p.second});
    //         o.push_back({to_hex_string(p.first), to_hex_string(p.second.begin(), p.second.end())});
    //     }
    //     j["accounts"] = o;
    // }

    // void from_json(const nlohmann::json& j, FragmentedGlobalState& s)
    // {
    //     if (j.find("block") != j.end()) {
    //         s.currentBlock = j["block"];
    //     }

    //     for (const auto& it : j["accounts"].items()) {
    //         const auto& v = it.value();
    //         // a.m_frag_accounts[fragIdx].insert(make_pair(to_uint256(v[0]), v[1]));
    //         s.m_frag_accounts[fragIdx].insert(to_uint256(v[0]), bytesConstRef(v[1]));

    //         // TODO: persist storages later
    //         // s.m_frag_storages[fragIdx][to_uint256(v[0])] = v[1];
    //     }
    // }


}  // namespace eevm
