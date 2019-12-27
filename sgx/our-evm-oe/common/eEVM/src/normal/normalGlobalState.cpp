// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/bigint.h"

#include "aleth-mp3/FixedHash.h"

#include <fmt/format_header_only.h>


using namespace dev;

namespace eevm
{
    void NormalGlobalState::remove(const Address& addr)
    {
        m_accounts.remove(h256(addr));
    }

    AccountState NormalGlobalState::get(const Address& addr)
    {
        std::string json_data = m_accounts.at(h256(addr));

        if (json_data.empty()) {  // create a new account if it does not exist
            return create(addr, 0, {});
        }

        // populate account object
        SimpleAccount a;
        from_json(json_data, a);
        assert(a.get_address() == addr);

        // fetch account's data from storage map
        SimpleStorage s = m_storages.at(addr);  // TODO: resolve non-existing
        return AccountState(a, s);
    }

    AccountState NormalGlobalState::create(const Address& addr, const uint256_t& balance, const Code& code)
    {
        insert({SimpleAccount(addr, balance, code), {}});
        return get(addr);
    }

    bool NormalGlobalState::exists(const Address& addr) { return m_accounts.contains(addr); }
    size_t NormalGlobalState::num_accounts() { return (dynamic_cast<db::MemoryDB*>(m_accounts.db()->db().get()))->size(); }
    const Block& NormalGlobalState::get_current_block() { return currentBlock; }
    uint256_t NormalGlobalState::get_block_hash(uint8_t offset) { return 0u; /* IH: cool */ }
    void NormalGlobalState::insert(const StateEntry& p) { m_accounts.insert(p.first.get_address(), p.first.asJsonBytesRef()); }

    void NormalGlobalState::dump_full_db(std::vector<std::string>* db_keys,
                                         std::vector<std::string>* db_values,
                                         std::vector<size_t>* values_sizes,
                                         size_t& db_keys_size, size_t& values_sizes_size,
                                         std::vector<uint8_t>* storages, std::vector<size_t>* storages_sizes, size_t& storages_sizes_size)
    {
        db::MemoryDB* mem_db = this->db();

        values_sizes_size = 0, storages_sizes_size = 0;
        size_t summed_keys_size = 0;

        db_keys = new std::vector<std::string>();
        db_values = new std::vector<std::string>();
        values_sizes = new std::vector<size_t>();
        storages = new std::vector<uint8_t>();
        storages_sizes = new std::vector<size_t>();

        for (auto const& e : mem_db->data()) {
            db_keys->push_back(e.first);
            db_values->push_back(e.second);
            values_sizes->push_back(e.second.size());

            summed_keys_size += e.first.size();
            values_sizes_size += e.second.size();

            // dump also storages of accounts
            _dump_single_storage((h256(e.first)), storages, storages_sizes, storages_sizes_size);
        }
        db_keys_size = mem_db->size() * 32;
        std::cerr << fmt::format("dump_full_db: db_keys_size = {} | summed_keys_size = {} \n", db_keys_size, summed_keys_size);
        assert(db_keys_size == summed_keys_size);
    }

    void NormalGlobalState::_dump_single_storage(Address addr, std::vector<uint8_t>* storages, std::vector<size_t>* storages_sizes, size_t& storages_sizes_size) const
    {
        auto const& cur_storage = m_storages.at(addr);  // if 'addr' does not exists, just raise exception

        size_t cur_storage_size = cur_storage.toBytes(storages);  // updates 'storages' vector

        storages_sizes->push_back(cur_storage_size);
        storages_sizes_size += sizeof(size_t);  // account for the size variable
    }


    ////////////////////////////// Static Methods //////////////////////////////

    /**
     * Constructs  NormalGlobalState object from parameters passed. (called from enclave)
     */
    static int construct_full_state(NormalGlobalState* out_gs, const uint8_t* db_keys, size_t db_keys_size,
                                    const uint8_t* db_values, const size_t* values_sizes, size_t db_values_sizes_size,
                                    uint8_t* const storages, const size_t* storages_sizes, size_t storages_sizes_size)
    {
        out_gs = new NormalGlobalState();

        // insert account states one by one to global MP3
        for (size_t i = 0; i < count; i++) {
            /* code */
        }


        out_gs->m_accounts.insert();
    }

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
