#pragma once

#include "common.h"
#include "eEVM/bigint.h"
// #include "eEVM/bigint.h"
#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/transaction.h"
#include "eEVM/util.h"

#include "secp256k1.h"
#include "signing.h"
#include <boost/tokenizer.hpp>
#include <nlohmann/json.hpp>

using namespace eevm;

typedef boost::char_separator<char> separator;

using Bytes = std::vector<uint8_t>;

struct CtorPar {
    std::string name;
    std::string type;
    u256 value;

    CtorPar(const std::string& name, const std::string& type, const u256& value)
      : name(name), type(type), value(value)
    {}
};

enum class ParamTypes {
    address,
    uint256
};


struct ContrDefinition {
    std::string name;
    std::vector<uint8_t> bin;                              // code of the contract
    std::vector<std::pair<std::string, Bytes>> endpoints;  // functions available
    std::vector<CtorPar> ctor_params;                      // parameters of ctor (with defaut values)

    // not part of definition
    Address owner = 0u;  // owner of the contract is added after deployment

    Bytes getEpBinByName(std::string name)
    {
        for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
            if (name == it->first) {
                return it->second;
            }
        }
        throw std::logic_error(fmt::format("EP with name = {} does not exist.", name));
    }

    std::vector<ParamTypes> getParamTypesOfEP(uint endpointID)
    {
        if (endpointID >= endpoints.size()) {
            throw std::logic_error(fmt::format("Invalid endpoint ID passed {}; range is <0-{}>\n", endpointID, endpoints.size()));
        }
        std::vector<ParamTypes> ret;

        auto sep = separator{"(,)"};
        auto tokens = boost::tokenizer<separator>{endpoints[endpointID].first, sep};

        // shift to the 1st param type
        auto it = tokens.begin();
        std::advance(it, 1);

        // iterate over all parameters of the requested endpoint
        for (; it != tokens.end(); ++it) {
            TRACE_HOST("param type: %s", (*it).c_str());
            if (*it == "uint256") {
                ret.emplace_back(ParamTypes::uint256);
            } else if (*it == "address") {
                ret.emplace_back(ParamTypes::address);
            } else {
                throw std::logic_error(fmt::format("Unknown parameter type {}", *it));
            }
        }
        return ret;
    }

    std::string toString()
    {
        nlohmann::json j = nlohmann::json::object();
        j["name"] = name;
        j["bin"] = to_hex_string(bin);

        nlohmann::json eps = nlohmann::json::array();
        for (auto& ep : endpoints) {
            eps.push_back(nlohmann::json({{"endpoint", ep.first}, {"bin", to_hex_string(ep.second)}}));
        }
        j["endpoints"] = eps;

        nlohmann::json ctor = nlohmann::json::array();
        for (auto& ep : ctor_params) {
            ctor.push_back(nlohmann::json({{"name", ep.name}, {"type", ep.type}, {"value", ep.value}}));
        }
        j["ctor"] = ctor;
        j["owner"] = to_hex_string(owner);

        return j.dump(4);
    }
};


struct OperAccount {
    eevm::Address addr;
    uint8_t SK[ECC_SK_SIZE];
    secp256k1_pubkey PK;

    OperAccount() = default;
    OperAccount(const uint8_t* sk, const secp256k1_pubkey* pk, eevm::Address a)
      : addr(a)
    {
        memcpy(SK, sk, ECC_SK_SIZE);
        memcpy(&PK, pk, sizeof(secp256k1_pubkey));
    }
};

class AQLedger {
public:

    NormalGlobalState m_gs;  // the full global state of the ECL ledger

    ECC* m_ecc;  // ECC signing wrapper

    MODE m_mode;

    eevm::Address operAddr;

    inline AQLedger(ECC* e)
      : m_ecc(e), m_mode(MODE::FullStateMaintained){};

    PersistantTransaction* createHelloWorldTX(OperAccount& sender,
                                              size_t nonce);

    PersistantTransaction* createSumTx(int a, int b,
                                       secp256k1_pubkey& PK_sender,
                                       uint8_t* SK_sender,
                                       size_t nonce);

    PersistantTransaction* createIncCounterTX(secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender);

    PersistantTransaction* createDeploymentTX(const ContrDefinition& cdef,
                                              OperAccount& sender,
                                              size_t nonce,
                                              uint64_t value);

    PersistantTransaction* createCallFunctionTX(const OperAccount& sender,
                                                const eevm::Address to,
                                                const std::vector<u256> params,
                                                const Bytes& function_hex_ptr,
                                                const size_t nonce,
                                                const uint64_t value);

    PersistantTransaction* createNewAccountTX(secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender,
                                              const Address& newAddr,
                                              unsigned initBalance,
                                              size_t nonce);

    int executeTX(PersistantTransaction* tx, uint256_t& result_u256);

private:
    int _execute_transfer_tx(Transaction& etx);
};
