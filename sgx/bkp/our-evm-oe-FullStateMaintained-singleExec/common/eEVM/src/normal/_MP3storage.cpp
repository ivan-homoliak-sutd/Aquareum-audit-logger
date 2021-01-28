// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/normal/_MP3storage.h"

#include "eEVM/util.h"

#include <ostream>

namespace eevm
{
// MP3storage::MP3storage(const nlohmann::json& j) {
//     for (auto it = j.cbegin(); it != j.cend(); it++)
//         m_storage.emplace(std::piecewise_construct,
//             /* key */ std::forward_as_tuple(to_uint256(it.key())),
//             /* value */ std::forward_as_tuple(to_uint256(it.value())));
// }

// MP3storage::~MP3storage(){}

void MP3storage::store(const h256& key, uint256_t data) {
    m_storage.insert(
        key.ref(),


    );
}

bytesConstRef MP3storage::load(const h256& key) {
    auto e = m_storage.at(key);
    return bytesConstRef(e);
}

bool MP3storage::exists(const h256& key) {
    return m_storage.contains(key);
}

bool MP3storage::remove(const h256& key) {
    if (!m_storage.contains(key))
        return false

            m_storage.remove(key);
    return true;
}

// bool MP3storage::operator==(const MP3storage& that) const {
//     return m_storage == that.m_storage;
// }

void to_json(nlohmann::json& j, const MP3storage& s) {
    j = nlohmann::json::object();

    for (const auto& p : s.m_storage) {
        j[to_hex_string(p.first)] = to_hex_string(p.second);
    }
}

void from_json(const nlohmann::json& j, MP3storage& s) {
    for (decltype(auto) it = j.cbegin(); it != j.cend(); it++) {
        s.m_storage.emplace(to_uint256(it.key()), to_uint256(it.value()));
    }
}

inline std::ostream& operator<<(std::ostream& os, const MP3storage& s) {
    os << nlohmann::json(s).dump(2);
    return os;
}
}  // namespace eevm
