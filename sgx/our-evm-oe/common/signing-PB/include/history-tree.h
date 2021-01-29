#pragma once

#include "aleth-mp3/FixedHash.h"
#include "merkle-tree.h"

class HistoryTree {
public:
    HistoryTree()
        :m_root(EMPTY_HASH_OBJ)
    {}

    void add(const eevm::KeccakHash& a);    

    void printFHCache(); 

    inline dev::h256 getRoot(){ return m_root; }

private:
    HashesArray m_FH_cache; // cache of full hashes (used mostly by E)
    size_t m_itemsCnt;      // the number of items in the history tree
    dev::h256 m_root;
};
