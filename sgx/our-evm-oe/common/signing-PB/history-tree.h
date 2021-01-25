#pragma once

#include "aleth-mp3/FixedHash.h"
#include "eEVM/constants.h"
#include "eEVM/util.h"
#include "merkle-tree.h"

class HistoryTree {
public:
    inline HistoryTree()
    {
        items = 0;
        root = EMPTY_HASH_OBJ;
    }

    inline void add(const eevm::KeccakHash& a)
    {
        cache.push_back(a);
        items++;

        HashesArray new_;
        for (size_t i = 0; i < cache.size(); ++i) {
            dev::h256 c(reinterpret_cast<const uint8_t*>(cache.data() + i * HASH_SIZE), dev::h256::ConstructFromPointer);
            new_.push_back(c);
        }

        for (int i = cache.size() - 2; i >= 0; --i) {
            eevm::keccak_256(new_.data() + i * HASH_SIZE, 2 * HASH_SIZE, new_.data() + i * HASH_SIZE);
        }

        root = dev::h256(new_.data(), dev::h256::ConstructFromPointer);
        updateCache();
    }

    inline dev::h256 getRoot()
    {
        return root;
    }


private:
    HashesArray cache;
    int items;
    dev::h256 root;

    inline void updateCache()
    {
        int log = floor(log2(items));
    	for (int i=2; i<=pow(2,log); i*=2) {
    		if (items%i == 0) {
                if (cache.size() > 1) {
                    eevm::keccak_256(cache.data() + (cache.size() - 2) * HASH_SIZE, 2 * HASH_SIZE, cache.data() + (cache.size() - 2) * HASH_SIZE);
    			    cache.pop_back();
                }
    		}
    	}
    }
};
