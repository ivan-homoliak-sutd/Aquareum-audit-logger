#include "eEVM/util.h"
#include "eEVM/tracing.h"
#include "history-tree.h"

void HistoryTreeHost::add(const eevm::KeccakHash& a)
{        
    // insert new element at the end of the 0th layer (i.e., data hashes layer)
    m_layers[0].push_back(a);    
    m_itemsCnt++;        
    updateLayersAndRoot();
}

/**
 * @brief It updates all (cached) layers of the tree, including root. It inserts temporary stubs, which are removed after its end. 
 * 
 */
void HistoryTreeHost::updateLayersAndRoot(){ 
    
    int newLayerIdx = (getSizeElements() > 1)? ceil(log2(getSizeElements())): 1; // idx of the new layer from bottom of the tree (note that log2(1) needs special handling)    
    int maxLayerIdx = getHeight() - 1;
    if(maxLayerIdx < newLayerIdx){
        m_layers.push_back(HashesArray()); // add a new layer if the depth of the currently extended tree was increased
    }

    // reduce the data layer into other layers (including root)
    bool stubAdded = false;
    for (size_t idxL = 0; idxL < getHeight() - 1; idxL++){                
        
        // a) insert stub element for odd size layers
        if(m_layers[idxL].size() % 2 != 0){
            m_layers[idxL].push_back(EMPTY_HASH_OBJ); // align the elements to even number
            stubAdded = true;
        }
        
        fullReduceLayer(idxL);           

        // b) remove the last stub element of the layer idxL (if any)
        if(stubAdded){
            m_layers[idxL].pop_back();
            stubAdded = false;
        }        
    }            
}

/**
 * @brief It reduces the full previous layer of history tree to the next (above) layer; including root hash (i.e., the highest layer)
 *  TODO: needs to be optimized by skipping of computations that were already done before (using FH cache).
 * 
 * @param idxL - index of the current layer to be reduced 
 */
void HistoryTreeHost::fullReduceLayer(int idxL){    
    assert(m_layers[idxL].size() % 2 == 0); // we always have even number of elements in the current layer

    // resize the next layer if needed
    if(m_layers[idxL].size() / 2 > m_layers[idxL + 1].size()){
        m_layers[idxL + 1].resize(m_layers[idxL].size() / 2);
    }

    // store the reduced content of the layer with 'idxL' to the layer with 'idxL' + 1 
    for (size_t i = 0; i < m_layers[idxL].size(); i += 2) {                        
            int idxInHigherLayer = i / 2; // twice slower than the lower layer            
            eevm::keccak_256(m_layers[idxL].dataAt(i), 2 * HASH_SIZE, m_layers[idxL + 1].dataAt(idxInHigherLayer));            
    }       
}

void HistoryTreeHost::printElements(){        
        auto& elems = const_cast<HashesArray &>(getElements());
        std::cout << "Elements are as follows " << "[size: " << elems.size() << "]:" ;
        for(size_t i = 0; i < elems.size(); i++){
            std::cout <<  elems.toHex(i).substr(0, 6)  << ", ";
        }        
        std::cout << "\n";
}

void HistoryTreeHost::printLayers(){                
    
    for(int idxL = getLayers().size() - 1; idxL >= 0; idxL--){
        auto& elems = const_cast<HashesArray &>(getLayers()[idxL]);

        std::cout << "[Layer: " << idxL << "]: ";
        for(size_t i = 0; i < elems.size(); i++){
            std::cout <<  elems.toHex(i).substr(0, 6)  << ", ";
        }        
        std::cout << "\n";
    }                
}
