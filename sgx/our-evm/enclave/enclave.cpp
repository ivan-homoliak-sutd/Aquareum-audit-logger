#include "enclave_t.h"
#include "string.h"

#include "enclave.h"
#include "data_types.h"

#include "sgx_trts.h"
#include "sgx_tseal.h"

/*
* This function is called only once - when sealed file does not exist.
* The initialization of SK and PK under the signature scheme of the blockchain is performed here.
*/
int ecall_initialize_evm(void){

	sgx_status_t ocall_status, sealing_status;
	int ocall_ret;

	// create EVM state struct
	SealedEvmState_T* evm_state = (SealedEvmState_T*)malloc(sizeof(SealedEvmState_T));
	memset(evm_state, 0, sizeof(SealedEvmState_T));

	// generate EVM key under sig. scheme of PB and store it to evm state struct
	sgx_status_t rand_status = sgx_read_rand(evm_state->sec.keypair.SK_PB, 32);
	if(SGX_SUCCESS != rand_status){
		free(evm_state);
		return ERR_RAND_FAILED;
	}
	// TODO: compute PK from SK
	// ...


	// seal evm state object
	size_t sealed_size = sizeof(sgx_sealed_data_t) + sizeof(SealedEvmState_T);
	uint8_t* sealed_data = (uint8_t*)malloc(sealed_size);
    sealing_status = sgx_seal_data(0, NULL, sizeof(SealedEvmState_T), (uint8_t*)evm_state, sealed_size, (sgx_sealed_data_t*)sealed_data);
    free(evm_state);
    if (sealing_status != SGX_SUCCESS) {
		free(sealed_data);
		return ERR_FAIL_SEAL_STATE;
    }

	// save sealed evm state to file
	ocall_status = ocall_save_evm_state(&ocall_ret, sealed_data, sealed_size);
	free(sealed_data);
	if (ocall_ret != 0 || ocall_status != SGX_SUCCESS) {
		return ERR_CANNOT_SAVE_EVM_STATE;
	}

	return RET_SUCCESS;
}
