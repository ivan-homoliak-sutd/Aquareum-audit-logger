#pragma once

#include "common.h"
#include "eEVM/transaction.h"
#include "secp256k1.h"

class ECLedger{

  public:
	ECLedger();
	eevm::PersistantTransaction * createHelloWorldTX(secp256k1_pubkey & PK_sender, uint8_t (& SK_sender) [ECC_SK_SIZE], secp256k1_context & ctx);
};
