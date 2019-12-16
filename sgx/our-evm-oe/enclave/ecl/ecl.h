#pragma once

#include "data_types.h"
#include "eEVM/simple/simpleglobalstate.h"

class ECLedger {

  public:
    ECLedger(){};

    eevm::SimpleGlobalState gs; // the global state of the ECL ledger

    // TODO: drop the following 2 (they are just temporary)
    int execute_hello_world(void);
    int execute_sum_a_b(int a, int b);

    int execute_tx(PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size);
};
