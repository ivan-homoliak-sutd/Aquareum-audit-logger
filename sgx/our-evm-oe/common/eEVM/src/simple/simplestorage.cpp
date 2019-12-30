// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/simple/simplestorage.h"

#include "eEVM/util.h"

#include <ostream>

namespace eevm
{
    SimpleStorage::SimpleStorage(const nlohmann::json& j)
    {
        for (auto it = j.cbegin(); it != j.cend(); it++)
            m_s.emplace(
                std::piecewise_construct,
                /* key */ std::forward_as_tuple(to_uint256(it.key())),
                /* value */ std::forward_as_tuple(to_uint256(it.value())));
    }

    // SimpleStorage::~SimpleStorage(){}

    void SimpleStorage::store(const uint256_t& key, const uint256_t& value)
    {
        m_s[key] = value;
    }

    uint256_t SimpleStorage::load(const uint256_t& key)
    {
        auto e = m_s.find(key);
        if (e == m_s.end())
            return 0;
        return e->second;
    }

    bool SimpleStorage::exists(const uint256_t& key)
    {
        return m_s.find(key) != m_s.end();
    }

    bool SimpleStorage::remove(const uint256_t& key)
    {
        auto e = m_s.find(key);
        if (e == m_s.end())
            return false;
        m_s.erase(e);
        return true;
    }

    size_t SimpleStorage::toBytes(std::vector<uint8_t>* toAppend) const
    {
        size_t size_of_storage = 0;

        for (auto& e : m_s) {
            // 1) store key
            uint8_t as_bytes[32];
            to_big_endian(e.first, as_bytes);
            for (size_t i = 0; i < 32; i++) {
                toAppend->push_back(as_bytes[i]);
            }

            // 1) store value
            to_big_endian(e.first, as_bytes);
            for (size_t i = 0; i < 32; i++) {
                toAppend->push_back(as_bytes[i]);
            }
            size_of_storage += 64;
        }
        return size_of_storage;
    }

    /////////////////////////////////
    // operators and static methods
    /////////////////////////////////

    static SimpleStorage* SimpleStorage::fromBytes(uint8_t* const data, size_t size)
    {
        // TODO

        auto s = new SimpleStorage();


        return s;
    }

    bool SimpleStorage::operator==(const SimpleStorage& that) const
    {
        return m_s == that.m_s;
    }

    void to_json(nlohmann::json& j, const SimpleStorage& s)
    {
        j = nlohmann::json::object();

        for (const auto& p : s.m_s) {
            j[to_hex_string(p.first)] = to_hex_string(p.second);
        }
    }

    void from_json(const nlohmann::json& j, SimpleStorage& s)
    {
        for (decltype(auto) it = j.cbegin(); it != j.cend(); it++) {
            s.m_s.emplace(to_uint256(it.key()), to_uint256(it.value()));
        }
    }

    inline std::ostream& operator<<(std::ostream& os, const SimpleStorage& s)
    {
        os << nlohmann::json(s).dump(2);
        return os;
    }


}  // namespace eevm
