// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "eEVM/storage.h"

#include <map>
#include <nlohmann/json.hpp>

namespace eevm
{
    /**
   * Simple std::map-backed implementation of Storage
   *
   * IH: it is a per account storage that support only uint256_t to uint256_t mapping
   */
    class SimpleStorage : public Storage {
        std::map<uint256_t, uint256_t> m_s;

    public:
        SimpleStorage() = default;
        SimpleStorage(const nlohmann::json& j);

        void store(const uint256_t& key, const uint256_t& value) override;
        uint256_t load(const uint256_t& key) override;
        bool exists(const uint256_t& key);
        bool remove(const uint256_t& key) override;

        size_t toBytes(std::vector<uint8_t>* toAppend) const;

        bool operator==(const SimpleStorage& that) const;

        static SimpleStorage* SimpleStorage::fromBytes(uint8_t* const data, size_t size);

        friend void to_json(nlohmann::json&, const SimpleStorage&);
        friend void from_json(const nlohmann::json&, SimpleStorage&);
    };

    void to_json(nlohmann::json& j, const SimpleStorage& s);
    void from_json(const nlohmann::json& j, SimpleStorage& s);
}  // namespace eevm
