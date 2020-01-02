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
    public:
        std::map<uint256_t, uint256_t> m_s;

        SimpleStorage() = default;
        SimpleStorage(const nlohmann::json& j);
        // SimpleStorage(SimpleStorage& other)
        // {
        //     this->m_s = other.data();
        // };

        void store(const uint256_t& key, const uint256_t& value) override;
        uint256_t load(const uint256_t& key) override;
        bool exists(const uint256_t& key);
        bool remove(const uint256_t& key) override;

        inline std::map<uint256_t, uint256_t>& data() { return m_s; }

        bool operator==(const SimpleStorage& that) const;

        // serialization
        size_t toBytes(std::vector<uint8_t>& toAppend) const;
        static SimpleStorage* fromBytes(const uint8_t* data, size_t size);
        uint256_t hash() const;

        friend void to_json(nlohmann::json&, const SimpleStorage&);
        friend void from_json(const nlohmann::json&, SimpleStorage&);
    };

    void to_json(nlohmann::json& j, const SimpleStorage& s);
    void from_json(const nlohmann::json& j, SimpleStorage& s);
}  // namespace eevm
