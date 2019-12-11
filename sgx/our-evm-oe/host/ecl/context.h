
#pragma once

#include <vector>

// includes from eEVM
#include "eEVM/transaction.h"
#include "eEVM/address.h"
#include "eEVM/bigint.h"

// default numbers of unprocessed TXs and processed blocks required to flush into VM enclave / PB
#define NUM_TXS_FLUSH_VM 10
#define NUM_BLKS_FLUSH_PB 50

// default flushing timeouts in seconds
#define TIMEOUT_FLUSH_VM 20
#define TIMEOUT_FLUSH_PB 40

namespace ecl {


    struct MP_tree {

    };

    struct Header {
        uint256_t id; // increment-only counter
        uint256_t txs_root; // Merkle root
        uint256_t rcps_root; // Merkle root
        uint256_t st_root; // Merkle-Patricia root
    };

    struct Block {
        Header hdr;
        std::vector<eevm::PersistantTransaction> txs; // transactions
        std::vector<eevm::PersistantTransaction> rcps; // recipes associated with transactions
    };

    struct FlushingLimits {
        uint num_txs_VM = NUM_TXS_FLUSH_VM; // the number of uprocessed txs for flushing to VM enclave
        uint num_blks_PB = NUM_BLKS_FLUSH_PB; // the number of processed blocks for flushing to PB

        uint timeout_VM = TIMEOUT_FLUSH_VM; // timeout for flushing to VM enclave,
        uint timeout_PB = TIMEOUT_FLUSH_PB; // timeout for flushing to PB,
    };

    struct HostContext {
        uint256_t PK_E_TEE;
        uint256_t PK_E_PB;
        uint256_t PK_O; // PK of operator (under Sigma_PB)
        uint256_t SK_O; // SK of operator (under Sigma_PB)

        std::vector<eevm::PersistantTransaction> txs_uprocessed; // cache of unprocessed TXs,
        std::vector<Block> blks_processed; // cache of processed blocks, not synced with PB yet

        uint t_vm; // time of the last flush to VM enclave
        uint t_pb; // time of the last flush to PB

        MP_tree state_cur; // current state of VM (Merkle Patricia tree)

        std::vector<eevm::PersistantTransaction> cens_txs; // cache of posted cens. TXs to smart contract

        std::vector<Block> ledger; // data of Ledger that were synced with PB,

        uint256_t LRoot_PB; // the last root of Ledger flushed to PB,
        uint256_t LRoot_cur; // the current root of (L U blks_processed) not flushed to PB

        FlushingLimits flush_lims; // the flushing limits

    };

}

