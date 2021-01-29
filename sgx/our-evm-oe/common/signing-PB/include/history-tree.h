#pragma once

#include "aleth-mp3/FixedHash.h"
#include "merkle-tree.h"

class HistoryTree {
public:
            
    HistoryTree()
        :m_root(EMPTY_HASH_OBJ)
    {}

    void virtual add(const eevm::KeccakHash& a);    

    void printFHCache(); 

    void printElements(); 

    inline dev::h256 getRoot(){ return m_root; }

protected:
    HashesArray m_FH_cache; // cache of full hashes (used mostly by E)
    size_t m_itemsCnt;      // the number of items in the history tree
    dev::h256 m_root;
};

class HistoryTreeHost: public HistoryTree {
public:
    
    HistoryTreeHost()
        :m_layers(1) // the first layer will store elements
    {}

    void add(const eevm::KeccakHash& a) override;    

    inline const HashesArray & getElements() { return m_layers[0]; }  // !!! including stub (if any) 
    
    inline size_t getSizeElements() { return m_layers[0].size(); } // !!! including stub (if any)

    inline size_t getHeight() {return m_layers.size(); }

    inline const dev::h256 & getRoot() {return m_layers[m_layers.size() - 1].at(0); }

    inline dev::h256 && getNode(int idxLayer, int idxElem) {
        assert(m_layers[idxLayer].size() >= abs(idxElem)); // range check
        idxElem = (idxElem < 0 )? m_layers[idxLayer].size() + idxElem : idxElem; 
        return m_layers[idxLayer].at(idxElem);
    }

private:        
    void updateLayersAndRoot();

    void fullReduceLayer(int idxL); // it passes the full layer and reduces it to the next one (not optimal)
        
    std::vector<HashesArray> m_layers; // cached layers of non-terminal nodes of the tree - they serve for fast provision of proofs to C
};