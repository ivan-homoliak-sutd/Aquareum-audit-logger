// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/bigint.h"

using namespace dev;

namespace eevm
{
// NormalGlobalState::NormalGlobalState() {}

void NormalGlobalState::remove(const Address& addr) {
    m_accounts.remove(h256(addr));
}

AccountState NormalGlobalState::get(const Address& addr) {

    std::string json_data = m_accounts.at(h256(addr));

    if(json_data.empty()){ // create a new account if it does not exist
        return create(addr, 0, {});
    }

    // populate account object
    SimpleAccount a;
    from_json(json_data, a);
    assert(a.get_address() == addr);

    // fetch account's data from storage map
    SimpleStorage s = m_storages.at(addr); // TODO: resolve non-existing
    return AccountState(a, s);
}

AccountState NormalGlobalState::create(const Address& addr, const uint256_t& balance, const Code& code) {
    insert({SimpleAccount(addr, balance, code), {}});

    return get(addr);
}

bool NormalGlobalState::exists(const Address& addr) {
    return m_accounts.contains(addr);
}

size_t NormalGlobalState::num_accounts() {
    return (dynamic_cast<db::MemoryDB *>(m_accounts.db()->db().get()))->size();
}

const Block& NormalGlobalState::get_current_block() {
    return currentBlock;
}

uint256_t NormalGlobalState::get_block_hash(uint8_t offset) {
    return 0u;  // IH: cool
}

void NormalGlobalState::insert(const StateEntry& p) {
    m_accounts.insert(p.first.get_address(),  p.first.asJsonBytesRef());
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
