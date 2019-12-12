// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "address.h"

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
        const Address origin;

        const uint64_t value; // call_value
        const Code code;

        const uint64_t gas_price;
        const uint64_t gas_limit;

        std::array<uint8_t, SIG_SIZE_PB_BYTES> signature; // computed over: origin, value, code, gas_price, gas_limit,

        PersistantTransaction(
            const Address origin,
            uint64_t value = 0,
            Code code = {},
            uint64_t gas_price = 0,
            uint64_t gas_limit = 0,
            std::array<uint8_t, SIG_SIZE_PB_BYTES> signature = {}) : origin(origin),
                                                                     value(value),
                                                                     code(code),
                                                                     gas_price(gas_price),
                                                                     gas_limit(gas_limit),
                                                                     signature(signature) {}
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
            std::array<uint8_t, SIG_SIZE_PB_BYTES> signature = {}) : PersistantTransaction(origin, value, code, gas_price, gas_limit, signature),
                                                                     log_handler(lh) {}
    };
} // namespace eevm
