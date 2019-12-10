#include "enclave_t.h"
#include "sgx_trts.h"
#include "sgx_tcrypto.h"

#include "signing-PB/signing.h"
#include "data_types.h"
#include "enclave.h"
#include "secp256k1.h"

int generate_keypair_PB(KeyPairPB *keypair){

	// 1) compute SK of PB
	sgx_status_t rand_status = sgx_read_rand(keypair->SK_PB, 32);
	if(SGX_SUCCESS != rand_status){ return ERR_RAND_FAILED; }

	// 2) compute PK of PB
	static secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN); // if sth needs to be verified here, then add also | SECP256K1_CONTEXT_VERIFY
	if( 1 != secp256k1_ec_pubkey_create(ctx, &keypair->PK_PB, keypair->SK_PB)) { return ERR_KEYPAIR_GEN_FAILED; }
	return 0;

}
