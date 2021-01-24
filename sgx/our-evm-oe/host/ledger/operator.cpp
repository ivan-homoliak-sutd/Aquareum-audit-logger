#include "operator.h"
#include "common.h"
#include "secp256k1.h"
#include "utils.h"

#include <boost/tokenizer.hpp>
#include <chrono>
#include <fmt/format_header_only.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

using namespace aql;

Operator::Operator(secp256k1_pubkey* _enc_PK)
  : m_ecc(), m_ledger(&m_ecc)
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

        // 2) compute PK of operator (under PB)
        if (1 != secp256k1_ec_pubkey_create(ECC::s_ctx, &this->PK_O, (const uint8_t*)&this->SK_O)) {
            error_print(string("secp256k1_ec_pubkey_create failed"));
            return;
        }
        this->persistMyKeys();
    }
    memcpy(this->PK_E_PB.data, _enc_PK->data, ECC_PK_SIZE);
    this->m_ledger.operAddr = eevm::from_big_endian(this->PK_O.data, PB_ADDR_SIZE);  // copy the address of O to the ECL object

    info_print(string("PK_E_PB = ") + to_hex_str(_enc_PK->data, ECC_PK_SIZE));
    info_print(string("SK_O = ") + to_hex_str(this->SK_O, ECC_SK_SIZE));
    info_print(string("PK_O = ") + to_hex_str((const unsigned char*)&this->PK_O, ECC_PK_SIZE));
}

void Operator::_sendMyPKtoEnclave(oe_enclave_t* enclave)
{
    int ret;
    oe_result_t ecall_ret = ecall_set_operator_address(enclave, &ret, this->PK_O.data, PK_SIZE_PB);

    if (ecall_ret != OE_OK || is_error(ret)) {
        error_print("Error when passing operator's PK to Enclave.");
    }
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

const std::string expand_var(const std::string token_text, std::unordered_map<std::string, std::string>& sh_vars)
{
    if (token_text.substr(0, 1) == "$" && sh_vars.end() != sh_vars.find(token_text)) {
        return sh_vars[token_text];
    } else {
        return token_text;
    }
}

std::string expand_vars(const char* command, std::unordered_map<std::string, std::string>& sh_vars)
{
    auto sep = separator{" \t"};
    auto tmp = std::string(command);
    auto tokens = boost::tokenizer<separator>{tmp, sep};

    std::string ret("");

    // iterate over all parameters of the requested endpoint
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        // debug_print(fmt::format("token: {}", *it));
        ret.append(expand_var(*it, sh_vars));
        ret.append(" ");
    }
    return ret;  // RVO
}

bool correct_token_cnt(std::string& command, std::set<unsigned> allowedCnts, boost::tokenizer<separator>** tokens, uint* cnt = NULL)
{
    *tokens = new boost::tokenizer<separator>{command, sep};
    auto _tokens = *tokens;

    auto distance = std::distance(_tokens->begin(), _tokens->end());
    if (allowedCnts.end() == allowedCnts.find(distance)) {
        std::cerr << "wrong token count: " << std::distance(_tokens->begin(), _tokens->end()) << "\n";
        return false;
    }
    if (NULL != cnt)
        *cnt = distance;
    return true;
}

void Operator::_printEvmState(PublicSealedData_T& es)
{
    cout << "\t PK_E_PB = " << to_hex_str(this->PK_E_PB.data, ECC_PK_SIZE) << "\n"
         << "\t PK_O = " << to_hex_str((const unsigned char*)&this->PK_O, ECC_PK_SIZE) << "\n"
         << "\t SK_O = " << to_hex_str((const unsigned char*)&this->SK_O, ECC_SK_SIZE) << "\n"
         << "\t ADDR of O = " << eevm::address_to_hex_string(this->m_ledger.operAddr) << "\n";

    cout << fmt::format("\t block [{}]:\n", es.idCurrent)
         //  << to_hex_str(es.hdrLast, HASH_SIZE) << "\t(the last header created by E)\n"
         << "\t logRootPB  = " << to_hex_str(es.logRootPB, HASH_SIZE) << "\t(the last root of L flushed to PB)\n"
         << "\t globStRoot = " << to_hex_str(es.globStRoot, HASH_SIZE) << "\t(the actual global state root in E; not flushed to PB)\n"
         << "\t txsRoot  = " << to_hex_str(es.txsRoot, HASH_SIZE) << "\t(Merkle Root of TXs in the last block processed by E)\n"
         << "\t rcpsRoot  = " << to_hex_str(es.rcpsRoot, HASH_SIZE) << "\t(Merkle Root of TX receipts in the last block processed by E)\n"
         //  << "\t |txsErrCache| = " << es.txsErrCache.count << "\n"
         << "\t diskInits = " << es.diskInits << "\n";

    eevm::print_sep();
}

void Operator::_printGlobalState(unsigned max = 1000)
{
    std::cout << "\nGlobal state of host contains accounts:\n";

    nlohmann::json j;

    unsigned i = 1;
    for (const auto& a : this->m_ledger.m_gs.getAccounts()) {
        j = nlohmann::json::parse(a.second.toString());

        SimpleAccount acc;
        eevm::from_json(j, acc);
        std::string superTag = (this->m_ledger.operAddr == acc.get_address()) ? "*" : " ";  // mark SUPER account of O

        std::string contrTag = "";
        if (acc.get_code_ref() != EMPTY_CODE_OBJ) {
            std::string name = (m_contracts.end() != m_contracts.find(acc.get_address())) ? m_contracts[acc.get_address()].name : "-";
            contrTag.append(fmt::format(" [{}]", name));  // show name of contract if any
        }

        std::cout << fmt::format("\t{}[{}] {}{}\n", superTag, i++, acc.toString(), contrTag);
        if (i - 1 == max) {
            break;
        }
    }
    if (i - 1 == max) {
        // std::cout << fmt::format("... {} accounts were omitted ... \n", this->m_ledger.m_gs.getAccounts().size() - (max - 1)); // IH: this is still buggy
        std::cout << fmt::format("... some accounts were omitted ... \n");
    }
    eevm::print_sep();
}


void Operator::_printTrailOfMP3Leaf(Address& key)
{
    std::cout << "\n MP3 Printing trail of address: " << to_hex_string(key) << "\n";
    auto& accounts = this->m_ledger.m_gs.getAccounts();
    auto it = accounts.lower_bound(h256(key));

    // print trail of iterator
    uint k = 0;
    auto& trail = it.get_trail();
    for (auto& node : trail) {
        dev::RLP rlp = dev::RLP(node.rlp);
        std::string mp3_data = "";

        if (2 == rlp.itemCount() && dev::isLeaf(rlp)) {  // has 2 items
            // append partial nibble
            std::stringstream s;
            s << dev::keyOf(rlp);
            mp3_data += fmt::format("[Leaf]\t k={} | ", s.str());
            // append data of entry
            mp3_data += fmt::format("v={}\n", rlp[1].toString());

        } else if (2 == rlp.itemCount()) {  //  extension node (or in a special case might be empty root node)
            std::stringstream s;
            s << dev::keyOf(rlp);
            auto h = h256(rlp[1]);
            mp3_data += fmt::format("[Extension] path={} | db_k={}\n", s.str(), h.hex());

        } else {  // branch node
            assert(17 == rlp.itemCount());
            mp3_data += "[Branch]\n";
            uint j = 0;
            for (auto r : rlp) {
                auto h = h256(r);
                std::string idx = (j != 16) ? fmt::format("{}", j) : "val";
                mp3_data += fmt::format("\t\t {} : {}\n", idx, h.hex());
                j++;
            }
        }

        // parse partial key in Node struct

        auto h = dev::sha3(rlp.data());
        std::cout << fmt::format("\t\t[{}]  db_k {} => {} \n", k++, h.hex(), mp3_data);
    }
    print_sep();
}

void Operator::_iterExps(Address& key)
{
    auto& accounts = this->m_ledger.m_gs.getAccounts();

    // normal iterator - passes only leafs
    std::cout << "\n MP3 Normal iterator starting from node: " << to_hex_string(key) << "\n";
    uint i = 1;
    for (auto it = accounts.lower_bound(h256(key)); it != accounts.end(); ++it) {  //
        nlohmann::json j = nlohmann::json::parse((*it).second.toString());

        SimpleAccount acc;
        eevm::from_json(j, acc);
        std::cout << fmt::format("\t[{}] {}\n", i++, acc.toString());
    }
    print_sep();

    std::cout << "\n MP3 Full DB iterator:\n";
    i = 1;
    for (auto it = accounts.beginFullDB(); it != accounts.endFullDB(); ++it) {
        h256 db_key = (*it).first;
        // auto mp3_entry_raw = (*it).second.toString();

        dev::RLP rlp = dev::RLP((*it).second);
        std::string mp3_data = "";

        if (2 == rlp.itemCount() && dev::isLeaf(rlp)) {  // has 2 items
            // append partial nibble
            std::stringstream s;
            s << dev::keyOf(rlp);
            mp3_data += fmt::format("[Leaf]\t k={} | ", s.str());

            // append data of entry
            mp3_data += fmt::format("v={}\n", rlp[1].toString());

        } else if (2 == rlp.itemCount()) {  //  extension node (or in a special case might be empty root node)
            std::stringstream s;
            s << dev::keyOf(rlp);
            auto h = h256(rlp[1]);
            mp3_data += fmt::format("[Extension] path={} | db_k={}\n", s.str(), h.hex());

        } else {  // branch node
            assert(17 == rlp.itemCount());
            mp3_data += "[Branch]\n";
            int j = 0;
            for (auto r : rlp) {
                auto h = h256(r);
                std::string idx = (j != 16) ? fmt::format("{}", j) : "val";
                mp3_data += fmt::format("\t\t {} : {}\n", idx, h.hex());
                j++;
            }
        }

        std::cout << fmt::format("\t[{}] db_k: {} => {} \n", i++, db_key.hex(), mp3_data);
    }
    print_sep();
}

ContrDefinition Operator::_parseDefinitionFile(const std::string& contract_path)
{
    std::ifstream contract_fstream(contract_path);
    if (!contract_fstream) {
        error_print(fmt::format("Unable to open contract definition file: \"{}\"", contract_path));
        throw std::logic_error("Unable to open contract definition file");
    }

    // parsing JSON
    const auto contracts_definition = nlohmann::json::parse(contract_fstream);
    const auto all_contracts = contracts_definition["contracts"];


    // if (1 != all_contracts.size()) {
    //     error_print("Multiple contracts found in the definition file... just one is supported for now.");
    //     throw std::logic_error("Multiple contracts found in the definition file");
    // }

    auto conDef = ContrDefinition();

    // skip all imported contracts and go to the last one
    auto cit = all_contracts.begin();
    std::advance(cit, all_contracts.size() - 1);

    info_print(fmt::format("Processing contract definition called: '{}'", cit.key()));
    conDef.name = std::string(cit.key().substr(0, cit.key().find(".")));
    const auto& contract_definition = cit.value();

    conDef.bin = eevm::to_bytes(contract_definition["bin"]);

    auto endpoints = contract_definition["hashes"];
    for (auto&& e : endpoints.items()) {
        conDef.endpoints.push_back(
            std::make_pair(e.key(), eevm::to_bytes(e.value())));
    }

    // ctor is optional parameter
    bool ctorFound = true;
    try {
        contract_definition.at("ctor");
    } catch (const std::exception& e) {
        ctorFound = false;
    }

    if (ctorFound) {
        for (auto& ctor_param : contract_definition["ctor"]) {
            debug_print(fmt::format("parsing ctor parameter: {} {} => {} ", string(ctor_param["type"]), string(ctor_param["name"]), string(ctor_param["value"])));
            if (string(ctor_param["type"]) == "uint256" || string(ctor_param["type"]) == "address") {
                conDef.ctor_params.emplace_back(string(ctor_param["name"]), string(ctor_param["type"]), std::stoul(string(ctor_param["value"])));
            } else {
                throw std::logic_error(fmt::format("Unsupported type of parameter in contract's constructor: '{}'", string(ctor_param["type"])));
            }
        }
    }

    return conDef;
}

uint256_t get_random_uint256(size_t bytes = 32)
{
    std::vector<uint8_t> raw(bytes);
    std::generate(raw.begin(), raw.end(), []() { return rand(); });
    return eevm::from_big_endian(raw.data(), raw.size());
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
    uint256_t output_u256;  // first 32B output of EVM execution

    this->_sendMyPKtoEnclave(enclave);
    this->_createMyAccntState(enclave);

    // origin and to selected in operator's shell
    eevm::Address sh_origin = this->m_ledger.operAddr;
    eevm::Address sh_to(0u);
    eevm::PersistantTransaction* tx = NULL;  // here will be allocated TX data if needed and freed upon exection

    // shell variables
    std::unordered_map<std::string, std::string> sh_vars;
    sh_vars["$?"] = "NULL";                                     // the last deployed contract
    sh_vars["$ERC"] = "./contracts/erc20/ERC20_combined.json";  // testing definition file
    sh_vars["$KID"] = "./contracts/CTX1/Kid_combined.json";
    sh_vars["$PAR"] = "./contracts/CTX1/Parent_combined.json";
    sh_vars["$O"] = address_to_hex_string(sh_origin);  // operator's super account
    sh_vars["$REPEAT"] = "30";                         // the number of test repetitions for statistical evaluation of mean and std dev


    while (true) {
        if (tokens)
            free(tokens);
        if (tx)
            delete tx;
        tokens = NULL;
        tx = NULL;

        std::string mode_short = (this->m_ledger.m_mode == AQLedger::MODE::FullStateTransfer) ? "F" : ((this->m_ledger.m_mode == AQLedger::MODE::FullStateMaintained) ? "M" : "P");
        std::string operatorFlag = (sh_origin == this->m_ledger.operAddr) ? "<SUPER>" : "";
        std::string toFlag = (m_contracts.end() != m_contracts.find(sh_to)) ? string("<") + m_contracts[sh_to].name + string(">") : "";
        cout << fmt::format("$[from={}..{} | to={}..{}]:({}) $>",
                            address_to_hex_string(sh_origin).substr(0, 8), operatorFlag,
                            address_to_hex_string(sh_to).substr(0, 8), toFlag, mode_short);
        cin.getline(command, MAX_CMD_LEN);
        std::string command_s(expand_vars(command, sh_vars));
        // TRACE_HOST("expanded_cmd = %s", command_s.c_str());

        // shell variables' handling
        if (0 == strncmp(command, "$", 1)) {
            boost::char_separator<char> sep("=");
            auto tmp = string(command);
            auto tokens = boost::tokenizer<separator>{tmp, sep};
            auto cnt = std::distance(tokens.begin(), tokens.end());
            auto it = tokens.begin();

            if (cnt == 1) {  // just display
                std::cout << fmt::format("\t {} = {} \n", *it, sh_vars[*it]);
                continue;
            } else if (cnt != 2) {
                std::cerr << "\t Incorrect arguments.\n";
                continue;
            }

            auto key = *it;
            it++;
            sh_vars[key] = expand_var(*it, sh_vars);  // do expansion also here
            std::cout << fmt::format("\t Setting  {} <= {} \n", key, sh_vars[key]);

        } else if (0 == strcmp(command, "")) {
            continue;

        } else if (0 == strcmp(command, "help") || 0 == strcmp(command, "h")) {
            // clang-format off
            std::cout << "Supported commands are:\n"
                      << "\t show | s"     << "\t display info about operator and enclave.\n"
                      << "\t gs [n]"       << "\t\t display global state with max 'n' entries [default=100].\n"
                      << "\t gen [n]"      << "\t generate 'n' random accounts with initial balance 10 [default=5].\n"
                      << "\t origin a"     << "\t change the active address of origin to 'a' [default=operator's SUPER].\n"
                      << "\t to a"         << "\t\t change the active address of contract destination to 'a'.\n"
                      << "\t deploy f"     << "\t deploy a contract using definition file 'f'.\n"
                      << "\t ep | end"     << "\t display # of endpoints for selected destination contract.\n"
                      << "\t call # [...]" << "\t call endpoint # of selected destination contract with parameters '...'.\n"
                      << "\t pay a [b]"    << "\t pay amount 'a' to address 'b' [default=active destination].\n"
                      << "\t defs"         << "\t\t print loaded definitions of contracts with ctor parameters.\n"
                      << "\t vars"         << "\t\t display defined variables \n"
                      << "\t contracts"    << "\t print all deployed contracts.\n"
                      << "\t mode [m]"     << "\t get the current mode to 'm': 0 for FullStateMaintained | 1 for FullGsTransfer | 2 for PartialGsTransfer \n"                      
                      << "\t mem"          << "\t prints enclave/host memory stats about global state\n"                      

                      << "\n"
                      << "Hardcoded testing:\n"
                      << "\t test"         << "\t\t create some TX in enclave and run it there.\n"
                      << "\t test erc [a][b]" << "\t execute 'a' token transfer TXs (among 5 random accounts) through selected ERC contract, proceessed in batches of size 'b' [default a=10, b=10].\n"
                      << "\t test pay [a][b]" << "\t execute 'a' native payment transfer TXs (among 5 random accounts), proceessed in batches of size 'b' [default a=10, b=10].\n"
                      << "\t tx"           << "\t\t create TX that returns hello word string and send it to enclave.\n"
                      << "\t tx add a b"   << "\t create TX that sums 'a' and 'b' in host and send it to enclave.\n"
                      << "\t iter [a]"     << "\t experimennts with MP3 iterator.\n"
                      << "\t trail [a]"    << "\t print trail of MP3 related to account with address a [default=1st address]. .\n"
                      << "\n";
            // clang-format on

        } else if (0 == strcmp(command, "show") || 0 == strcmp(command, "s")) {
            PublicSealedData_T pub_evm_state;
            ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
            if (ecall_ret != OE_OK && is_error(ret)) {
                error_print("Failed to read the state of enclave.");
            }
            this->_printEvmState(pub_evm_state);

        } else if (0 == strcmp(command, "mem")) {
            // dump memory stats about global state stored within the enclave (i.e., database size)
            std::cout << "Host memory for gs:\n";
            auto db_stats = m_ledger.m_gs.db()->m_stats;

            unsigned total = db_stats.size_main_data + db_stats.size_aux_data + db_stats.size_main_keys + db_stats.size_aux_keys;
            std::cout << fmt::format("\t main data = {}:\n \t main keys= {}\n ", db_stats.size_main_data, db_stats.size_main_keys);
            std::cout << fmt::format("\t aux data  = {}:\n \t aux keys = {}\n ", db_stats.size_aux_data, db_stats.size_aux_keys);
            std::cout << fmt::format("\t total = {}\n", total);


            // TODO
            // std::cout << "\nEnclave memory for gs:\n";
            // ecall_ret = ecall_get_memory_stats(enclave, &ret);
            // if (ecall_ret != OE_OK || is_error(ret)) {
            //     error_print("Failed to get memory stats of enlave.");
            //     return ret;
            // }


        } else if (0 == strcmp(command, "defs")) {
            // dump definitions
            unsigned i = 0;
            std::cout << "All loaded definitions:\n";
            for (auto& d : m_contracts) {
                std::cout << fmt::format("\t [{}] Definition of contract on addr = {}:\n {}\n", ++i, to_hex_string(d.first), d.second.toString());
            }

        } else if (0 == strcmp(command, "vars")) {
            std::cout << "\tAll defined shell variables:\n";
            for (auto& v : sh_vars) {
                std::cout << fmt::format("\t\t {} = {}\n", v.first, v.second);
            }

        } else if (0 == strcmp(command, "contracts")) {
            // dump contract accounts
            unsigned i = 0;
            std::cout << "All contracts deployed by operator:\n";
            for (auto& c : m_contracts) {
                auto acc = this->getAccount(c.first).acc;
                std::string contrTag = fmt::format(" [{}]", m_contracts[acc.get_address()].name);
                std::cout << fmt::format("\t[{}] {}{}\n", i++, acc.toString(), contrTag);
            }
            print_sep();

        } else if (0 == strncmp(command, "gs", 2)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            int n = 100;  // default max no of entries to print
            if (2 == tokenCnt) {
                try {
                    auto it = tokens->begin();
                    std::advance(it, 1);
                    n = std::stoi(*it);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid argument\n";
                    continue;
                }
            }
            try {
                this->_printGlobalState(n);
            } catch (const std::exception& e) {
                std::cerr << "Exception in _printGlobalState: " << e.what() << '\n';
                continue;
            }


        } else if (0 == strncmp(command, "mode", 4)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            if (1 == tokenCnt) {
                std::string m = (this->m_ledger.m_mode == AQLedger::MODE::FullStateTransfer) ? "full GS transfer" : 
                        ((this->m_ledger.m_mode == AQLedger::MODE::FullStateMaintained) ? "full GS is maintained in E" : "partial GS transfer");
                std::cout << "The current mode is: " << m << "\n";
                continue;
            }

            AQLedger::MODE mode;
            try {
                auto it = tokens->begin();
                std::advance(it, 1);
                mode = AQLedger::MODE(std::stoi(*it));
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }
            if(this->m_ledger.m_mode == AQLedger::MODE::FullStateTransfer || this->m_ledger.m_mode == AQLedger::MODE::PartialStateTransfer){
                if(AQLedger::MODE::FullStateMaintained == mode){
                    error_print("Not allowed to change mode from '[Full|Partial]StateTransfer' to 'FullStateMaintained' (since E's full MP3 DB would be outdated).");
                    continue;
                }                
            }

            if (mode != AQLedger::MODE::FullStateTransfer && mode != AQLedger::MODE::PartialStateTransfer && mode != AQLedger::MODE::FullStateMaintained) {
                error_print("Unknown mode. Supported options are [0,1,2].");
                continue;
            }
            m_ledger.m_mode = AQLedger::MODE(mode);

        } else if (0 == strncmp(command, "iter", 4)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            eevm::Address addr;
            if (1 == tokenCnt) {
                addr = (*this->m_ledger.m_gs.getAccounts().begin()).first;
            } else {
                auto it = tokens->begin();
                std::advance(it, 1);
                try {
                    addr = string_to_uint256(*it);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid argument\n";
                    continue;
                }
            }
            this->_iterExps(addr);

        } else if (0 == strncmp(command, "trail", 5)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            eevm::Address addr;
            if (1 == tokenCnt) {
                addr = (*this->m_ledger.m_gs.getAccounts().begin()).first;
            } else {
                auto it = tokens->begin();
                std::advance(it, 1);
                try {
                    addr = string_to_uint256(*it);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid argument\n";
                    continue;
                }
            }
            this->_printTrailOfMP3Leaf(addr);

        } else if (0 == strncmp(command, "origin", 6)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            if (1 == tokenCnt) {
                std::cout << "\t The active address of origin is: " << address_to_hex_string(sh_origin) << operatorFlag << "\n";
                continue;
            }

            auto it = tokens->begin();
            std::advance(it, 1);

            eevm::Address addr = string_to_uint256(*it);
            if (m_accounts.end() == m_accounts.find(addr)) {
                error_print(fmt::format("Requested address {} was not found in local cache... (maybe contract?)", address_to_hex_string(addr)));
                continue;
            }
            info_print(fmt::format("\t The active origin account of shell changed to: {}", address_to_hex_string(addr)));
            sh_origin = addr;

        } else if (0 == strncmp(command, "to", 2)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            if (1 == tokenCnt) {
                std::cout << "\t The active address of destination for contract calls is: " << address_to_hex_string(sh_to) << "\n";
                continue;
            }

            auto it = tokens->begin();
            std::advance(it, 1);
            eevm::Address addr;
            try {
                addr = string_to_uint256(*it);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            if (m_contracts.end() == m_contracts.find(addr)) {
                error_print(fmt::format("Requested address {} was not found in local cache of contracts... (maybe simple account?)", address_to_hex_string(addr)));
                continue;
            }
            info_print(fmt::format("\t The destination account for contract calls of shell changed to: {}\n", address_to_hex_string(addr)));
            sh_to = addr;

        } else if (0 == strncmp(command, "pay", 3)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {2, 3}, &tokens, &tokenCnt))
                continue;

            // parse amount
            uint amount;
            auto it = tokens->begin();
            try {
                std::advance(it, 1);
                amount = std::stoul(*it);
                info_print(fmt::format("Amount = {}", amount));
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            // adjust destination
            Address dest;
            if (2 == tokenCnt) {
                if (sh_to == Address(0u)) {
                    std::cerr << "Destination account for contract calls not selected yet.\n";
                    continue;
                }
                dest = sh_to;
            } else {
                try {
                    std::advance(it, 1);
                    dest = string_to_uint256(*it);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid argument\n";
                    continue;
                }
            }

            // create TX and execute it
            auto selAccnt = getAccount(sh_origin).acc;  // get O's account state
            eevm::Code emptyFunc = {0u};
            tx = this->m_ledger.createCallFunctionTX(m_accounts[sh_origin], dest, {}, emptyFunc, selAccnt.get_nonce(), amount);

            if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
                continue;

        } else if (0 == strncmp(command, "gen", 3)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            uint n = 5;  // default number of random accounts to generate
            uint initBal = 10;
            if (2 == tokenCnt) {
                try {
                    auto it = tokens->begin();
                    std::advance(it, 1);
                    n = std::stoul(*it);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid argument\n";
                    continue;
                }
            }

            auto addrLast = this->_createNRandomAccounts(n, initBal, enclave);

            sh_vars["$?"] = address_to_hex_string(addrLast);  // store the last generated account address into $?
            this->_printGlobalState();
        } else if (0 == strncmp(command, "test erc", 8)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {2, 3, 4}, &tokens, &tokenCnt))
                continue;

            uint accntsCount = 5;  // default number of accounts involved in transactions
            uint n = 10;           // default number of transactions
            uint b = 10;           // default number of TXs in one batch that is processed by E
            uint repetitions;

            if (tokenCnt == 3 || tokenCnt == 4) {
                try {
                    auto it = tokens->begin();
                    std::advance(it, 2);
                    n = std::stoul(*it);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid argument\n";
                    continue;
                }
                if (tokenCnt == 4) {
                    try {
                        auto it = tokens->begin();
                        std::advance(it, 3);
                        b = std::stoul(*it);
                    } catch (const std::invalid_argument& ia) {
                        std::cerr << "Invalid argument\n";
                        continue;
                    }
                }
            }
            if (n < 10) {
                std::cerr << "The minimum number of TXs is 10.\n";
                continue;
            }
            if (b < 1) {
                std::cerr << "The minimal size of the batch is 1.\n";
                continue;
            }
            if (m_contracts.end() == m_contracts.find(sh_to) || m_contracts[sh_to].name != "ERC20") {
                error_print(fmt::format("Selected contract {} is not an ERC20 contract.", address_to_hex_string(sh_to)));
                continue;
            }
            if (m_accounts.size() < accntsCount) {
                error_print(fmt::format("The minimal number of normal accounts (current = {}) needs to be at least {}", m_accounts.size(), accntsCount));
                continue;
            }
            try {
                repetitions = std::stoul(sh_vars["$REPEAT"]);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "The variable $REPEAT is not an integer.\n";
                continue;
            }
            // this->_testBulkERC_1by1(enclave, n, accntsCount, sh_to);
            this->_testBulkERC_batched_repeated(enclave, n, accntsCount, sh_to, b, repetitions);

        } else if (0 == strncmp(command, "test pay", 8)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {2, 3, 4}, &tokens, &tokenCnt))
                continue;

            uint accntsCount = 5;  // default number of accounts involved in transactions
            uint n = 10;           // default number of transactions
            uint b = 10;           // default number of TXs in one batch that is processed by E
            uint repetitions;

            if (tokenCnt == 3 || tokenCnt == 4) {
                try {
                    auto it = tokens->begin();
                    std::advance(it, 2);
                    n = std::stoul(*it);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid argument\n";
                    continue;
                }
                if (tokenCnt == 4) {
                    try {
                        auto it = tokens->begin();
                        std::advance(it, 3);
                        b = std::stoul(*it);
                    } catch (const std::invalid_argument& ia) {
                        std::cerr << "Invalid argument\n";
                        continue;
                    }
                }
            }
            if (n < 10) {
                std::cerr << "The minimum number of TXs is 10.\n";
                continue;
            }
            if (b < 1) {
                std::cerr << "The minimal size of the batch is 1.\n";
                continue;
            }
            if (m_accounts.size() < accntsCount) {
                error_print(fmt::format("The minimal number of normal accounts (current = {}) needs to be at least {}", m_accounts.size(), accntsCount));
                continue;
            }
            try {
                repetitions = std::stoul(sh_vars["$REPEAT"]);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "The variable $REPEAT is not an integer.\n";
                continue;
            }
            // this->_testBulkNativePayments_1by1(enclave, n, accntsCount); // TODO: compare this with the following
            this->_testBulkNativePayments_batched_repeated(enclave, n, accntsCount, b, repetitions);


        } else if (0 == strcmp(command, "test")) {
            info_print("Invoking internally generated TXs in enclave...");

            ecall_ret = ecall_enclave_aqledger(enclave);
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when invoking internal TX generation.");
            }
        } else if (0 == strncmp(command, "call ", 5)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {2, 3, 4, 5, 6, 7, 8, 9, 10}, &tokens, &tokenCnt))  // MAX is 10 params so far
                continue;

            if (m_contracts.end() == m_contracts.find(sh_to)) {
                error_print(fmt::format("Destination address {} was not found in local cache of contracts... (maybe already deleted?)", address_to_hex_string(sh_to)));
                continue;
            }

            auto it = tokens->begin();
            std::advance(it, 1);

            // Scan endpoint ID
            uint endpointID;
            try {
                endpointID = std::stoul(*it);
            } catch (const std::invalid_argument& ia) {
                std::cerr << fmt::format("Invalid argument for endpoint ID ({}). The range for the current contract is: <0-{}>\n", *it, m_contracts[sh_to].endpoints.size() - 1);
                continue;
            }

            // Check the number of endpoint's parameters passed
            auto& cdef = m_contracts[sh_to];
            auto& ep = cdef.endpoints[endpointID];
            vector<ParamTypes> requiredParTypes;
            try {
                requiredParTypes = cdef.getParamTypesOfEP(endpointID);
            } catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
                continue;
            }
            if (tokenCnt - 2 != requiredParTypes.size()) {
                std::cerr << fmt::format("Invalid number of arguments ({}) passed for endpoint #{} (required {}).\n", tokenCnt - 2, endpointID, requiredParTypes.size());
                continue;
            }

            // Process parameters for endpoint
            std::advance(it, 1);
            std::vector<u256> parsedParams;
            uint j = 0;
            for (; it != tokens->end(); ++it, j++) {
                if (ParamTypes::address == requiredParTypes[j]) {
                    parsedParams.push_back(string_to_uint256(*it));  // this might later change
                } else if (ParamTypes::uint256 == requiredParTypes[j]) {
                    parsedParams.push_back(string_to_uint256(*it));
                } else {
                    std::cerr << fmt::format("Invalid parameter type passed {} at parameter position {} \n", (uint)requiredParTypes[j], j);
                    continue;
                }
            }

            INFO_PRINT("Creating TX that calls contract function %s ...", ep.first.c_str());
            auto selAccnt = getAccount(sh_origin).acc;  // get O's account state
            tx = this->m_ledger.createCallFunctionTX(m_accounts[sh_origin], sh_to, parsedParams, ep.second, selAccnt.get_nonce(), 0);

            if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
                continue;

        } else if (0 == strcmp(command, "call") || 0 == strcmp(command, "ep") || 0 == strcmp(command, "end")) {
            if (!correct_token_cnt(command_s, {1}, &tokens))
                continue;

            if (sh_to == u256(0u)) {
                error_print("No destination contract selected.");
                continue;
            } else if (m_contracts.end() == m_contracts.find(sh_to)) {
                error_print(fmt::format("Destination address {} was not found in local cache of contracts... (maybe already deleted?)", address_to_hex_string(sh_to)));
                continue;
            }

            std::cout << fmt::format("\n\t Displaying endpoints of '{}' contract.\n", m_contracts[sh_to].name);
            unsigned ep_id = 0;
            for (auto& ep : m_contracts[sh_to].endpoints) {
                std::cout << fmt::format("\t [#{}] => {} \n", ep_id++, ep.first);
            }
            print_sep();
        } else if (0 == strncmp(command, "tx add", 6)) {
            if (!correct_token_cnt(command_s, {4}, &tokens))
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
            auto selAccnt = getAccount(sh_origin).acc;  // get O's account state
            tx = this->m_ledger.createSumTx(a, b, this->PK_O, this->SK_O, selAccnt.get_nonce());

            if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
                continue;

            // [Alternative] executing TX in E while using E's full state
            // ecall_ret = ecall_run_single_tx_simplestate(enclave, &ret,
            //                                             (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
            //                                             (const uint8_t*)tx->code.data(), tx->code.size());
            // if (ecall_ret != OE_OK || is_error(ret)) {
            //     error_print("Error when processing sum TX in Enclave.");
            // }
        } else if (0 == strcmp(command, "tx inc")) {
            info_print("Creating increment counter TX ...");

            // create and sign TX
            tx = this->m_ledger.createIncCounterTX(this->PK_O, this->SK_O);

            ecall_ret = ecall_run_single_tx_simplestate(enclave, &ret,
                                                        (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                        (const uint8_t*)tx->code.data(), tx->code.size());
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when processing increment counter TX in Enclave.");
            }
        } else if (0 == strncmp(command, "deploy", 6)) {
            info_print("Creating contract ...");
            if (!correct_token_cnt(command_s, {2}, &tokens))
                continue;

            auto it = tokens->begin();
            std::advance(it, 1);

            const auto contract_path = *it;
            ContrDefinition def;

            // Parse the contract definition from file
            try {
                def = this->_parseDefinitionFile(contract_path);
            } catch (const std::exception& e) {
                std::cerr << "Exception occured:" << e.what() << "\n";
                continue;
            }

            // create and sign deployment TX
            auto selAccnt = getAccount(sh_origin).acc;  // get account state of active account
            tx = this->m_ledger.createDeploymentTX(def, m_accounts[sh_origin], selAccnt.get_nonce(), 0);
            if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
                continue;

            info_print(fmt::format("Created contract with addr = {}", address_to_hex_string(tx->to)));
            sh_vars["$?"] = address_to_hex_string(tx->to);
            def.owner = sh_origin;
            m_contracts[tx->to] = def;  // store binding of contract address to its definition
        } else if (0 == strcmp(command, "tx")) {
            info_print("Creating hello world TX ...");
            auto selAccnt = m_ledger.m_gs.get(sh_origin).acc;  // get O's account state

            // create and sign TX
            tx = this->m_ledger.createHelloWorldTX(m_accounts[sh_origin], selAccnt.get_nonce());
            if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
                continue;

            // [Alternative] executing TX in E while using E's full state
            // ecall_ret = ecall_run_single_tx_simplestate(enclave, &ret,
            // (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
            // (const uint8_t*)tx->code.data(), tx->code.size());
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
 * Execute 'numberOfTx' TXs that transfer ERC20 tokens among 'accountsCnt' normal accounts through interaction with address 'erc'.
 * Note that function execute additional 'accountsCnt' TXs to get balances of accounts at the begining.
 */
void Operator::_testBulkERC_1by1(oe_enclave_t* enclave, uint numberOfTx, uint accountsCnt, Address erc)
{
    u256 output_u256;
    auto def = m_contracts[erc];

    // select accountsCnt accounts, where the 1st one is the owner
    std::vector<eevm::Address> selectedAccnts;
    selectedAccnts.push_back(def.owner);
    for (auto it = m_accounts.begin(); it != m_accounts.end() && selectedAccnts.size() < accountsCnt; ++it) {
        if (it->first == def.owner)  // owner was already added
            continue;
        selectedAccnts.push_back(it->first);
    }

    // remember balances in the cache
    std::vector<u256> balances;
    for (uint i = 0; i < accountsCnt; i++) {
        auto epbin = def.getEpBinByName("balanceOf(address)");

        std::vector<u256> parsedParams;  // add address parameter
        parsedParams.push_back(selectedAccnts[i]);

        auto oper = getAccount(def.owner).acc;  // get O's account state
        eevm::PersistantTransaction* tx = this->m_ledger.createCallFunctionTX(m_accounts[def.owner], erc, parsedParams, epbin, oper.get_nonce(), 0);

        if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
            exit(1);

        balances.push_back(output_u256);
        delete tx;
    }

    // execute TXs one by one
    auto start_t = chrono::steady_clock::now();
    for (uint i = 0; i < numberOfTx; i++) {
        auto epbin = def.getEpBinByName("transfer(address,uint256)");

        // select random origin who has some funds at ERC20 contract
        uint j;
        do {
            j = uint(std::rand() % accountsCnt);
        } while (balances[j] == u256(0u));

        auto origin = getAccount(selectedAccnts[j]).acc;
        uint destIdx = std::rand() % accountsCnt;
        auto to = selectedAccnts[destIdx];

        std::vector<u256> parsedParams;
        parsedParams.push_back(to);                       // add receiver of ERC20 tokens
        auto value = get_random_uint256() % balances[j];  // add amount of ERC20 tokens
        parsedParams.push_back(value);

        eevm::PersistantTransaction* tx = this->m_ledger.createCallFunctionTX(m_accounts[origin.get_address()], erc, parsedParams, epbin, origin.get_nonce(), 0);

        if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
            exit(1);

        // adjust balances in our cache
        balances[j] -= value;
        balances[destIdx] += value;
        delete tx;
    }
    auto end_t = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(end_t - start_t).count();

    std::cout << fmt::format("\nElapsed time = {}ms => {} TXs/sec.\n", ms, numberOfTx / (ms / 1000.0));

    // print the final balances
    std::cout << fmt::format("The final balances of ERC {} contract are:\n", to_hex_string(erc));
    for (uint i = 0; i < accountsCnt; i++) {
        std::cout << fmt::format("\t {} => {}\n", address_to_hex_string(selectedAccnts[i]), to_hex_string(balances[i]));
    }
}

/**
 * Repeat and statistically evaluate results of function "_testBulkNativePayments_batched_repeated"
 */
void Operator::_testBulkERC_batched_repeated(oe_enclave_t* enclave, uint numberOfTx, uint accountsCnt, Address erc, uint batchSize, uint repetitions)
{
    vector<double> items;
    double sum = 0.0;
    for (uint i = 0; i < repetitions; i++) {
        std::cout << fmt::format("\t Iteration [{}/{}]\n", i + 1, repetitions);
        auto item = this->_testBulkERC_batched(enclave, numberOfTx, accountsCnt, erc, batchSize);
        items.push_back(item);
        sum += item;

        double mean = sum / items.size();
        double std = stddev(items);
        std::cout << fmt::format("\t After {} repetitions: AVG = {} | STDDEV = {}\n", i, mean, std);
        eevm::print_sep();
    }
}

/**
 * Execute 'numberOfTx' TXs that transfer ERC20 tokens among 'accountsCnt' normal accounts through interaction with address 'erc'.
 * Transactions are batched according to 'batchSize' parameter.
 * Note that function execute additional 'accountsCnt' TXs to get balances of accounts at the begining.
 */
double Operator::_testBulkERC_batched(oe_enclave_t* enclave, uint numberOfTx, uint accountsCnt, Address erc, uint batchSize)
{
    assert(batchSize >= 1);
    u256 output_u256;
    auto def = m_contracts[erc];

    // select accountsCnt accounts, where the 1st one is the owner of contract called
    std::vector<eevm::Address> selectedAccnts;
    std::vector<Account::Nonce> nonces;  // nonces also need tracking alike balances
    selectedAccnts.push_back(def.owner);
    for (auto it = m_accounts.begin(); it != m_accounts.end() && selectedAccnts.size() < accountsCnt; ++it) {
        if (it->first == def.owner)  // owner was already added
            continue;
        selectedAccnts.push_back(it->first);
    }

    // remember balances in the cache
    std::vector<u256> balances;
    for (uint i = 0; i < accountsCnt; i++) {
        auto epbin = def.getEpBinByName("balanceOf(address)");

        std::vector<u256> parsedParams;  // add address parameter
        parsedParams.push_back(selectedAccnts[i]);

        auto oper = getAccount(def.owner).acc;  // get O's account state
        eevm::PersistantTransaction* tx = this->m_ledger.createCallFunctionTX(m_accounts[def.owner], erc, parsedParams, epbin, oper.get_nonce(), 0);

        if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
            exit(1);

        nonces.push_back(oper.get_nonce() + 1);
        balances.push_back(output_u256);
        delete tx;
    }

    // execute TXs in batches
    auto start_t = chrono::steady_clock::now();
    std::vector<eevm::PersistantTransaction*> txs_in_batch;
    for (uint i = 0; i < numberOfTx; i++) {
        auto epbin = def.getEpBinByName("transfer(address,uint256)");

        // select random origin who has some funds at ERC20 contract
        uint j;
        do {
            j = uint(std::rand() % accountsCnt);
        } while (balances[j] == u256(0u));

        uint destIdx = std::rand() % accountsCnt;
        auto to = selectedAccnts[destIdx];

        std::vector<u256> parsedParams;
        parsedParams.push_back(to);                       // add receiver of ERC20 tokens
        auto value = get_random_uint256() % balances[j];  // add amount of ERC20 tokens
        parsedParams.push_back(value);

        eevm::PersistantTransaction* tx = this->m_ledger.createCallFunctionTX(m_accounts[selectedAccnts[j]], erc, parsedParams, epbin, nonces[j], 0);

        // dispatch TXs from batch if the batch is full already
        if (txs_in_batch.size() == batchSize) {
            if (RET_SUCCESS != this->_dispatchManyTXs(enclave, txs_in_batch))
                exit(1);
            txs_in_batch.clear();
        }
        txs_in_batch.push_back(tx);
        m_ledger.m_gs.db()->purge(); // clean up unused entries of database

        // adjust balances in our cache
        balances[j] -= value;
        balances[destIdx] += value;
        nonces[j] += 1;
    }

    // resolve remaining TXs in the last (non-full) batch
    if (txs_in_batch.size() != 0) {
        if (RET_SUCCESS != this->_dispatchManyTXs(enclave, txs_in_batch))
            exit(1);
    }    
    auto end_t = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(end_t - start_t).count();
    m_ledger.m_gs.db()->purge(); // clean up unused entries of database

    double ret = numberOfTx / (ms / 1000.0);
    std::cout << fmt::format("\nElapsed time = {}ms => {} TXs/sec.\n", ms, ret);

    // print the final balances
    std::cout << fmt::format("The final balances of ERC {} contract are:\n", to_hex_string(erc));
    for (uint i = 0; i < accountsCnt; i++) {
        std::cout << fmt::format("\t {} => {}\n", address_to_hex_string(selectedAccnts[i]), to_hex_string(balances[i]));
    }
    return ret;
}

/**
 * Repeat and statistically evaluate results of function "_testBulkNativePayments_batched_repeated"
 */
void Operator::_testBulkNativePayments_batched_repeated(oe_enclave_t* enclave, uint numberOfTx, uint accountsCnt, uint batchSize, uint repetitions)
{
    vector<double> items;
    double sum = 0.0;
    for (uint i = 0; i < repetitions; i++) {
        std::cout << fmt::format("\t Iteration [{}/{}]\n", i + 1, repetitions);
        auto item = this->_testBulkNativePayments_batched(enclave, numberOfTx, accountsCnt, batchSize);
        items.push_back(item);
        sum += item;

        double mean = sum / items.size();
        double std = stddev(items);
        std::cout << fmt::format("\t After {} repetitions: AVG = {} | STDDEV = {}\n", i, mean, std);
        eevm::print_sep();
    }
}

/**
 * Execute 'numberOfTx' native payment TXs that transfer native tokens among 'accountsCnt' normal accounts.
 * It batches TXs before passing it to Enclave to batch of size 'batchSize'.
 */
double Operator::_testBulkNativePayments_batched(oe_enclave_t* enclave, uint numberOfTx, uint accountsCnt, uint batchSize)
{
    assert(batchSize >= 1);

    // select accountsCnt accounts, where the 1st one is the owner
    std::vector<eevm::Address> selectedAccnts;
    std::vector<u256> balances;          // current balances need tracking since we batch and do not update account states immediatelly
    std::vector<Account::Nonce> nonces;  // nonces also need tracking alike balances
    for (auto it = m_accounts.begin(); it != m_accounts.end() && selectedAccnts.size() < accountsCnt; ++it) {
        selectedAccnts.push_back(it->first);

        SimpleAccountState as = getAccount(it->first);
        uint256_t balance = as.acc.get_balance();
        balances.push_back(balance);
        nonces.push_back(as.acc.get_nonce());
    }

    // execute TXs in batches of size batchSize
    std::vector<eevm::PersistantTransaction*> txs_in_batch;
    double sum_time = 0;
    for (uint i = 0; i < numberOfTx; i++) {
        // select random origin who has some funds
        uint j;
        Account::Nonce origin_nonce;
        uint256_t origin_balance;
        do {
            j = uint(std::rand() % accountsCnt);
            origin_balance = balances[j];
            origin_nonce = nonces[j];
        } while (origin_balance == u256(0u));

        // select random destination
        uint destIdx = std::rand() % accountsCnt;
        uint64_t value = static_cast<uint64_t>(get_random_uint256() % origin_balance);  // add value of native token

        eevm::Code emptyFunc = {0u};
        eevm::PersistantTransaction* tx = this->m_ledger.createCallFunctionTX(
            m_accounts[selectedAccnts[j]], selectedAccnts[destIdx], {}, emptyFunc, origin_nonce, value);

        // dispatch TXs from batch if the batch is full already
        if (txs_in_batch.size() == batchSize) {
            auto start_t = chrono::steady_clock::now();
            if (RET_SUCCESS != this->_dispatchManyTXs(enclave, txs_in_batch))
                exit(1);
            auto end_t = chrono::steady_clock::now();
            sum_time += chrono::duration_cast<chrono::milliseconds>(end_t - start_t).count();
            txs_in_batch.clear();
        }
        txs_in_batch.push_back(tx);
        m_ledger.m_gs.db()->purge(); // clean up unused entries of database

        // adjust balances and nonces in our cache
        balances[j] -= value;
        balances[destIdx] += value;
        nonces[j] += 1;
    }

    // resolve remaining TXs in the last (non-full) batch
    if (txs_in_batch.size() != 0) {
        auto start_t = chrono::steady_clock::now();
        if (RET_SUCCESS != this->_dispatchManyTXs(enclave, txs_in_batch))
            exit(1);
        auto end_t = chrono::steady_clock::now();
        sum_time += chrono::duration_cast<chrono::milliseconds>(end_t - start_t).count();
    }
    m_ledger.m_gs.db()->purge(); // clean up unused entries of database

    double ret = numberOfTx / (sum_time / 1000.0);
    std::cout << fmt::format("\nElapsed time = {}ms => {} TXs/sec.\n", sum_time, ret);

    // print the final balances
    std::cout << fmt::format("The final balances are:\n");
    for (uint i = 0; i < accountsCnt; i++) {
        SimpleAccountState as = getAccount(selectedAccnts[i]);
        std::cout << fmt::format("\t {} => {}\n", address_to_hex_string(selectedAccnts[i]), (uint64_t)as.acc.get_balance());
    }
    return ret;
}

/**
 * Execute 'numberOfTx' native payment TXs that transfer native tokens among 'accountsCnt' normal accounts.
 */
void Operator::_testBulkNativePayments_1by1(oe_enclave_t* enclave, uint numberOfTx, uint accountsCnt)
{
    u256 output_u256;

    // select accountsCnt accounts, where the 1st one is the owner
    std::vector<eevm::Address> selectedAccnts;
    for (auto it = m_accounts.begin(); it != m_accounts.end() && selectedAccnts.size() < accountsCnt; ++it) {
        selectedAccnts.push_back(it->first);
    }

    // execute TXs one by one
    auto start_t = chrono::steady_clock::now();
    for (uint i = 0; i < numberOfTx; i++) {
        // select random origin who has some funds
        uint j;
        Account::Nonce origin_nonce;
        uint256_t origin_balance;
        do {
            j = uint(std::rand() % accountsCnt);
            SimpleAccountState origin_as = getAccount(selectedAccnts[j]);
            origin_balance = origin_as.acc.get_balance();
            origin_nonce = origin_as.acc.get_nonce();
        } while (origin_balance == u256(0u));

        // select random destination
        uint destIdx = std::rand() % accountsCnt;

        uint64_t value = static_cast<uint64_t>(get_random_uint256() % origin_balance);  // add value of native token

        eevm::Code emptyFunc = {0u};
        eevm::PersistantTransaction* tx = this->m_ledger.createCallFunctionTX(
            m_accounts[selectedAccnts[j]], selectedAccnts[destIdx], {}, emptyFunc, origin_nonce, value);

        if (RET_SUCCESS != this->_dispatchTX(enclave, tx, output_u256))
            exit(1);

        delete tx;
    }
    auto end_t = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(end_t - start_t).count();

    std::cout << fmt::format("\nElapsed time = {}ms => {} TXs/sec.\n", ms, numberOfTx / (ms / 1000.0));

    // print the final balances
    std::cout << fmt::format("The final balances are:\n");
    for (uint i = 0; i < accountsCnt; i++) {
        SimpleAccountState as = getAccount(selectedAccnts[i]);
        std::cout << fmt::format("\t {} => {}\n", address_to_hex_string(selectedAccnts[i]), (uint64_t)as.acc.get_balance());
    }
}

/**
 * It creates O's account state in E.
 */
void Operator::_createMyAccntState(oe_enclave_t* enclave)
{
    std::cout << "Creating account of Operator...\n";
    auto* tx = this->m_ledger.createNewAccountTX(this->PK_O, this->SK_O, this->getOperAddr(), 100, 0);

    std::vector<eevm::PersistantTransaction*> txs_in_batch;
    txs_in_batch.push_back(tx);

    // u256 output_u256;
    if (RET_SUCCESS != this->_dispatchManyTXs(enclave, txs_in_batch)) {
        delete tx;
        exit(1);
    }

    auto operAccnt = this->getAccount(this->getOperAddr());  // get the updated account state of O
    info_print(fmt::format("created operator's account: {} ", operAccnt.acc.toString()));

    m_accounts[this->getOperAddr()] = OperAccount(this->SK_O, &this->PK_O, this->getOperAddr());
    eevm::print_sep();
    delete tx;
}

/**
 * Create N simple accounts with initial balance set to 'initBalance'
 * For each account creation, do ecall into E.
 * Return the last created account;
 */
Address Operator::_createNRandomAccounts(unsigned N, unsigned initBalance, oe_enclave_t* enclave)
{
    std::cout << fmt::format("\nCreating {} random accounts by O with initial balance {}\n", N, initBalance);
    auto operAccnt = this->getAccount(this->getOperAddr()).acc;  // already deployed  O's account
    size_t nonceBefore = operAccnt.get_nonce();
    u256 output_u256;

    OperAccount acc;
    for (unsigned i = 0; i < N; i++) {
        std::cout << fmt::format("\n [{}] Creating next operator's testing account...\n", i);


        // 1) generate SK of account
        if (1 != RAND_priv_bytes((unsigned char*)&acc.SK, ECC_SK_SIZE)) {
            unsigned long err = ERR_get_error();
            throw std::logic_error(fmt::format("RAND_pseudo_bytes failed, err = {}", err));
        }

        // 2) compute PK of account
        if (1 != secp256k1_ec_pubkey_create(ECC::s_ctx, &acc.PK, (const uint8_t*)&acc.SK))
            throw std::logic_error("secp256k1_ec_pubkey_create failed");

        acc.addr = eevm::from_big_endian(acc.PK.data, PB_ADDR_SIZE);

        // 3) dispatch TX into E
        std::vector<eevm::PersistantTransaction*> txs_in_batch;
        auto* tx = this->m_ledger.createNewAccountTX(this->PK_O, this->SK_O, acc.addr, initBalance, operAccnt.get_nonce());
        txs_in_batch.push_back(tx);

        if (RET_SUCCESS != this->_dispatchManyTXs(enclave, txs_in_batch)) {
            delete txs_in_batch[0];
            throw std::logic_error("error when dispatching TX");
        }

        eevm::AccountState accntState = this->m_ledger.m_gs.get(acc.addr);
        TRACE_HOST("%s", fmt::format("created account: {} ", accntState.acc.toString()).c_str());
        operAccnt = this->getAccount(this->getOperAddr()).acc;  // get the updated account state of O

        m_accounts[acc.addr] = acc;
        delete txs_in_batch[0];
    }
    assert(nonceBefore + N == operAccnt.get_nonce());
    return acc.addr;
}

/**
 * The point of interaction with the Enclave. Store the first 32B of the result into 'output_u256'
 * Processes just a single TX.
 */
int Operator::_dispatchTX(oe_enclave_t* enclave, eevm::PersistantTransaction* tx, uint256_t& output_u256)
{
    int ret;

    switch (this->m_ledger.m_mode) {
        case AQLedger::MODE::FullStateTransfer:
            ret = _dispatchTX_FullState(enclave, tx, output_u256);
            break;
        case AQLedger::MODE::FullStateMaintained:
            std::cerr << "FullStateMaintained mode is not supported for single TX execution.\n";            
            ret = 1;
            break;
        case AQLedger::MODE::PartialStateTransfer:
            ret = _dispatchTX_PartialState(enclave, tx, output_u256);
            break;
        default:
            std::cerr << "Unknown mode: " << static_cast<int>(this->m_ledger.m_mode) << "\n";
            exit(1);
    }
    return ret;
}

/**
 * The point of interaction with the Enclave. Store the first 32B of the result into 'output_u256'
 * Processes batched TXs.
 */
int Operator::_dispatchManyTXs(oe_enclave_t* enclave, std::vector<eevm::PersistantTransaction*>& txs_in_batch)
{
    int ret;
    switch (this->m_ledger.m_mode) {
        case AQLedger::MODE::PartialStateTransfer:
            ret = _dispatchManyTXs_PartialState(enclave, txs_in_batch);
            break;
        case AQLedger::MODE::FullStateMaintained:
            ret = _dispatchManyTXs_FullStateMaintained(enclave, txs_in_batch);
            break;            
        default:
            std::cerr << "Unsupported mode: " << static_cast<int>(this->m_ledger.m_mode) << "\n";
            exit(1);
    }
    return ret;
}

/**
 * Executes many TXs in Enclave, while it does not transfer the MP3 to enclave at all (i.e., enclave stores a full MP3 state)
 */
int Operator::_dispatchManyTXs_FullStateMaintained(oe_enclave_t* enclave, std::vector<eevm::PersistantTransaction*>& txs_in_batch)
{
    int ret;

    std::vector<uint8_t> txs_persistant;  // data of all TXs in batch (except their codes)
    size_t txs_persistant_size = 0;       // size of the previous vector

    std::vector<uint8_t> codes;       // data codes
    std::vector<size_t> codes_sizes;  // vectorized sizes of the codes
    size_t codes_sizes_size = 0;      // size of the previous vector

    // 1) Copy data of TXs and their code // maybe we can somehow optimize and do not copy already existing data??
    for (auto tx : txs_in_batch) {       
        // copy data of a current TX and its code
        PersistantTxProxy_T* ptx = (PersistantTxProxy_T*)tx;
        txs_persistant.insert(txs_persistant.end(), (uint8_t*)ptx, (uint8_t*)ptx + sizeof(PersistantTxProxy_T));
        codes.insert(codes.end(), tx->code.begin(), tx->code.end());
        codes_sizes.push_back(tx->code.size());
    }
    txs_persistant_size = txs_in_batch.size() * sizeof(PersistantTxProxy_T);
    codes_sizes_size = codes_sizes.size() * sizeof(size_t);    
    assert(txs_persistant.size() == txs_persistant_size);

    // 2) Execute TXs in Host one by one        
    for (auto& tx : txs_in_batch) {
        uint256_t output_u256;
        ret = this->m_ledger.executeTX(tx, output_u256);
        if (ret != RET_SUCCESS) {  // this updates global account state in the host
            error_print("Error when executing TX in HOST.");
            return ret;
        }
    }    

     // 3) Execute all TXs from batch in Enclave
    oe_result_t ecall_ret = ecall_run_many_txs_maintained_full_mp3state(enclave, &ret,
                                                (const uint8_t*)txs_persistant.data(), txs_persistant_size,
                                                (const uint8_t*)codes.data(), sumVectST(codes_sizes), codes_sizes.data(), codes_sizes_size);                                                                

    if (ecall_ret != OE_OK || is_error(ret)) {
        error_print("Error when executing batch of TXs in ENCLAVE.");
        return ret;
    }

    // 4) Fetch the updated global state of E
    PublicSealedData_T pub_evm_state;
    ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
    if (ecall_ret != OE_OK && is_error(ret)) {
        error_print("Failed to read the state of enclave.");
        return ret;
    }

    // 5) Compare E's state to host's state
    assert(eevm::from_big_endian(pub_evm_state.globStRoot) == this->m_ledger.m_gs.root());
    info_print(">> State in Host and Enclave match! <<");
    return RET_SUCCESS;
}

/**
 * Executes many TXs in Enclave, while it transfers only a partial MP3 state to Enclave. [fast]
 */
int Operator::_dispatchManyTXs_PartialState(oe_enclave_t* enclave, std::vector<eevm::PersistantTransaction*>& txs_in_batch)
{
    int ret;

    // 1) Dump partial global state (i.e., MP3 DB entries). Note that storages are dumped as full entries.
    std::set<h256> db_keys;         // this is a temporary list of all keys (i.e., hashes of RLP) in exported partial DB, which should avoid duplicity in 'db_data' vector
    std::set<eevm::Address> addrs;  // addresses whose trails in MP3 we need for partial state

    std::vector<uint8_t> txs_persistant;  // data of all TXs in batch (except their codes)
    size_t txs_persistant_size = 0;       // size of the previous vector

    std::vector<uint8_t> codes;       // data codes
    std::vector<size_t> codes_sizes;  // vectorized sizes of the codes
    size_t codes_sizes_size = 0;      // size of the previous vector

    std::vector<uint8_t> db_data;         // all dumped DB entries will be stored here as consecutive RLPs (sizes are encoded in RLP)
    std::vector<uint8_t> storages;        // \/== storages of all accounts
    std::vector<uint8_t> acnts_storages;  // \/== addresses of accounts related to dumped storages
    std::vector<size_t> storages_sizes;   // vectorized sizes of the storages
    size_t storages_sizes_size = 0;       // size of the previous vector

    for (auto tx : txs_in_batch) {
        // if 'origin' and 'to' exist then we need their trails in MP3
        if (m_ledger.m_gs.exists(tx->to)) {
            addrs.insert(tx->to);
            // auto as = m_ledger.m_gs.get(tx->to);
            // TRACE_HOST("Hash value of storage before dumping partial DB in account: %s and computed in storage: %s.", to_hex_string(as.acc.get_stHash()).c_str(), to_hex_string(as.st.hash()).c_str());
        }
        if (m_ledger.m_gs.exists(tx->origin)) {
            addrs.insert(tx->origin);
        }

        // copy data of a current TX and its code
        PersistantTxProxy_T* ptx = (PersistantTxProxy_T*)tx;
        txs_persistant.insert(txs_persistant.end(), (uint8_t*)ptx, (uint8_t*)ptx + sizeof(PersistantTxProxy_T));
        codes.insert(codes.end(), tx->code.begin(), tx->code.end());
        codes_sizes.push_back(tx->code.size());
    }
    txs_persistant_size = txs_in_batch.size() * sizeof(PersistantTxProxy_T);
    codes_sizes_size = codes_sizes.size() * sizeof(size_t);
    m_ledger.m_gs.dump_partial_db(addrs, db_data, db_keys, storages, storages_sizes, storages_sizes_size, acnts_storages);
    assert(txs_persistant.size() == txs_persistant_size);

    // store root
    h256 root_orig = m_ledger.m_gs.root();

    // 2) Execute TXs in Host one by one (and log all newly created accounts and their trails)
    std::vector<uint8_t> db_data_aux;  // these are auxiliary DB data that are needed (on top of account trails) when inserting new accounts
    m_ledger.m_gs.startLookupLogging(&db_keys, &db_data_aux);

    for (auto& tx : txs_in_batch) {
        uint256_t output_u256;
        ret = this->m_ledger.executeTX(tx, output_u256);
        if (ret != RET_SUCCESS) {  // this updates global account state in the host
            error_print("Error when executing TX in HOST.");
            return ret;
        }
    }
    unsigned cntLookups = m_ledger.m_gs.finishLookupLogging();
    info_print(fmt::format("The number of auxiliary entries fetched from DB is {}.", cntLookups));

    info_print(fmt::format("Size of state passed to E: (accounts = {}B + {}B Aux | storages = {}B); SUM = {}B",
                           db_data.size(), db_data_aux.size(), sumVectST(storages_sizes), db_data.size() + db_data_aux.size() + sumVectST(storages_sizes)));
    info_print(fmt::format("Size of code passed to E is {}", sumVectST(codes_sizes)));


    // 3) Execute all TXs from batch in Enclave
    oe_result_t ecall_ret = ecall_run_many_txs_mp3state_partial(enclave, &ret,
                                                                (const uint8_t*)txs_persistant.data(), txs_persistant_size,
                                                                (const uint8_t*)codes.data(), sumVectST(codes_sizes), codes_sizes.data(), codes_sizes_size,
                                                                (const uint8_t*)root_orig.data(), 32u,
                                                                (const uint8_t*)db_data.data(), db_data.size(),
                                                                (const uint8_t*)db_data_aux.data(), db_data_aux.size(),
                                                                (const uint8_t*)storages.data(), storages_sizes.data(),
                                                                storages_sizes_size, (const uint8_t*)acnts_storages.data());

    if (ecall_ret != OE_OK || is_error(ret)) {
        error_print("Error when executing batch of TXs in ENCLAVE.");
        return ret;
    }

    // 5) Fetch the updated global state of E
    PublicSealedData_T pub_evm_state;
    ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
    if (ecall_ret != OE_OK && is_error(ret)) {
        error_print("Failed to read the state of enclave.");
        return ret;
    }

    // 6) Compare E's state to host's state
    assert(eevm::from_big_endian(pub_evm_state.globStRoot) == this->m_ledger.m_gs.root());
    info_print(">> State in Host and Enclave match! <<");
    return RET_SUCCESS;
}


/**
 * Executes one TX in enclave, while it dumps only partial MP3 state to enclave. [fast]
 */
int Operator::_dispatchTX_PartialState(oe_enclave_t* enclave, eevm::PersistantTransaction* tx, uint256_t& output_u256)
{
    int ret;

    // 1) Dump partial global state (i.e., MP3 DB entries). Note that storages are dumped as full entries.
    std::set<h256> db_keys;         // this a temporary list of all keys (i.e., hashes of RLP) in exported partial DB, which should avoid duplicity in 'data' vector
    std::set<eevm::Address> addrs;  // addresses whose trails in MP3 we need for partial state

    // if 'origin' and 'to' exist then we need their trails in MP3
    if (m_ledger.m_gs.exists(tx->to)) {
        addrs.insert(tx->to);
        auto as = m_ledger.m_gs.get(tx->to);
        // TRACE_HOST("Hash value of storage before dumping partial DB is in account: %s and computed in storage: %s.", to_hex_string(as.acc.get_stHash()).c_str(), to_hex_string(as.st.hash()).c_str());
    }
    if (m_ledger.m_gs.exists(tx->origin)) {
        addrs.insert(tx->origin);
    }
    std::vector<uint8_t> db_data;         // all dumped DB entries will be stored here as consecutive RLPs (sizes are encoded in RLP)
    std::vector<uint8_t> storages;        // \/== storages of all accounts
    std::vector<uint8_t> acnts_storages;  // \/== addresses of accounts related to dumped storages
    std::vector<size_t> storages_sizes;
    size_t storages_sizes_size = 0;
    m_ledger.m_gs.dump_partial_db(addrs, db_data, db_keys, storages, storages_sizes, storages_sizes_size, acnts_storages);

    // store root
    h256 root_orig = m_ledger.m_gs.root();

    // 2) Execute TX in Host    (and log all newly created accounts and their trails)
    std::vector<uint8_t> db_data_aux;  // these are auxiliary DB data that are needed (on top of account trails) when inserting new accounts
    m_ledger.m_gs.startLookupLogging(&db_keys, &db_data_aux);
    ret = this->m_ledger.executeTX(tx, output_u256);
    unsigned cntLookups = m_ledger.m_gs.finishLookupLogging();
    info_print(fmt::format("The number of auxiliary entries fetched from DB is {}.", cntLookups));
    if (ret != RET_SUCCESS) {  // this updates global account state in the host
        error_print("Error when executing TX in HOST.");
        return ret;
    }

    info_print(fmt::format("Size of state passed to E: (accounts = {}B + {}B Aux | storages = {}B); SUM = {}B",
                           db_data.size(), db_data_aux.size(), sumVectST(storages_sizes), db_data.size() + db_data_aux.size() + sumVectST(storages_sizes)));
    info_print(fmt::format("Size of code passed to E is {}", tx->code.size()));

    // 3) Execute TX in Enclave
    oe_result_t ecall_ret = ecall_run_single_tx_mp3state_partial(enclave, &ret,
                                                                 (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                                 (const uint8_t*)tx->code.data(), tx->code.size(),
                                                                 (const uint8_t*)root_orig.data(), 32u,
                                                                 (const uint8_t*)db_data.data(), db_data.size(),
                                                                 (const uint8_t*)db_data_aux.data(), db_data_aux.size(),
                                                                 (const uint8_t*)storages.data(), storages_sizes.data(),
                                                                 storages_sizes_size, (const uint8_t*)acnts_storages.data());

    if (ecall_ret != OE_OK || is_error(ret)) {
        error_print("Error when executing TX in ENCLAVE.");
        return ret;
    }

    // 5) Fetch the updated global state of E
    PublicSealedData_T pub_evm_state;
    ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
    if (ecall_ret != OE_OK && is_error(ret)) {
        error_print("Failed to read the state of enclave.");
        return ret;
    }

    // 6) Compare E's state to host's state
    assert(eevm::from_big_endian(pub_evm_state.globStRoot) == this->m_ledger.m_gs.root());
    info_print(">> State in Host and Enclave match! <<");
    return RET_SUCCESS;
}

/**
 * Executes one TX in enclave, while it dumps the full MP3 state to enclave. [slow when MP3 is big]
 */
int Operator::_dispatchTX_FullState(oe_enclave_t* enclave, eevm::PersistantTransaction* tx, uint256_t& output_u256)
{
    int ret;

    // 1) Dump global MP3 state into basic C types (to be passed into enclave)
    std::vector<uint8_t> db_keys;  // \/== global account state
    std::vector<uint8_t> db_values;
    std::vector<size_t> values_sizes;
    size_t db_keys_size, values_sizes_size;
    std::vector<uint8_t> storages;  // \/== storages of all accounts
    std::vector<size_t> storages_sizes;
    size_t storages_sizes_size;
    m_ledger.m_gs.dump_full_db(db_keys, db_values, values_sizes, db_keys_size, values_sizes_size, storages, storages_sizes, storages_sizes_size);

    info_print(fmt::format("Size of state passed to E: (accounts = {}B + {}B | storages = {}B)", db_keys_size, sumVectST(values_sizes), sumVectST(storages_sizes)));
    info_print(fmt::format("Size of code passed to E is {}", tx->code.size()));
    // debug_print(fmt::format("Code passed to E is {}", to_hex_string(tx->code)));

    // 2) Execute TX in Host
    ret = this->m_ledger.executeTX(tx, output_u256);
    if (ret != RET_SUCCESS) {  // this updates global account state in the host
        error_print("Error when executing TX in HOST.");
        return ret;
    }

    // 3) Execute TX in Enclave
    oe_result_t ecall_ret = ecall_run_single_tx_mp3state_full(enclave, &ret,
                                                              (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                                              (const uint8_t*)tx->code.data(), tx->code.size(),
                                                              (const uint8_t*)db_keys.data(), db_keys_size,
                                                              (const uint8_t*)db_values.data(), values_sizes.data(), values_sizes_size,
                                                              (const uint8_t*)storages.data(), storages_sizes.data(), storages_sizes_size);

    if (ecall_ret != OE_OK || is_error(ret)) {
        error_print("Error when executing TX in ENCLAVE.");
        return ret;
    }

    // 4) Fetch the updated global state of E
    PublicSealedData_T pub_evm_state;
    ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
    if (ecall_ret != OE_OK && is_error(ret)) {
        error_print("Failed to read the state of enclave.");
        return ret;
    }

    // 5) Compare E's state to host's state
    assert(eevm::from_big_endian(pub_evm_state.globStRoot) == this->m_ledger.m_gs.root());
    info_print(">> State in Host and Enclave match! <<");
    return RET_SUCCESS;
}
