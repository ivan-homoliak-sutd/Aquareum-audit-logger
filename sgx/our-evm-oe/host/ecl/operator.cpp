#include "operator.h"
#include "common.h"
#include "secp256k1.h"
#include "utils.h"

#include <boost/tokenizer.hpp>
#include <fmt/format_header_only.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

using namespace ecl;

Operator::Operator(secp256k1_pubkey* _enc_PK)
  : ecl()
{
    // If keys were generated and persisted before, just load them, otherwise generate new keys
    if (this->existsMyKeyFile()) {
        info_print(string("loading operator's keys from file."));
        if (RET_SUCCESS != this->loadMyKeysFromFile()) {
            error_print(string("Error when loading operator's keys."));
            return;
        }
    } else {
        info_print(string("generating new operator's keys."));

        // 1) compute SK of operator (under PB)
        int rc = RAND_priv_bytes((unsigned char*)&this->SK_O, ECC_SK_SIZE);
        if (rc != 1) {
            unsigned long err = ERR_get_error();
            error_print(string("RAND_pseudo_bytes failed, err = ") + std::to_string(err));
            return;
        }

        this->ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);  // ECC context

        // 2) compute PK of operator (under PB)
        if (1 != secp256k1_ec_pubkey_create(this->ctx, &this->PK_O, (const uint8_t*)&this->SK_O)) {
            error_print(string("secp256k1_ec_pubkey_create failed"));
            return;
        }
        this->persistMyKeys();
    }
    memcpy(this->PK_E_PB.data, _enc_PK->data, ECC_PK_SIZE);

    info_print(string("PK_E_PB = ") + to_hex_str(_enc_PK->data, ECC_PK_SIZE));
    info_print(string("SK_O = ") + to_hex_str(this->SK_O, ECC_SK_SIZE));
    info_print(string("PK_O = ") + to_hex_str((const unsigned char*)&this->PK_O, ECC_PK_SIZE));
}

int Operator::loadMyKeysFromFile()
{
    ifstream file(FILE_OPERATOR_KEYS, ios::in | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.read((char*)this->SK_O, ECC_SK_SIZE);
    file.read((char*)this->PK_O.data, ECC_PK_SIZE);
    file.close();
    return 0;
}

bool Operator::existsMyKeyFile()
{
    struct stat buffer;
    if (0 != stat(FILE_OPERATOR_KEYS, &buffer)) {
        return false;
    }
    return true;
}

int Operator::persistMyKeys()
{
    ofstream file(FILE_OPERATOR_KEYS, ios::out | ios::binary);
    if (file.fail()) {
        return ERR_SAVING_OPER_KEYS;
    }
    file.write((const char*)this->SK_O, ECC_SK_SIZE);
    file.write((const char*)this->PK_O.data, ECC_PK_SIZE);
    file.close();
    return RET_SUCCESS;
}

//////////////////// AUX ////////////////////

typedef boost::char_separator<char> separator;
auto sep = separator{" "};


bool correct_token_cnt(std::string& command, uint N, boost::tokenizer<separator>** tokens)
{
    *tokens = new boost::tokenizer<separator>{command, sep};
    auto _tokens = *tokens;
    // std::cerr << "tokens = " << _tokens << "\n";

    // std:cerr << "distance = " << std::distance(tokens->begin(), tokens->end()) << "\n";
    if (std::distance(_tokens->begin(), _tokens->end()) != N) {
        std::cerr << "wrong token count: " << std::distance(_tokens->begin(), _tokens->end()) << "\n";
        return false;
    }

    return true;
}

void Operator::print_evm_state(PublicSealedData_T& es)
{
    cout << "\t PK_E_PB = " << to_hex_str(this->PK_E_PB.data, ECC_PK_SIZE) << "\n"
         << "\t PK_O = " << to_hex_str((const unsigned char*)&this->PK_O, ECC_PK_SIZE) << "\n"
         << "\t SK_O = " << to_hex_str((const unsigned char*)&this->SK_O, ECC_SK_SIZE) << "\n";

    cout << fmt::format("\t hdrLast[{}] = ", es.idCurrent) << to_hex_str(es.hdrLast, HASH_SIZE) << "\t(the last header created by E)\n"
         << "\t logRootPB  = " << to_hex_str(es.logRootPB, HASH_SIZE) << "\t(the last root of L flushed to PB)\n"
         << "\t globStRoot = " << to_hex_str(es.globStRoot, HASH_SIZE) << "\t(the actual global state root in E; not flushed to PB)\n"
         << "\t |txsErrCache| = " << es.txsErrCache.count << "\n"
         << "\t diskInits = " << es.diskInits << "\n";

    eevm::print_sep();
}

////////////////////////////////////////
// Processing commands from operator  //
////////////////////////////////////////

void Operator::operatorLoop(oe_enclave_t* enclave)
{
    int ret;                // internal return value
    oe_result_t ecall_ret;  // return value of general enclave call
    char command[MAX_CMD_LEN];
    boost::tokenizer<separator>* tokens = NULL;  // tokens object for parsing command line
    string command_s;

    this->createNRandomAccounts(30, 1, enclave);

    while (true) {
        if (tokens) {
            free(tokens);
            tokens = NULL;
        }

        cout << "$>";
        cin.getline(command, MAX_CMD_LEN);
        command_s = string(command);

        if (0 == strcmp(command, "help") || 0 == strcmp(command, "h")) {
            std::cout << "Supported commands are:\n"
                      << "\t show:"
                      << "\t display info about operator and enclave."
                      << "\n"
                      << "\t test:"
                      << "\t create some TX in enclave and run it there."
                      << "\n"
                      << "\t tx:"
                      << "\t create TX that returns hello word string and send it to enclave."
                      << "\n"
                      << "\t tx add a b:"
                      << " create TX that sums {a} and {b} in host and send it to enclave."
                      << "\n"
                      << "\n";
        } else if (0 == strcmp(command, "show") || 0 == strcmp(command, "s")) {
            PublicSealedData_T pub_evm_state;
            ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
            if (ecall_ret != OE_OK && is_error(ret)) {
                error_print("Failed to read the state of enclave.");
            }
            this->print_evm_state(pub_evm_state);
        } else if (0 == strcmp(command, "test")) {
            info_print("Invoking internally generated TXs in enclave...");

            ecall_ret = ecall_enclave_ecledger(enclave);
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when invoking internal TX generation.");
            }
            // info_print("...done");
        } else if (0 == strncmp(command, "tx add", 6)) {
            if (!correct_token_cnt(command_s, 4, &tokens))
                continue;

            int a, b;
            try {
                auto it = tokens->begin();
                std::advance(it, 2);
                a = std::stoi(*it);
                b = std::stoi(*std::next(it));
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }
            INFO_PRINT("Creating TX that sums %d + %d ...", a, b);

            // create TX using eEVM
            eevm::PersistantTransaction* tx = this->ecl.createSumTx(a, b, this->PK_O, this->SK_O, *(this->ctx));
            ecall_ret = ecall_run_single_tx_simplestate(enclave, &ret,
                                                        (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                        (const uint8_t*)tx->code.data(), tx->code.size());
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when processing sum TX in Enclave.");
            }
        } else if (0 == strcmp(command, "tx inc")) {
            info_print("Creating increment counter TX ...");

            // create and sign TX
            eevm::PersistantTransaction* tx = this->ecl.createIncCounterTX(this->PK_O, this->SK_O, *(this->ctx));

            ecall_ret = ecall_run_single_tx_simplestate(enclave, &ret,
                                                        (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                        (const uint8_t*)tx->code.data(), tx->code.size());
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when processing increment counter TX in Enclave.");
            }

        } else if (0 == strncmp(command, "deploy ", 7)) {
            info_print("Creating contract ...");
            if (!correct_token_cnt(command_s, 2, &tokens))
                continue;

            auto it = tokens->begin();
            std::advance(it, 1);

            const auto contract_path = *it;
            std::ifstream contract_fstream(contract_path);
            if (!contract_fstream) {
                error_print(fmt::format("Unable to open contract definition file: \"{}\"", contract_path));
                continue;
            }

            // Parse the contract definition from file
            const auto contracts_definition = nlohmann::json::parse(contract_fstream);
            const auto all_contracts = contracts_definition["contracts"];
            if (1 != all_contracts.size()) {
                error_print("Multiple contracts found in the definition file... just is supported for now.");
                continue;
            }

            const auto cit = all_contracts.begin();
            info_print(fmt::format("Processing contract definition called: '{}'", cit.key()));
            const auto& contract_definition = cit.value();

            debug_print("1");
            // create and sign deployment TX
            eevm::PersistantTransaction* tx = this->ecl.createDeploymentTX(contract_definition, this->PK_O, this->SK_O, *(this->ctx));

            // dump DB into basic C types (to be passed into enclave)
            // global account state
            std::vector<uint8_t> db_keys;
            std::vector<uint8_t> db_values;
            std::vector<size_t> values_sizes;
            size_t db_keys_size, values_sizes_size;
            // storages of all accounts
            std::vector<uint8_t> storages;
            std::vector<size_t> storages_sizes;
            size_t storages_sizes_size;

            debug_print("2");
            // std::cout << "Current account state tree is:\n" <<  ecl.m_gs.getAccounts();
            // print_sep()

            ecl.m_gs.dump_full_db(db_keys, db_values, values_sizes, db_keys_size, values_sizes_size, storages, storages_sizes, storages_sizes_size);

            info_print(fmt::format("Size of state passed to E: (accounts = {}B + {}B | storages = {}B)", db_keys_size, sumVectST(values_sizes), sumVectST(storages_sizes)));
            debug_print(fmt::format("values_sizes_size = {}", values_sizes_size));

            ecall_ret = ecall_run_single_tx_mp3state_full(enclave, &ret,
                                                          (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                          (const uint8_t*)tx->code.data(), tx->code.size(),
                                                          (const uint8_t*)db_keys.data(), db_keys_size,
                                                          (const uint8_t*)db_values.data(), values_sizes.data(), values_sizes_size,
                                                          (const uint8_t*)storages.data(), storages_sizes.data(), storages_sizes_size);

            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when deploying contract in Enclave.");
            }

            // Create a new account entry in the global state of host and compare the root hash with the one from the enclave
            debug_print(std::string("host->state_root before TX = ") + ecl.m_gs.root().hex());
            ecl.m_gs.create(tx->to, tx->value, tx->code);
            debug_print(std::string("host->state_root after TX = ") + ecl.m_gs.root().hex());
            // TODO: ...
            // assert(ecl.m_gs.root() == ...);

        } else if (0 == strcmp(command, "tx")) {
            info_print("Creating hello world TX ...");

            // create and sign TX
            eevm::PersistantTransaction* tx = this->ecl.createHelloWorldTX(this->PK_O, this->SK_O, *(this->ctx));

            ecall_ret = ecall_run_single_tx_simplestate(enclave, &ret,
                                                        (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                        (const uint8_t*)tx->code.data(), tx->code.size());
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when processing print hello world TX in Enclave.");
            }
        } else if (0 == strcmp(command, "q") || 0 == strcmp(command, "quit")) {
            info_print("Syncing sealed state of enclave to disk...");
            ecall_ret = ecall_sync_evm_sealed_state_to_disk(enclave, &ret);
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when syncing sealed state.");
            }
            std::cout << "Operator shell quits...\n";
            break;
        } else {
            std::cout << "Unknown command\n";
        }
    }
}

/**
 * Create N simple accounts with initial balance set to 'initBalance'
 * For each account creation, do ecall into E.
 */
void Operator::createNRandomAccounts(unsigned N, unsigned initBalance, oe_enclave_t* enclave)
{
    int ret;
    for (unsigned i = 0; i < N; i++) {
        // debug_print(fmt::format("\t creating random account: {} ", i));
        std::vector<uint8_t> raw_address(20);
        std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
        const eevm::Address addr = eevm::from_big_endian(raw_address.data(), raw_address.size());

        auto* tx = this->ecl.createNewAccountTX(this->PK_O, this->SK_O, *(this->ctx), addr, initBalance);

        // dump global MP3 state into basic C types (to be passed into enclave)
        std::vector<uint8_t> db_keys;  // \/== global account state
        std::vector<uint8_t> db_values;
        std::vector<size_t> values_sizes;
        size_t db_keys_size, values_sizes_size;
        std::vector<uint8_t> storages;  // \/== storages of all accounts
        std::vector<size_t> storages_sizes;
        size_t storages_sizes_size;
        ecl.m_gs.dump_full_db(db_keys, db_values, values_sizes, db_keys_size, values_sizes_size, storages, storages_sizes, storages_sizes_size);

        info_print(fmt::format("Size of state passed to E: (accounts = {}B + {}B | storages = {}B)", db_keys_size, sumVectST(values_sizes), sumVectST(storages_sizes)));

        // ecall
        oe_result_t ecall_ret = ecall_run_single_tx_mp3state_full(enclave, &ret,
                                                                  (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                                  (const uint8_t*)tx->code.data(), tx->code.size(),
                                                                  (const uint8_t*)db_keys.data(), db_keys_size,
                                                                  (const uint8_t*)db_values.data(), values_sizes.data(), values_sizes_size,
                                                                  (const uint8_t*)storages.data(), storages_sizes.data(), storages_sizes_size);

        if (ecall_ret != OE_OK || is_error(ret)){
            error_print("Error when executing TX in ENCLAVE.");
            return;
        }

        if (RET_SUCCESS != this->ecl.executeTX(tx)){ // this updates global account state in the host
            error_print("Error when executing TX in HOST.");
            return;
        }

        // Fetch the updated global state of E
        PublicSealedData_T pub_evm_state;
        ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
        if (ecall_ret != OE_OK && is_error(ret))
            error_print("Failed to read the state of enclace.");

        // Compare E's state to host's state
        assert(eevm::from_big_endian(pub_evm_state.globStRoot) == this->ecl.m_gs.root());

        eevm::AccountState accntState = this->ecl.m_gs.get(addr);
        debug_print(fmt::format("created account: {} ", accntState.acc.asJsonBytesRef().toString()));
    }
    eevm::print_sep();
}
