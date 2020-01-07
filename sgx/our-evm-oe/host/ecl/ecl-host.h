#pragma once

#include "common.h"
#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/transaction.h"
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
    std::vector<byte> bin;                                 // code of the contract
    std::vector<std::pair<std::string, Bytes>> endpoints;  // functions available
    std::vector<CtorPar> ctor_params;                      // parameters of ctors (with defaut values)

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
            if (*it == "uint256") {
                ret.emplace_back(ParamTypes::uint256);
            } else if (*it == "address") {
                ret.emplace_back(ParamTypes::address);
            } else {
                throw std::logic_error(fmt::format("Unknown parameter type {}", *it));
            }
        }
        return ret;
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

    class ECLedger {
    public:
        NormalGlobalState m_gs;  // the full global state of the ECL ledger

        ECC* m_ecc;  // ECC signing wrapper

        eevm::Address operAddr;


        inline ECLedger(ECC* e)
          : m_ecc(e){};

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
                                                    const std::vector<uint256> params,
                                                    const Bytes& function_hex_ptr,
                                                    const size_t nonce,
                                                    const uint64_t value)

            PersistantTransaction* createNewAccountTX(secp256k1_pubkey& PK_sender,
                                                      uint8_t* SK_sender,
                                                      const Address& newAddr,
                                                      unsigned initBalance,
                                                      size_t nonce);

        int executeTX(PersistantTransaction* tx);

    private:
        int _execute_transfer_tx(Transaction& etx);
    };
