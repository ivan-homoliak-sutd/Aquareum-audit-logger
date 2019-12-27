#pragma once

#include "data_types.h"
#include "eEVM/simple/simpleglobalstate.h"

class ECLedger {
public:
    ECLedger(){};

    eevm::SimpleGlobalState simple_gs;  // the simple global state that is internal to the enclace (i.e., only tmp/testing object)

    // TODO: drop the following 2 (they are just temporary)
    int execute_hello_world(void);
    int execute_sum_a_b(int a, int b);

    int execute_tx_simplestate_internal(PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size);

    int execute_tx_mp3state_full(PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size,
                                 const uint8_t* db_keys, size_t db_keys_size,
                                 const uint8_t* db_values, const size_t* values_sizes, size_t db_values_sizes_size,
                                 uint8_t* const storages, const size_t* storages_sizes, size_t storages_sizes_size);
};
