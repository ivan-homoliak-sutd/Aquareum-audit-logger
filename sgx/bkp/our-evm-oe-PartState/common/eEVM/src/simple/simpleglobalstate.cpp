// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/simple/simpleglobalstate.h"

namespace eevm
{
    SimpleGlobalState::SimpleGlobalState() {}

    void SimpleGlobalState::remove(const Address& addr)
    {
        accounts.erase(addr);
    }

    SimpleAccountState SimpleGlobalState::get(const Address& addr)
    {
        const auto acc = accounts.find(addr);
        if (acc != accounts.cend())
            return SimpleAccountState(std::move(acc->second.first), acc->second.second);

        return create(addr, 0, {});
    }

    SimpleAccountState SimpleGlobalState::create(const Address& addr, const uint256_t& balance, const Code& code)
    {
        insert(std::make_pair(SimpleAccount(addr, balance, code), SimpleStorage()));

        return get(addr);
    }

    SimpleAccountState SimpleGlobalState::update(const Address& addr, const StateEntry& p)
    {
        const auto acc = accounts.find(addr);
        if (acc != accounts.cend())
            throw std::logic_error("Requested account does not exist.");

        accounts[addr] = p;
        return get(addr);
    };


    void SimpleGlobalState::insert(const StateEntry& p)
    {
#ifdef NDEBUG
        accounts.insert(std::make_pair(p.first.get_address(), p));
#else
        const auto ib = accounts.insert(std::make_pair(p.first.get_address(), p));
        assert(ib.second);
#endif
    }

    bool SimpleGlobalState::exists(const Address& addr)
    {
        return accounts.find(addr) != accounts.end();
    }

    size_t SimpleGlobalState::num_accounts()
    {
        return accounts.size();
    }

    const Block& SimpleGlobalState::get_current_block()
    {
        return currentBlock;
    }

    uint256_t SimpleGlobalState::get_block_hash(uint8_t offset)
    {
        return 0u;  // IH: cool
    }

    bool operator==(const SimpleGlobalState& l, const SimpleGlobalState& r)
    {
        return (l.accounts == r.accounts) && (l.currentBlock == r.currentBlock);
    }

    void to_json(nlohmann::json& j, const SimpleGlobalState& s)
    {
        j["block"] = s.currentBlock;
        auto o = nlohmann::json::array();
        for (const auto& p : s.accounts) {
            o.push_back({to_hex_string(p.first), p.second});
        }
        j["accounts"] = o;
    }

    void from_json(const nlohmann::json& j, SimpleGlobalState& a)
    {
        if (j.find("block") != j.end()) {
            a.currentBlock = j["block"];
        }

        for (const auto& it : j["accounts"].items()) {
            const auto& v = it.value();
            a.accounts.insert(make_pair(to_uint256(v[0]), v[1]));
        }
    }
}  // namespace eevm
