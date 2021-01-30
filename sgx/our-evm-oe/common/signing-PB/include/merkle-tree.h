#pragma once

#include "aleth-mp3/FixedHash.h"
#include "eEVM/constants.h"
#include "eEVM/util.h"

#define EMPTY_HASH_OBJ dev::h256("0x0000000000000000000000000000000000000000000000000000000000000000", dev::h256::FromHex)

class HashesArray {
  
  public:
    std::vector<uint8_t> m_data;  // raw data of hashes of the layer in the tree
    size_t m_size = 0;            // the number of items, where each item has 32B

    HashesArray() = default;

    HashesArray(HashesArray const & other) = default; // copy constructor
    //     :m_data(other.m_data), m_size(other.m_size) {}

    inline void push_back(const dev::h256& a){
        m_data.insert(m_data.end(), a.begin(), a.end());
        m_size++;
    }

    inline void push_back(const eevm::KeccakHash& a){
        m_data.insert(m_data.end(), a.begin(), a.end());
        m_size++;
    }

    inline void pop_back(){
        if (m_data.size() >= HASH_SIZE) {  // not empty hash array
            m_data.resize(m_data.size() - HASH_SIZE);
            m_size--;
        }
    }

    inline dev::h256 && at(int idx){ return std::move(dev::h256(m_data.data() + idx * HASH_SIZE, dev::h256::ConstructFromPointer)); }

    inline const size_t size(){ return m_size; }

    inline void resize(size_t newSize){ m_data.resize(newSize * HASH_SIZE); m_size = newSize; }

    inline uint8_t* data(){ return m_data.data(); }
    inline uint8_t* dataAt(size_t idx){ return m_data.data() + idx * HASH_SIZE; }

    inline const std::string toHex(size_t idx){
          assert(idx < size());
          auto ret = dev::h256(m_data.data() + idx * HASH_SIZE, dev::h256::ConstructFromPointer);
          return ret.hex();
    }
};

class MerkleTreeArray {
public:
    
    MerkleTreeArray() = default;
    
    inline void add(dev::h256& a){
        m_hashes.push_back(a);
    }

    inline void add(eevm::KeccakHash& a){
        m_hashes.push_back(a);
    }

    dev::h256 computeRoot();  // uses internal field m_hashes  

private:
    HashesArray m_hashes;
};
