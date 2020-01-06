// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/simple/simplestorage.h"
#include "eEVM/tracing.h"

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
        TRACE_ME("Storing [%s] <= %s", to_hex_string(key).c_str(), to_hex_string(value).c_str());
        m_s[key] = value;
        TRACE_ME("done");
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

    uint256_t SimpleStorage::hash()
    {
        std::vector<uint8_t> asBytes = {0u};  // empty storage will also have some hash associated
        this->toBytes(asBytes);

        auto h = keccak_256(asBytes);
        return from_big_endian(h.data());
    }

    size_t SimpleStorage::toBytes(std::vector<uint8_t>& toAppend) const
    {
        size_t size_of_storage = 0;

        for (auto& e : m_s) {
            // 1) store key
            uint8_t as_bytes[32];
            to_big_endian(e.first, as_bytes);
            for (size_t i = 0; i < 32; i++) {
                toAppend.push_back(as_bytes[i]);
            }

            // 1) store value
            to_big_endian(e.first, as_bytes);
            for (size_t i = 0; i < 32; i++) {
                toAppend.push_back(as_bytes[i]);
            }
            size_of_storage += 64;  // 64 accounts for uint256 key and value
        }
        return size_of_storage;
    }

    void SimpleStorage::to_json(nlohmann::json& j) const
    {
        for (const auto& p : m_s) {
            j[to_hex_string(p.first)] = to_hex_string(p.second);
        }
    }

    std::string SimpleStorage::toString() const
    {
        nlohmann::json j;
        to_json(j);
        return std::string(std::move(j.dump()));
    }

    /////////////////////////////////
    // operators and static methods
    /////////////////////////////////

    SimpleStorage* SimpleStorage::fromBytes(const uint8_t* data, size_t size)
    {
        auto s = new SimpleStorage();

        for (unsigned i = 0; i < size / 64; i++) {
            auto key = intx::be::unsafe::load<uint256_t>(&data[i * 64]);
            auto value = intx::be::unsafe::load<uint256_t>(&data[i * 64 + 32]);
            s->store(key, value);
        }
        return s;
    }

    bool SimpleStorage::operator==(const SimpleStorage& that) const
    {
        return m_s == that.m_s;
    }

    void to_json(nlohmann::json& j, const SimpleStorage& s)  // IH: TODO: drop this later since it is not const and instance-based
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

    uint256_t SimpleStorage::hashOfEmptyStorage()
    {
        std::vector<uint8_t> asBytes = {10u};  // empty storage will also have some hash associated
        auto h = keccak_256(asBytes);
        return from_big_endian(h.data());
    }

}  // namespace eevm
