#include "enclave_t.h"
#include "string.h"

#include "enclave.h"
#include "data_types.h"

#include "sgx_trts.h"
#include "sgx_tseal.h"
#include "sgx_tcrypto.h"

// #include "secp256k1.h"
// #include "scalar_4x64.h"
#include"signing-PB/signing.h"

SealedEvmState_T _evm_state;
bool _evm_initialized = false;


/*
* This function is called only once - when sealed file does not exist.
* The initialization of SK and PK under the signature scheme of the blockchain is performed here.
*/
int ecall_initialize_evm(void){

	sgx_status_t ocall_status, sealing_status;
	int ocall_ret;

	//check whether sealed state does not exist; if yes, then just init from it
	ocall_status = ocall_does_sealed_state_exist(&ocall_ret);
	if(ocall_status != SGX_SUCCESS){
		return ERR_STAT_FILE_INIT;
	} else if(0 == ocall_ret){ 		// EVM state file exists, so initialize from it

		// load sealed file with EVM state
		size_t sealed_size = sizeof(sgx_sealed_data_t) + sizeof(SealedEvmState_T);
		uint8_t* sealed_data = (uint8_t*)malloc(sealed_size);
		ocall_status = ocall_load_evm_state(&ocall_ret, sealed_data, sealed_size);
		if (ocall_ret != 0 || ocall_status != SGX_SUCCESS) {
			free(sealed_data);
			return ERR_CANNOT_LOAD_WALLET;
		}

		uint32_t plaintext_size = sizeof(SealedEvmState_T);
		SealedEvmState_T* evm_state_unsealed = (SealedEvmState_T*)malloc(plaintext_size);
		ocall_status = sgx_unseal_data((sgx_sealed_data_t*)sealed_data, NULL, NULL, (uint8_t*)evm_state_unsealed, &plaintext_size);
		if (ocall_ret != 0 || ocall_status != SGX_SUCCESS) {
			return ERR_LOAD_EVM_STATE;
		}
		evm_state_unsealed->pub.diskInits++;
		memcpy(&_evm_state, evm_state_unsealed, sizeof(SealedEvmState_T)); // TODO: later do deep copy of err TXs
		_evm_initialized = true;
		return RET_SUCCESS_INIT_LOADED_STATE;

	} else{
		// sealed state file does not exist, so create it

		SealedEvmState_T* evm_state_unsealed = (SealedEvmState_T*)malloc(sizeof(SealedEvmState_T));
		memset(evm_state_unsealed, 0, sizeof(SealedEvmState_T));

		// generate EVM key under sig. scheme of PB and store it to evm state struct
		if(0 != generate_keypair_PB(&evm_state_unsealed->sec.keypair)){
			free(evm_state_unsealed);
			return ERR_KEYPAIR_GEN_FAILED;
		}
		evm_state_unsealed->pub.diskInits = 0; // TODO: change later

		// store EVM state in enclave memory
		memcpy(&_evm_state, evm_state_unsealed, sizeof(SealedEvmState_T)); // TODO: later do deep copy of err TXs

		// seal evm state object
		size_t sealed_size = sizeof(sgx_sealed_data_t) + sizeof(SealedEvmState_T);
		uint8_t* sealed_data = (uint8_t*)malloc(sealed_size);
		sealing_status = sgx_seal_data(0, NULL, sizeof(SealedEvmState_T), (uint8_t*)evm_state_unsealed, sealed_size, (sgx_sealed_data_t*)sealed_data);
		free(evm_state_unsealed);
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
		_evm_initialized = true;
		return RET_SUCCESS_INIT_NEW_STATE;
	}
}

int ecall_sync_evm_sealed_state_to_disk(void){

	// seal invernalt evm state object which is in memory
	size_t sealed_size = sizeof(sgx_sealed_data_t) + sizeof(SealedEvmState_T);
	uint8_t* sealed_data = (uint8_t*)malloc(sealed_size);
	sgx_status_t sealing_status = sgx_seal_data(0, NULL, sizeof(SealedEvmState_T), (uint8_t*)&_evm_state, sealed_size, (sgx_sealed_data_t*)sealed_data);
	if (sealing_status != SGX_SUCCESS) {
		free(sealed_data);
		return ERR_FAIL_SEAL_STATE;
	}


	int ocall_ret;
	sgx_status_t ocall_status = ocall_save_evm_state(&ocall_ret, sealed_data, sealed_size);
	free(sealed_data);
	if (ocall_ret != 0 || ocall_status != SGX_SUCCESS) {
		return ERR_CANNOT_SAVE_EVM_STATE;
	}
	return 0;
}


int ecall_read_pub_state(PublicSealedData *pub_evm_state, size_t pub_state_size){
	(* pub_evm_state) = _evm_state.pub;
	return 0;
}

