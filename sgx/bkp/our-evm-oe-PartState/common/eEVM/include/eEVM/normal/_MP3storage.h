// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "eEVM/storage.h"

#include "aleth-mp3/Common.h"
#include "aleth-mp3/database/SecureTrieDB.h"

#include <map>
#include <nlohmann/json.hpp>

using namespace dev;

namespace eevm
{
/**
   * MP3-backed implementation of per account storage
   *
   */
class MP3Storage : public Storage {
    SecureTrieDB<h256, db::OverlayDB> m_storage;  //(const_cast<OverlayDB*>(&m_db), root);

public:
    MP3Storage()
      : m_storage(new db::MemoryDB());
    {
        m_storage.init();  // create some tmp node
    };

    // MP3Storage(const nlohmann::json& j);

    void store(const h256& key, const uint256_t& value) override;

    // uint256_t load(const uint256_t& key) override;

    bytesConstRef load(const h256& key) override;
    bool exists(const h256& key);
    bool remove(const h256& key) override;

    // bool operator==(const MP3Storage& that) const;

    friend void to_json(nlohmann::json&, const MP3Storage&);
    friend void from_json(const nlohmann::json&, MP3Storage&);
};

void to_json(nlohmann::json& j, const MP3Storage& s);
void from_json(const nlohmann::json& j, MP3Storage& s);
}  // namespace eevm
