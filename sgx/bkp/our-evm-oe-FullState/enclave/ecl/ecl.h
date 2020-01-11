#pragma once

#include "data_types.h"
#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/simple/simpleglobalstate.h"
#include "eEVM/transaction.h"
#include "signing.h"

class ECLedger {
public:
    ECC ecc;  // ECC signing and verification

    eevm::Address operAddr;

    inline ECLedger()
    {
        ecc = ECC();
    }

    // TODO: drop later
    eevm::SimpleGlobalState simple_gs;  // the simple global state that is internal to the enclace (i.e., only tmp/testing object)

    // TODO: drop the following 2 (they are just temporary)
    int execute_hello_world(void);
    int execute_sum_a_b(int a, int b);

    int execute_tx_simplestate_internal(PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size);

    int execute_tx_mp3state_full(eevm::NormalGlobalState* gs, PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size);

private:
    int _execute_transfer_tx(eevm::NormalGlobalState* gs, eevm::Transaction& etx);
};
