// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "address.h"
#include "bigint.h"

#include <array>
#include <cassert>
#include <nlohmann/json.hpp>
#include <vector>

#define SIG_SIZE_PB_BYTES 65
#define ADDRESS_SIZE 32
// note that only 20B of 32 are used, but the full 32B are required due to internals of eevm

namespace eevm
{
    using Code = std::vector<uint8_t>;

    namespace log
    {
        using Data = std::vector<uint8_t>;
        using Topic = uint256_t;
    }  // namespace log

    struct LogEntry {
        Address address;
        log::Data data;
        std::vector<log::Topic> topics;

        bool operator==(const LogEntry& that) const;

        friend void to_json(nlohmann::json&, const LogEntry&);
        friend void from_json(const nlohmann::json&, LogEntry&);
    };

    struct LogHandler {
        virtual ~LogHandler() = default;
        virtual void handle(LogEntry&&) = 0;
    };

    struct NullLogHandler : public LogHandler {
        virtual void handle(LogEntry&&) override {}
    };

    void to_json(nlohmann::json&, const LogEntry&);
    void from_json(const nlohmann::json&, LogEntry&);
    std::string txlog_to_json_str(const LogHandler& lh);

    struct VectorLogHandler : public LogHandler {
        std::vector<LogEntry> logs;

        virtual ~VectorLogHandler() = default;
        virtual void handle(LogEntry&& e) override
        {
            logs.emplace_back(e);
        }
        friend void to_json(nlohmann::json&, const VectorLogHandler&);
    };


    /**
   * Represents data of an Ethereum transaction that needs to be persisted to
   * blockchain/ledger
   *
   */
    struct PersistantTransaction {
        Address origin;  // sender of the TX (IH: later can be removed to save space - we have ECC PK recovery anyway)
        Address to;      // the recepient of the TX
        uint64_t nonce;  // the number of TXs send by the sender of this TX (i.e., protection against replay attacks)
        uint64_t value;  // call_value
        uint64_t gas_price;
        uint64_t gas_limit;
        uint8_t signature[SIG_SIZE_PB_BYTES];  // computed over: origin, to, value, code, gas_price, gas_limit, nonce
        Code code;

        PersistantTransaction(
            Address origin,
            Address to,
            uint64_t nonce,
            uint64_t value = 0,
            Code code = {},
            uint64_t gas_price = 0,
            uint64_t gas_limit = 0,
            uint8_t* signature = NULL)
          : origin(origin),
            to(to),
            nonce(nonce),
            value(value),
            gas_price(gas_price),
            gas_limit(gas_limit),
            code(code)
        {
            if (signature) {
                memcpy(this->signature, signature, SIG_SIZE_PB_BYTES);
            }
        }

        std::vector<uint8_t> asDataForHash()
        {
            assert(sizeof(Address) == ADDRESS_SIZE);

            std::vector<uint8_t> ret = std::vector<uint8_t>(2 * sizeof(Address) + sizeof(uint64_t) * 4 + code.size());

            // construct data object in the order: origin, to, value, gas_price, gas_limit, nonce, code
            uint8_t addr[ADDRESS_SIZE];

            intx::be::unsafe::store((uint8_t*)&addr, this->origin);  // convert Address to vector of Bytes ((intx::uint<256>))
            memcpy(ret.data(), addr, ADDRESS_SIZE);

            intx::be::unsafe::store((uint8_t*)&addr, this->to);  // convert Address to vector of Bytes ((intx::uint<256>))
            memcpy(ret.data() + sizeof(Address), addr, ADDRESS_SIZE);

            memcpy(ret.data() + 2 * sizeof(Address), &(this->value), sizeof(uint64_t));
            memcpy(ret.data() + 2 * sizeof(Address) + sizeof(uint64_t), &(this->gas_price), sizeof(uint64_t));
            memcpy(ret.data() + 2 * sizeof(Address) + 2 * sizeof(uint64_t), &(this->gas_limit), sizeof(uint64_t));
            memcpy(ret.data() + 2 * sizeof(Address) + 3 * sizeof(uint64_t), &(this->nonce), sizeof(uint64_t));
            memcpy(ret.data() + 2 * sizeof(Address) + 4 * sizeof(uint64_t), this->code.data(), this->code.size());

            return ret;  // hopes in as-if 'return value optimization'
        };
    };

    /**
   * Ethereum transaction wrapped for need of eEVM
   */
    struct Transaction : PersistantTransaction {
        LogHandler& log_handler;
        std::vector<Address> destroy_list;

        Transaction(
            Address origin,
            Address to,
            LogHandler& lh,
            Code code = {},
            uint64_t value = 0,
            uint64_t nonce = 0,
            uint64_t gas_price = 0,
            uint64_t gas_limit = 0,
            uint8_t* signature = NULL)
          : PersistantTransaction(origin, to, nonce, value, code, gas_price, gas_limit, signature),
            log_handler(lh)
        {}

        // constructor with pointers to Address fields
        Transaction(
            Address* origin,
            Address* to,
            LogHandler& lh,
            Code code = {},
            uint64_t value = 0,
            uint64_t nonce = 0,
            uint64_t gas_price = 0,
            uint64_t gas_limit = 0,
            uint8_t* signature = NULL)
          : PersistantTransaction(*origin, *to, nonce, value, code, gas_price, gas_limit, signature),
            log_handler(lh)
        {}
    };
}  // namespace eevm
