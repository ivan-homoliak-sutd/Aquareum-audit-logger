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

        SimpleStorage()
          : m_s()
        {}

        SimpleStorage(const nlohmann::json& j);
        SimpleStorage(const SimpleStorage& other)  // IH: copy ctor
          : m_s(other.m_s)
        {}
        SimpleStorage(SimpleStorage&& other)  // IH: movable ctor
          : m_s(std::move(other.m_s))
        {}

        void store(const uint256_t& key, const uint256_t& value) override;
        uint256_t load(const uint256_t& key) override;
        bool exists(const uint256_t& key);
        bool remove(const uint256_t& key) override;

        inline std::map<uint256_t, uint256_t>& data() { return m_s; }

        bool operator==(const SimpleStorage& that) const;
        SimpleStorage& operator=(const SimpleStorage&) = default;


        // serialization
        size_t toBytes(std::vector<uint8_t>& toAppend) const;
        size_t toBytes(uint8_t * toAppend) const;
        static SimpleStorage* fromBytes(const uint8_t* data, size_t size);

        uint256_t hash() override;
        static uint256_t hashOfEmptyStorage();
        inline size_t sizeB() const { return m_s.size() * 2 * sizeof(uint256_t); }

        std::string toString() const override;
        void obj_to_json(nlohmann::json& j) const;

        friend void to_json(nlohmann::json&, const SimpleStorage&);
        friend void from_json(const nlohmann::json&, SimpleStorage&);
    };

    void to_json(nlohmann::json& j, const SimpleStorage& s);
    void from_json(const nlohmann::json& j, SimpleStorage& s);
}  // namespace eevm
