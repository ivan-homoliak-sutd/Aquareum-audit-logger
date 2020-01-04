// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "eEVM/account.h"
#include "eEVM/simple/simplestorage.h"

#include "aleth-mp3/Common.h"
#include <nlohmann/json.hpp>

using namespace dev;

namespace eevm
{
    /**
   * Simple implementation of Account
   */
    class SimpleAccount : public Account {
    private:
        Address address = {};
        uint256_t balance = {};
        Code code = {};
        Nonce nonce = {};             // the number of TXs send by the owner of the account
        uint256_t storage_hash = {};  // the integrity value of the storage related to this account (might be the root hash of MP3 or just hash of the set)

    public:
        SimpleAccount()
          : storage_hash(std::move(SimpleStorage::hashOfEmptyStorage())){};

        SimpleAccount(const SimpleAccount& other)  // copy ctor
        {
            address = other.address;
            balance = other.balance;
            code = other.code;
            nonce = other.nonce;
            storage_hash = other.storage_hash;
        }
        SimpleAccount(SimpleAccount&& other)  // IH: movable ctor - can be optimized by making move to intx
        {
            address = other.address;
            balance = other.balance;
            code = std::move(other.code);
            nonce = std::exchange(other.nonce, INT_MOVED);
            storage_hash = other.storage_hash;
        }

        SimpleAccount(const Address& a, const uint256_t& b, const Code& c)
          : address(a),
            balance(b),
            code(c),
            nonce(0),
            storage_hash(std::move(SimpleStorage::hashOfEmptyStorage()))
        {}

        SimpleAccount(const Address& a, const uint256_t& b, const Code& c, Nonce n)
          : address(a),
            balance(b),
            code(c),
            nonce(n),
            storage_hash(std::move(SimpleStorage::hashOfEmptyStorage()))
        {}

        SimpleAccount(const Address& a, const uint256_t& b, const Code& c, Nonce n, Storage& s)
          : address(a),
            balance(b),
            code(c),
            nonce(n),
            storage_hash(s.hash())
        {}

        // SimpleAccount(
        //     const Address& a, const uint256_t& b, const Code& c, Nonce n, uint256_t storage_h)
        //   : address(a),
        //     balance(b),
        //     code(c),
        //     nonce(n),
        //     storage_hash(storage_h)
        // {}

        virtual Address get_address() const override;
        virtual bytesConstRef get_address_h256() const;

        void set_address(const Address& a);

        virtual uint256_t get_balance() const override;
        virtual void set_balance(const uint256_t& b) override;

        virtual Nonce get_nonce() const override;
        void set_nonce(Nonce n);
        virtual void increment_nonce() override;

        virtual Code get_code() const override;
        virtual Code& get_code_ref() override;
        virtual void set_code(Code&& c) override;
        virtual bool has_code() override;

        inline uint256_t& get_stHash() { return storage_hash; };
        inline void set_stHash(uint256_t h) { storage_hash = h; };

        bool operator==(const Account&) const;
        SimpleAccount& operator=(const SimpleAccount&) = default;

        virtual std::vector<uint8_t>& asJsonBytes(std::vector<uint8_t>& output) override;

        std::string toString() const;

        friend void to_json(nlohmann::json&, const SimpleAccount&);
        friend void from_json(const nlohmann::json&, SimpleAccount&);
    };

    void to_json(nlohmann::json&, const SimpleAccount&);
    void from_json(const nlohmann::json&, SimpleAccount&);
}  // namespace eevm
