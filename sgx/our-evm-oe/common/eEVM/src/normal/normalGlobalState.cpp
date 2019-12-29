// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/simple/simpleglobalstate.h"

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

    SimpleAccountState NormalGlobalState::get(const Address& addr)
    {
        // std::cout << "NormalGlobalState::get addr = " << to_hex_string(addr) << "\n";
        std::string acnt_json = m_accounts.at(h256(addr));
        // std::cout << "NormalGlobalState::get  acnt_json = " << acnt_json << "\n";

        if (acnt_json.empty()) {  // create a new account if it does not exist
            return create(addr, 0, {});
        }

        // populate account object
        auto j = nlohmann::json::parse(acnt_json);
        SimpleAccount a;
        from_json(j, a);
        assert(a.get_address() == addr);

        // fetch account's data from storage map
        SimpleStorage s = m_storages.at(addr);  // TODO: resolve non-existing
        return SimpleAccountState(a, s);        // IH: hope c++ RVO works here
    }

    SimpleAccountState NormalGlobalState::create(const Address& addr, const uint256_t& balance, const Code& code)
    {
        insert({SimpleAccount(addr, balance, code), {}});
        return get(addr);
    }

    bool NormalGlobalState::exists(const Address& addr) { return m_accounts.contains(addr); }
    size_t NormalGlobalState::num_accounts() { return (dynamic_cast<db::MemoryDB*>(m_accounts.db()->db().get()))->size(); }
    const Block& NormalGlobalState::get_current_block() { return currentBlock; }
    uint256_t NormalGlobalState::get_block_hash(uint8_t offset) { return 0u; /* IH: cool */ }
    void NormalGlobalState::insert(const StateEntry& p)
    {
        auto addr = p.first.get_address();
        // std::cout << "NormalGlobalState::insert: account with addr: " << to_hex_string(addr) << "\n";

        auto _p = const_cast<StateEntry&>(p);
        m_accounts.insert(h256(addr), _p.first.asJsonBytesRef());
        if (m_storages.end() != m_storages.find(addr))
            throw std::logic_error(fmt::format("NormalGlobalState::insert - storage for address '{}' already exists.", to_hex_string(addr)));

        m_storages[addr] = p.second;
    }

    void NormalGlobalState::dump_full_db(std::string* db_keys,
                                         std::string* db_values,
                                         std::vector<size_t>* values_sizes,
                                         size_t& db_keys_size, size_t& values_sizes_size,
                                         std::vector<uint8_t>* storages, std::vector<size_t>* storages_sizes, size_t& storages_sizes_size)
    {
        // 1) we need to flush state cache of OverlayDB to persistant database (e.g., MemoryDB), coz later we will work with the persistant one
        this->commitAccntDB();
        db::MemoryDB* mem_db = this->db();

        values_sizes_size = 0, storages_sizes_size = 0;
        size_t summed_keys_size = 0;
        unsigned cnt_entries = 0;

        db_keys = new std::string();
        db_values = new std::string();
        values_sizes = new std::vector<size_t>();
        storages = new std::vector<uint8_t>();
        storages_sizes = new std::vector<size_t>();

        int i = 0;
        for (auto const& e : mem_db->data()) {
            RLP rlp(e.second);

            std::cout << i++ << " size of str = " << e.first.size() << "\n";
            std::cout << " item = " << h256(e.first) << "\n";

            // skip non-leaf nodes (extension nodes)
            if (!(rlp.isList() && isLeaf(rlp))) {
                std::cout << "skipping extension/branch node: " << rlp.toString() << "\n";
                continue;
            }
            std::cout << fmt::format("\t dump_full_db: appending DB entry {} => {} \n", to_hex_string(h256(e.first)), e.second);

            db_keys->append(e.first);
            db_values->append(e.second);
            values_sizes->push_back(e.second.size());

            summed_keys_size += e.first.size();
            values_sizes_size += e.second.size();

            // dump also storage of each account
            _dump_single_storage((h256(e.first)), storages, storages_sizes, storages_sizes_size);
            cnt_entries++;
        }
        db_keys_size = cnt_entries * 32;
        std::cerr << fmt::format("dump_full_db: db_keys_size = {} | summed_keys_size = {} \n", db_keys_size, summed_keys_size);
        assert(db_keys_size == summed_keys_size);
    }


    void NormalGlobalState::_dump_single_storage(Address addr, std::vector<uint8_t>* storages, std::vector<size_t>* storages_sizes, size_t& storages_sizes_size) const
    {
        std::cout << "\t dumping storage of addr = " << addr << "\n";
        auto const& cur_storage = m_storages.at(addr);  // if 'addr' does not exists, just raise exception

        size_t cur_storage_size = cur_storage.toBytes(storages);  // updates 'storages' vector

        storages_sizes->push_back(cur_storage_size);
        storages_sizes_size += sizeof(size_t);  // account for the size variable
    }

    ////////////////////////////// Static Methods //////////////////////////////

    /**
     * Constructs  NormalGlobalState object from parameters passed. (called from enclave)
     */
    // static int construct_full_state(NormalGlobalState* gs, const uint8_t* db_keys, size_t db_keys_size,
    //                                 const uint8_t* db_values, const size_t* values_sizes, size_t db_values_sizes_size,
    //                                 uint8_t* const storages, const size_t* storages_sizes, size_t storages_sizes_size)
    // {
    //     assert(db_keys_size / 32 == db_values_sizes_size / sizeof(size_t));

    //     // gs = new NormalGlobalState();

    //     // // 1) insert account states one by one to global MP3
    //     // for (size_t i = 0; i < db_values_sizes_size / sizeof(size_t); i++) {
    //     //     values_sizes[i];
    //     // }

    //     // gs->m_accounts.insert();
    //     return 0;
    // }

    // void to_json(nlohmann::json& j, const NormalGlobalState& s) {
    //     j["block"] = s.currentBlock;
    //     auto o = nlohmann::json::array();

    //     // items of iterator for MP3 are std::pair<bytesConstRef, bytesConstRef> // maybe interepret second?
    //     for (const auto& p : s.m_accounts) {
    //         // o.push_back({to_hex_string(p.first), p.second});
    //         o.push_back({to_hex_string(p.first), to_hex_string(p.second.begin(), p.second.end())});
    //     }
    //     j["accounts"] = o;
    // }

    // void from_json(const nlohmann::json& j, NormalGlobalState& s) {
    //     if (j.find("block") != j.end()) {
    //         s.currentBlock = j["block"];
    //     }

    //     for (const auto& it : j["accounts"].items()) {
    //         const auto& v = it.value();
    //         // a.m_accounts.insert(make_pair(to_uint256(v[0]), v[1]));
    //         s.m_accounts.insert(to_uint256(v[0]),  bytesConstRef(v[1]));

    //         // TODO: persist storages later
    //         // s.m_storages[to_uint256(v[0])] = v[1];
    //     }
    // }

}  // namespace eevm
