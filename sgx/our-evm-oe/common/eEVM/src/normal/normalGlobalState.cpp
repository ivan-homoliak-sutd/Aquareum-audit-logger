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

    // It creates a new account state if it does not exist!
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

    // // It iterates through low level persistant database
    // void NormalGlobalState::dump_full_db(std::string* db_keys,
    //                                      std::string* db_values,
    //                                      std::vector<size_t>* values_sizes,
    //                                      size_t& db_keys_size, size_t& values_sizes_size,
    //                                      std::vector<uint8_t>* storages, std::vector<size_t>* storages_sizes, size_t& storages_sizes_size)
    // {
    //     this->commitPersDB();
    //     auto db = this->persDB();
    //     // OverlayDB* db = this->db();


    //     values_sizes_size = 0, storages_sizes_size = 0;
    //     size_t summed_keys_size = 0;
    //     unsigned cnt_entries = 0;

    //     db_keys = new std::string();
    //     db_values = new std::string();
    //     values_sizes = new std::vector<size_t>();
    //     storages = new std::vector<uint8_t>();
    //     storages_sizes = new std::vector<size_t>();

    //     int i = 0;
    //     for (auto const& e : db->data()) {
    //         RLP rlp(e.second);
    //         auto DBkey = h256(e.first, h256::FromBinary);

    //         std::cout << "dump_full_db [" << i++ << "] size of key in DB = " << e.first.size() << "\n";
    //         std::cout << "key in DB = " << DBkey.hex() << "\n";
    //         std::cout << "value size = " << rlp.itemCount() << "\n";


    //         std::cout << ".\n";
    //         // skip non-leaf nodes (extension & branch nodes)
    //         if (!isLeaf(rlp)) {
    //             std::cout << "skipping extension/branch node: " << rlp.toString() << "\n";
    //             continue;
    //         }
    //         std::cout << "dump_full_db: appending Trie entry:\n\t";
    //         unsigned j = 0;
    //         for (auto r : rlp) {
    //             if (0 == j++ && 2 == rlp.itemCount()) {
    //                 std::cout << "'" << keyOf(r.payload()) << "' | ";
    //             } else {
    //                 std::cout << r.toString() << " | ";
    //             }
    //         }
    //         std::cout << "\n";

    //         db_keys->append(e.first);     // note that string contain binary data and need to be converted by dev::asBytes() to bytes
    //         db_values->append(e.second);  // full RLP data (2 or 17 items)
    //         values_sizes->push_back(e.second.size());

    //         summed_keys_size += e.first.size();
    //         values_sizes_size += e.second.size();


    //         auto addr = h256(rlp[0].payload());

    //         // dump also storage of each account
    //         _dump_single_storage(addr, storages, storages_sizes, storages_sizes_size);
    //         std::cout << ".done\n";
    //         cnt_entries++;
    //     }
    //     db_keys_size = cnt_entries * 32;
    //     std::cerr << fmt::format("dump_full_db: db_keys_size = {} | summed_keys_size = {} \n", db_keys_size, summed_keys_size);
    //     assert(db_keys_size == summed_keys_size);
    // }  // namespace eevm

    // It iterates MP3 entries through MP3's iterator (thus only leaf nodes are considered)
    void NormalGlobalState::dump_full_db(std::vector<uint8_t>& db_keys,
                                         std::vector<uint8_t>& db_values,
                                         std::vector<size_t>& values_sizes,
                                         size_t& db_keys_size, size_t& values_sizes_size,
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
            db_keys.insert(db_keys.end(), addr.begin(), addr.end());    // insert the full content of value
            db_values.insert(db_values.end(), val.begin(), val.end());  // insert the full content of key
            values_sizes.push_back(val.size());

            summed_keys_size += addr.size;
            values_sizes_size += sizeof(size_t);

            // dump also storage of each account
            _dump_single_storage(addr, storages, storages_sizes, storages_sizes_size);
            cnt_entries++;
        }
        db_keys_size = cnt_entries * 32;
        assert(db_keys_size == summed_keys_size);
        // print_sep();
    }  // namespace eevm


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
     * Constructs  NormalGlobalState object from parameters passed. (called from enclave)
     */
    int NormalGlobalState::construct_full_state(NormalGlobalState** gs,
                                                const uint8_t* db_keys, size_t db_keys_size,
                                                const uint8_t* db_values, const size_t* values_sizes, size_t db_values_sizes_size,
                                                const uint8_t* storages, const size_t* storages_sizes, size_t storages_sizes_size)
    {
        TRACE_ME("Constructing full state");
        assert(db_keys_size / ADDR_SIZE_B == db_values_sizes_size / sizeof(size_t));

        *gs = new NormalGlobalState();
        auto& acnts = (*gs)->getAccounts();
        auto& strgs = (*gs)->getStorages();

        size_t ptr_db_values = 0;  // indicates the current possition in db_values
        size_t ptr_storages = 0;   // indicates the current possition in storages

        // 1) insert account states one by one to global MP3
        for (size_t i = 0; i < db_values_sizes_size / sizeof(size_t); i++) {
            // TRACE_ME("[%ld]", i);
            auto key = h256(&(db_keys[i * ADDR_SIZE_B]), h256::ConstructFromPointer);  // ctor of h256 allocates memory
            auto val = new uint8_t[values_sizes[i]];                                   // manually allocating enclave memory since 'db_values' is in host memory
            memcpy(val, &db_values[ptr_db_values], values_sizes[i]);
            auto val_ref = bytesConstRef(val, values_sizes[i]);

            // std::cerr << " inserting entry: " << key << " => " << escaped(val_ref.toString(), false) << "\n";
            acnts.insert(key, val_ref);
            ptr_db_values += values_sizes[i];

            // 2) insert storage of the current account state
            SimpleStorage* s = SimpleStorage::fromBytes(&storages[ptr_storages], storages_sizes[i]);
            strgs[key] = std::move(*s);

            ptr_storages += storages_sizes[i];
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
