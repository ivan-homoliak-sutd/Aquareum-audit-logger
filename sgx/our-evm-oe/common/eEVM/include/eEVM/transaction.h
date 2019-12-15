// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "address.h"
#include "bigint.h"

#include <array>
#include <nlohmann/json.hpp>
#include <vector>

#define SIG_SIZE_PB_BYTES 64

namespace eevm {
    using Code = std::vector<uint8_t>;

    namespace log {
        using Data = std::vector<uint8_t>;
        using Topic = uint256_t;
    } // namespace log

    struct LogEntry {
        Address address;
        log::Data data;
        std::vector<log::Topic> topics;

        bool operator==(const LogEntry& that) const;

        friend void to_json(nlohmann::json&, const LogEntry&);
        friend void from_json(const nlohmann::json&, LogEntry&);
    };

    void to_json(nlohmann::json&, const LogEntry&);
    void from_json(const nlohmann::json&, LogEntry&);

    struct LogHandler {
        virtual ~LogHandler() = default;
        virtual void handle(LogEntry&&) = 0;
    };

    struct NullLogHandler : public LogHandler {
        virtual void handle(LogEntry&&) override {}
    };

    struct VectorLogHandler : public LogHandler {
        std::vector<LogEntry> logs;

        virtual ~VectorLogHandler() = default;
        virtual void handle(LogEntry&& e) override {
            logs.emplace_back(e);
        }
    };

    /**
   * Represents data of an Ethereum transaction that needs to be persisted to
   * blockchain/ledger
   *
   */
    struct PersistantTransaction {
        Address origin;

        uint64_t value; // call_value
        Code code;

        uint64_t gas_price;
        uint64_t gas_limit;

        std::array<uint8_t, SIG_SIZE_PB_BYTES> signature; // computed over: origin, value, code, gas_price, gas_limit,

        PersistantTransaction(
            const Address origin,
            uint64_t value = 0,
            Code code = {},
            std::array<uint8_t, SIG_SIZE_PB_BYTES> signature = {},
            uint64_t gas_price = 0,
            uint64_t gas_limit = 0) : origin(origin),
                                      value(value),
                                      code(code),
                                      gas_price(gas_price),
                                      gas_limit(gas_limit),
                                      signature(signature) {}

        std::vector<uint8_t> & asDataForHash() {
            std::vector<uint8_t> & ret = *(new std::vector<uint8_t> (sizeof(Address) + sizeof(uint64_t) * 3 + code.size()));

            // construct data object in the order: origin, value, gas_price, gas_limit, code
            // std::vector<uint8_t> addr(32);
            uint8_t addr[32];
            intx::be::unsafe::store((uint8_t *) &addr, this->origin); // convert Address to vector of Bytes ((intx::uint<256>))
            memcpy(ret.data(), addr, 32);

            memcpy(ret.data() + sizeof(Address), &(this->value), sizeof(uint64_t));
            memcpy(ret.data() + sizeof(Address) + sizeof(uint64_t), &(this->gas_price), sizeof(uint64_t));
            memcpy(ret.data() + sizeof(Address) + 2 * sizeof(uint64_t), &(this->gas_limit), sizeof(uint64_t));
            memcpy(ret.data() + sizeof(Address) + 3 * sizeof(uint64_t), this->code.data(), this->code.size());

            return ret;
        };
    };

    /**
   * Ethereum transaction wrapped for need of eEVM
   */
    struct Transaction : PersistantTransaction {
        LogHandler& log_handler;
        std::vector<Address> destroy_list;

        Transaction(
            const Address origin,
            LogHandler& lh,
            Code code = {},
            uint64_t value = 0,
            uint64_t gas_price = 0,
            uint64_t gas_limit = 0,
            std::array<uint8_t, SIG_SIZE_PB_BYTES> signature = {}) : PersistantTransaction(origin, value, code, signature, gas_price, gas_limit),
                                                                     log_handler(lh) {}
    };
} // namespace eevm
