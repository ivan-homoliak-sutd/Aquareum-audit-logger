#pragma once

#include "aleth-mp3/FixedHash.h"
#include "merkle-tree.h"

class FHPositionNode {        
public:
    FHPositionNode(FHPositionNode && other) = default;
    FHPositionNode(const FHPositionNode & other) = default;
    FHPositionNode& operator=(const FHPositionNode& other) = default; // copy operator
    FHPositionNode& operator=(FHPositionNode&& other) = default; // move operator
    
    unsigned int idxL;      // index of layer of the node from the bottom (starting by 0)
    unsigned long int idxE; // index of node within the layer (starting by 0)

    inline std::string str(){ std::stringstream ss; ss << " [" << idxL << "," << idxE << "]"; return ss.str(); }    
};

class HistoryTreeEnc {
public:
            
    HistoryTreeEnc()
        : m_itemsCnt(0), m_root(EMPTY_HASH_OBJ)
    {}

    void add(const eevm::KeccakHash& a, bool recomputeRoot = true);        

    inline virtual const dev::h256 & getRoot(){ return m_root; } // the root hash - note it is valid only after calling computeRootFromFH()        

    inline const size_t treeHeight() const { return ceil(log2(m_itemsCnt)) + 1; } // includes also stub nodes (if any)

    inline size_t getCurVersion() { return m_itemsCnt; }

    void printFHCache(); 

    void printElements(); 

    inline FHPositionNode & getFHPositionNodeRef(int idx) {
        assert(m_FH_pos.size() >= (size_t) abs(idx)); // range check
        idx = (idx < 0 )? m_FH_pos.size() + idx : idx; 
        return m_FH_pos[idx];
    }    

private:
    const dev::h256 & computeRootFromFHs(); // store root into m_root and returs its reference
    void _updateFHCache(); // reduces FHCache (called after adding a new item)

protected: 
    HashesArray m_FH_cache; // cache of full hashes (used mostly by E)
    std::vector<FHPositionNode> m_FH_pos; // positions of FH nodes within the tree (it corresponds to the above array)    
    size_t m_itemsCnt;      // the number of items in the history tree    
    dev::h256 m_root; 
};


class HistoryTreeHost: public HistoryTreeEnc {
public:
    
    enum ReduceType { FULL, PARTIAL };

    HistoryTreeHost(ReduceType r = PARTIAL)
        :HistoryTreeEnc(), m_layers(1), m_reduceType(r)  // the first layer will store elements
    {}

    void add(const eevm::KeccakHash& a); 

    int buildIncProof(const unsigned long int versionA, const  unsigned long int versionB, std::vector<dev::h256> & proofFHs, std::vector<FHPositionNode> & proofFHPos);   

    inline HashesArray & getElements() { return m_layers[0]; }  // excluding stub (if any) 
    
    inline size_t getSizeElements() { return m_layers[0].size(); } // excluding stub (if any)

    inline size_t getHeight() {return m_layers.size(); }

    inline const dev::h256 & getRoot() override { m_root = m_layers[m_layers.size() - 1].at(0); return m_root; }

    inline dev::h256 && getNode(int idxLayer, unsigned long int idxElem) {
        assert(m_layers[idxLayer].size() >= (size_t) abs(idxElem)); // range check
        idxElem = (idxElem < 0 )? m_layers[idxLayer].size() + idxElem : idxElem; 
        return m_layers[idxLayer].at(idxElem);
    }

    inline const uint8_t * getNodeData(int idxLayer, unsigned long int idxElem) {
        assert(m_layers[idxLayer].size() >= (size_t) abs(idxElem)); // range check
        idxElem = (idxElem < 0 )? m_layers[idxLayer].size() + idxElem : idxElem; 
        return m_layers[idxLayer].dataAt(idxElem);
    }

    inline const std::vector<HashesArray> & getLayers() { return m_layers; }

    inline void loadTree() { throw std::logic_error("Not implememented."); } // enable to load data from disk quickly (not by add(), which is slow)
    
    void printLayers();
    void printElements();
    void printIncProof(const unsigned long int versionA, const  unsigned long int versionB, 
                std::vector<dev::h256> & proofFHs, std::vector<FHPositionNode> & proofFHPos);

private:        
    void _updateLayersAndRoot();
    
    inline void _reduceSingleLayer(size_t idxL){ // wrapper for the following two          
        if(FULL == m_reduceType){
            _fullReduceSingleLayer(idxL);   
        } else if(PARTIAL == m_reduceType){
            _partialReduceSingleLayer(idxL);   
        } else
            throw std::logic_error("Unknown reduction mode of a layer.");        
    }
    void _fullReduceSingleLayer(int idxL); // it passes the full layer and reduces it to the next one (not optimal)
    void _partialReduceSingleLayer(int idxL); // it passes reduces only changed parts of the layer (optimal)
        
    std::vector<HashesArray> m_layers; // cached layers of non-terminal nodes of the tree - they serve for fast provision of proofs to C
    ReduceType m_reduceType;
};