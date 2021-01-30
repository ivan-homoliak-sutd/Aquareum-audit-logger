#pragma once

#include "aleth-mp3/FixedHash.h"
#include "merkle-tree.h"


typedef struct {        
    unsigned int idxL;      // index of layer of the node from the bottom (starting by 0)
    long unsigned int idxE; // index of node within the layer (starting by 0)

    inline std::string str(){
        std::stringstream ss;
        ss << " [" << idxL << "," << idxE << "]";
        return ss.str();
    }
} FHPositionNode;

class HistoryTreeEnc {
public:
            
    HistoryTreeEnc()
        : m_itemsCnt(0), m_root(EMPTY_HASH_OBJ)
    {}

    void virtual add(const eevm::KeccakHash& a);    

    const dev::h256 & computeRootFromFHs(); // store root into m_root and returs its reference

    inline virtual const dev::h256 & getRoot(){ return m_root; } // the root hash - note it is valid only after calling computeRootFromFH()        

    inline const size_t treeHeight() const { return ceil(log2(m_itemsCnt)) + 1; } // includes also stub nodes (if any)

    void printFHCache(); 

    void printElements(); 

    inline FHPositionNode & getFHPositionNodeRef(int idxElem) {
        assert(m_FH_pos.size() >= (size_t) abs(idxElem)); // range check
        idxElem = (idxElem < 0 )? m_FH_pos.size() + idxElem : idxElem; 
        return m_FH_pos[idxElem];
    }    

protected:
    HashesArray m_FH_cache; // cache of full hashes (used mostly by E)
    std::vector<FHPositionNode> m_FH_pos; // positions of FH nodes within the tree (it corresponds to the above array)    
    size_t m_itemsCnt;      // the number of items in the history tree    

private:
    dev::h256 m_root; 
};

class HistoryTreeHost: public HistoryTreeEnc {
public:
    
    HistoryTreeHost()
        :m_layers(1) // the first layer will store elements
    {}

    void add(const eevm::KeccakHash& a) override;    

    inline const HashesArray & getElements() { return m_layers[0]; }  // excluding stub (if any) 
    
    inline size_t getSizeElements() { return m_layers[0].size(); } // excluding stub (if any)

    inline size_t getHeight() {return m_layers.size(); }

    inline const dev::h256 & getRoot() override {return m_layers[m_layers.size() - 1].at(0); }

    inline dev::h256 && getNode(int idxLayer, int idxElem) {
        assert(m_layers[idxLayer].size() >= (size_t) abs(idxElem)); // range check
        idxElem = (idxElem < 0 )? m_layers[idxLayer].size() + idxElem : idxElem; 
        return m_layers[idxLayer].at(idxElem);
    }

    inline const std::vector<HashesArray> & getLayers() { return m_layers; }
    
    void printLayers();
    void printElements();

private:        
    void updateLayersAndRoot();

    void fullReduceLayer(int idxL); // it passes the full layer and reduces it to the next one (not optimal)
        
    std::vector<HashesArray> m_layers; // cached layers of non-terminal nodes of the tree - they serve for fast provision of proofs to C
};