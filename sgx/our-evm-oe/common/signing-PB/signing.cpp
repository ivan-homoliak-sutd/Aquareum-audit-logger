#include "signing.h"
#include "common.h"
#include "eEVM/util.h"
// #include "utils.h"

// definition of static context
secp256k1_context* ECC::s_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

int ECC::compute_PK_from_SK(KeyPairPB_T* keypair)  // assumption is that keypair contains already generated SK
{
    if (1 != secp256k1_ec_pubkey_create(m_ctx, &keypair->PK_PB, keypair->SK_PB)) {
        return ERR_PK_GEN_FAILED;
    }
    return RET_SUCCESS;
}


int ECC::sign_hash(secp256k1_ecdsa_recoverable_signature* rsig, const uint8_t* msg32, const uint8_t* SK)
{
    if (VALID_ECC_SIG_RET != secp256k1_ecdsa_sign_recoverable(m_ctx, rsig, msg32, SK, NULL, NULL)) {
        throw std::logic_error("Error when signing hash.");
        // return ERR_ECC_SIGNING;
    }
    return RET_SUCCESS;
}


int ECC::sign_data(const std::vector<uint8_t>& data, const uint8_t* SK, uint8_t* rsig)
{
    eevm::KeccakHash msg32 = eevm::keccak_256(data);
    return this->sign_hash((secp256k1_ecdsa_recoverable_signature*)rsig, msg32.data(), SK);
}


bool ECC::verify_sig(const secp256k1_ecdsa_recoverable_signature* rsig, const uint8_t* msg32, const eevm::Address addr)
{
    secp256k1_ecdsa_signature sig;  // normal signature
    secp256k1_pubkey recpubkey;     // recoverred public key

    if (VALID_ECC_SIG_RET != secp256k1_ecdsa_recoverable_signature_convert(m_ctx, &sig, rsig))
        throw std::logic_error("Conversion of recoverable sig to normal sig failed.");

    if (VALID_ECC_SIG_RET != secp256k1_ecdsa_recover(m_ctx, &recpubkey, rsig, msg32))
        throw std::logic_error("Recovery of PK from rsignature failed.");

    uint8_t usefullAddrData[eevm::ADDR_ETH_SIZE_B];
    eevm::addr_u256_to_eth160b(addr, usefullAddrData);

    // compare 20 Bytes  from recovered address with the passed address
    if (0 == memcmp(usefullAddrData, recpubkey.data + 64 - eevm::ADDR_ETH_SIZE_B, eevm::ADDR_ETH_SIZE_B)) {
        return true;
    }
    return false;
}
