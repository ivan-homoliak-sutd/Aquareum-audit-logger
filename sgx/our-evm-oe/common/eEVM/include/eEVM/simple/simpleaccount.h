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
    protected:
        Address address = {};
        uint256_t balance = 0u;
        Code code = {0u};
        uint256_t storage_hash = {0};  // the integrity value of the storage related to this account (might be the root hash of MP3 or just hash of the set)
        Nonce nonce = 0;               // the number of TXs send by the owner of the account

    public:
        SimpleAccount()
          : storage_hash(std::move(SimpleStorage::hashOfEmptyStorage())){};

        SimpleAccount(const SimpleAccount& other)  // copy ctor
          : address(other.address),
            balance(other.balance),
            code(other.code),
            storage_hash(other.storage_hash),
            nonce(other.nonce)
        {}

        SimpleAccount(SimpleAccount&& other)  // IH: movable ctor - can be optimized by making move to intx
          : address(std::move(other.address)),
            balance(std::move(other.balance)),
            code(std::move(other.code)),
            storage_hash(std::move(other.storage_hash))
        {
            nonce = std::exchange(other.nonce, INT_MOVED);
        }

        SimpleAccount(const Address& a, const uint256_t& b, const Code& c)
          : address(a),
            balance(b),
            code(c),
            storage_hash(std::move(SimpleStorage::hashOfEmptyStorage())),
            nonce(0)
        {}

        SimpleAccount(const Address& a, const uint256_t& b, const Code& c, Nonce n)
          : address(a),
            balance(b),
            code(c),
            storage_hash(std::move(SimpleStorage::hashOfEmptyStorage())),
            nonce(n)
        {}

        SimpleAccount(const Address& a, const uint256_t& b, const Code& c, Nonce n, Storage& s)
          : address(a),
            balance(b),
            code(c),
            storage_hash(s.hash()),
            nonce(n)
        {}

        // movable constructor utilized for deserialization
        SimpleAccount(const Address&& a, const uint256_t&& b, const Code&& c, Nonce&& n, uint256_t&& storage_h)
          : address(std::move(a)), // IH: is it necessary to explicitly write move?
            balance(std::move(b)), 
            code(std::move(c)),            
            storage_hash(std::move(storage_h))
        {
          nonce = std::exchange(n, INT_MOVED);
        }

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

        void toBytes(uint8_t * toAppend) const;

        bool operator==(const Account&) const;
        SimpleAccount& operator=(const SimpleAccount& a)
        {
            address = a.address;
            balance = a.balance;
            code = a.code;
            storage_hash = a.storage_hash;
            nonce = a.nonce;
            return *this;
        }

        static SimpleAccount* fromBytes(const uint8_t* data, size_t size);

        virtual std::vector<uint8_t>& asJsonBytes(std::vector<uint8_t>& output) const override;

        inline size_t sizeB() const { 
          constexpr size_t fixed_size = sizeof(Address) + 2 * sizeof(uint256_t) + sizeof(Nonce);                    
          return code.size() + fixed_size;
        };

        std::string toString() const;
        void obj_to_json(nlohmann::json& j) const;

        friend void to_json(nlohmann::json&, const SimpleAccount&);
        friend void from_json(const nlohmann::json&, SimpleAccount&);
    };

    void to_json(nlohmann::json&, const SimpleAccount&);
    void from_json(const nlohmann::json&, SimpleAccount&);
}  // namespace eevm
