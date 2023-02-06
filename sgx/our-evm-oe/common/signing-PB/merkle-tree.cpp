#include "merkle-tree.h"
#include "aleth-mp3/FixedHash.h"
#include "eEVM/constants.h"
#include "eEVM/util.h"
        
// its preserves m_hashes
dev::h256 MerkleTreeArray::computeRoot()
{
    HashesArray tmpHashes(m_hashes);
    
    if (tmpHashes.size() == 0) {
        return EMPTY_HASH_OBJ;
    }
    while (tmpHashes.size() > 1) {
        if (tmpHashes.size() & 1) {  // resolve odd arrays of items by appending empty hash object
            tmpHashes.push_back(EMPTY_HASH_OBJ);
        }

        // aggregate one layer
        for (size_t i = 0; i < tmpHashes.size() / 2; i++) {
            eevm::keccak_256(tmpHashes.data() + 2 * i * HASH_SIZE, 2 * HASH_SIZE, tmpHashes.data() + i * HASH_SIZE);
        }
        tmpHashes.m_size /= 2;  // the second half of the hashes in the layer are not needed anymore
    }
    /// The only valid element in tmpHashes is the root hash - others are just a trash
    return std::move(dev::h256(tmpHashes.data(), dev::h256::ConstructFromPointer));
}