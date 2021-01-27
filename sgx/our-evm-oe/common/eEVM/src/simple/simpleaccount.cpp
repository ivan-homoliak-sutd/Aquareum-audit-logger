// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "eEVM/simple/simpleaccount.h"
#include "eEVM/util.h"

#include "aleth-mp3/Common.h"
#include "aleth-mp3/FixedHash.h"

using namespace dev;

namespace eevm
{
    // SimpleAccount::SimpleAccount(){}
    // SimpleAccount::~SimpleAccount(){}

    Address SimpleAccount::get_address() const
    {
        return address;
    }

    bytesConstRef SimpleAccount::get_address_h256() const
    {
        return bytesConstRef(h256(address).data(), ADDR_SIZE_B);
    }

    void SimpleAccount::set_address(const Address& a)
    {
        address = a;
    }

    uint256_t SimpleAccount::get_balance() const
    {
        return balance;
    }

    void SimpleAccount::set_balance(const uint256_t& b)
    {
        balance = b;
    }

    Account::Nonce SimpleAccount::get_nonce() const
    {
        return nonce;
    }

    void SimpleAccount::set_nonce(Nonce n)
    {
        nonce = n;
    }

    void SimpleAccount::increment_nonce()
    {
        ++nonce;
    }

    Code SimpleAccount::get_code() const
    {
        return code;
    }

    Code& SimpleAccount::get_code_ref()
    {
        return code;
    }

    void SimpleAccount::set_code(Code&& c)
    {
        code = c;
    }

    bool SimpleAccount::has_code()
    {
        return !get_code().empty();
    }

    bool SimpleAccount::operator==(const Account& a) const
    {
        return get_address() == a.get_address() &&
               get_balance() == a.get_balance() && get_nonce() == a.get_nonce() &&
               get_code() == a.get_code();
    }

    // It serializes the Account object into JSON string (further transformed to byte vector)
    // the output is inserted as value to global account state of the ledger
    std::vector<uint8_t>& SimpleAccount::asJsonBytes(std::vector<uint8_t>& output) const
    {
        nlohmann::json j;
        obj_to_json(j);  // populate JSON object
        // to_json(j, *this);
        // std::cerr << "\t SimpleAccount::toString(): " << this->toString() << "\n";
        // std::cerr << "\t SimpleAccount::asJsonBytes: " << j.dump() << "\n";

        std::string s = std::string(j.dump());  // TODO: move?
        output.insert(output.begin(), s.begin(), s.end());
        return output;
    }

    std::string SimpleAccount::toString() const
    {
        std::string codeStr;
        if (code.size() <= 10) {
            codeStr = to_hex_string(code);
        } else {
            codeStr = to_hex_string(code.begin(), code.begin() + 10) + fmt::format(" (size={})", code.size());
        }
        std::string s = fmt::format("{} | bal={} | n={} | strgH={} | c={}",
                                    address_to_hex_string(address),
                                    to_hex_string(balance),
                                    nonce,
                                    to_hex_string(storage_hash),
                                    codeStr);

        return s;
    }

    void SimpleAccount::obj_to_json(nlohmann::json& j) const
    {
        j = nlohmann::json({{"address", address_to_hex_string(address)},
                            {"balance", to_hex_string(balance)},
                            {"nonce", to_hex_string(nonce)},
                            {"code", to_hex_string(code)},
                            {"storage_hash", to_hex_string(storage_hash)}});

        // j["address"] = address_to_hex_string(address);
        // j["balance"] = to_hex_string(balance);
        // j["nonce"] = to_hex_string(nonce);
        // j["code"] = to_hex_string(code);
        // j["storage_hash"] = to_hex_string(storage_hash);
    }  // namespace eevm


    /**
     * @brief More effient serialization of account object.
     * 
     * @param toAppend target for serialization
     */
    void SimpleAccount::toBytes(uint8_t * toAppend) const
    {        
        // store code at the end since it may have variable size
        to_big_endian(address, toAppend);        
        to_big_endian(balance, toAppend + sizeof(uint256_t)); // +32B        
        to_big_endian(storage_hash, toAppend + 2 * sizeof(uint256_t)); // +64B        
        memcpy(toAppend + 3 * sizeof(uint256_t), &nonce, sizeof(Nonce));
        memcpy(toAppend + 3 * sizeof(uint256_t) + sizeof(Nonce), code.data(), code.size());                
    }


    /////////////////////////////////
    // static methods
    /////////////////////////////////

    SimpleAccount* SimpleAccount::fromBytes(const uint8_t* data, size_t size)
    {                
        uint256_t _address = intx::be::unsafe::load<uint256_t>(data);
        uint256_t _balance = intx::be::unsafe::load<uint256_t>(data + sizeof(uint256_t));
        uint256_t _storage_hash = intx::be::unsafe::load<uint256_t>(data + 2 * sizeof(uint256_t));
        size_t _nonce;
        memcpy(&_nonce, data + 3 * sizeof(uint256_t), sizeof(Nonce));
        
        size_t size_of_rest = 3 * sizeof(uint256_t) + sizeof(Nonce);
        Code _code(size - size_of_rest);         
     
        auto* acc = new SimpleAccount(std::move(_address), std::move(_balance), std::move(_code), std::move(_nonce), std::move(_storage_hash));
        return acc;
    }


    void to_json(nlohmann::json& j, const SimpleAccount& a)
    {
        //IH: I do not like that that "https://github.com/nlohmann/json#serialization--deserialization" reconstructs the object
        j = nlohmann::json({{"address", address_to_hex_string(a.address)},
                            {"balance", to_hex_string(a.balance)},
                            {"nonce", to_hex_string(a.nonce)},
                            {"code", to_hex_string(a.code)},
                            {"storage_hash", to_hex_string(a.storage_hash)}});

        // j["address"] = address_to_hex_string(a.address);
        // j["balance"] = to_hex_string(a.balance);
        // j["nonce"] = to_hex_string(a.nonce);
        // j["code"] = to_hex_string(a.code);
        // j["storage_hash"] = to_hex_string(a.storage_hash);
    }

    void from_json(const nlohmann::json& j, SimpleAccount& a)
    {
        if (j.find("address") != j.end()) {
            a.address = to_uint256(j["address"]);
        }

        if (j.find("balance") != j.end()) {
            a.balance = to_uint256(j["balance"]);
        }

        if (j.find("nonce") != j.end()) {
            a.nonce = to_uint64(j["nonce"]);
        }

        if (j.find("code") != j.end()) {
            a.code = to_bytes(j["code"]);
        }

        if (j.find("storage_hash") != j.end()) {
            a.storage_hash = to_uint256(j["storage_hash"]);
        }
    }
}  // namespace eevm
