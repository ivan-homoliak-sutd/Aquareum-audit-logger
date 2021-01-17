#pragma once

#include "aleth-mp3/FixedHash.h"
#include "eEVM/constants.h"
#include "eEVM/util.h"

#define EMPTY_HASH_OBJ dev::h256("0x0000000000000000000000000000000000000000000000000000000000000000", dev::h256::FromHex)

typedef struct _HashesArray {
    std::vector<uint8_t> m_data;  // raw data of hashes of the layer in the tree
    size_t m_size = 0;            // the number of items, where each item has 32B

    inline void push_back(const dev::h256& a)
    {
        m_data.insert(m_data.end(), a.begin(), a.end());
        m_size++;
    }

    inline void push_back(const eevm::KeccakHash& a)
    {
        m_data.insert(m_data.end(), a.begin(), a.end());
        m_size++;
    }

    inline size_t size()
    {
        return m_size;
    }

    inline uint8_t* data()
    {
        return m_data.data();
    }
} HashesArray;

class MerkleTreeArray {
public:
    inline MerkleTreeArray(){};

    inline void add(dev::h256& a)
    {
        m_hashes.push_back(a);
    }

    inline void add(eevm::KeccakHash& a)
    {
        m_hashes.push_back(a);
    }

    // uses internal field m_hashes
    inline dev::h256 computeRoot()
    {
        if (m_hashes.size() == 0) {
            return EMPTY_HASH_OBJ;
        }
        while (m_hashes.size() > 1) {
            if (m_hashes.size() & 1) {  // resolve odd arrays of items by appending empty hash object
                m_hashes.push_back(EMPTY_HASH_OBJ);
            }

            // aggregate one layer
            for (size_t i = 0; i < m_hashes.size() / 2; i++) {
                eevm::keccak_256(m_hashes.data() + 2 * i * HASH_SIZE, 2 * HASH_SIZE, m_hashes.data() + i * HASH_SIZE);
            }
            m_hashes.m_size /= 2;  // the second half of the hashes in the layer are not needed anymore
        }
        return dev::h256(m_hashes.data(), dev::h256::ConstructFromPointer);
    }

private:
    HashesArray m_hashes;
};
