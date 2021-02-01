#include "eEVM/util.h"
#include "eEVM/tracing.h"
#include "history-tree.h"

/**
 * @brief Adds entry to m_layers and updates the full tree in m_layers as well. 
 * Additionally, calls parent method to update FH cache (i.e., current incremental proof)
 * 
 * @param a 
 */
void HistoryTreeHost::add(const eevm::KeccakHash& a)
{        
    // insert new element at the end of the 0th layer (i.e., data hashes layer)
    m_layers[0].push_back(a);    
    _updateLayersAndRoot();
    HistoryTreeEnc::add(a, false); // update FH cache and FH positions by utilizing data of parent history tree (for enclave); skip recomputation of E's m_root
}
    
/**
 * @brief It updates all (cached) layers of the tree, including root. It inserts temporary stubs, which are removed after processing. 
 * 
 */
void HistoryTreeHost::_updateLayersAndRoot(){ 
    
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
        
        _reduceSingleLayer(idxL); // uses default reduction mode

        // b) remove the last stub element of the layer idxL (if any)
        if(stubAdded){
            m_layers[idxL].pop_back();
            stubAdded = false;
        }        
    }            
}

/**
 * @brief In contrast to _fullReduceSingleLayer Optimized by skipping of computations that were already done before (using FH cache).
 * 
 * @param idxL - index of the current layer to be reduced 
 */
void HistoryTreeHost::_partialReduceSingleLayer(int idxL){    
    assert(m_layers[idxL].size() % 2 == 0); // we always have even number of elements in the current layer

    // 1) resize the next layer if needed
    if(m_layers[idxL].size() / 2 > m_layers[idxL + 1].size()){
        m_layers[idxL + 1].resize(m_layers[idxL].size() / 2);
    }

    // 2) store the PARTIAL reduced content of the layer with 'idxL' to the layer with 'idxL' + 1     
    size_t cntNodesInCurL = m_layers[idxL].size() - 1; // -1 to not assume curently added element by add()
    size_t idxInCur =  cntNodesInCurL - (cntNodesInCurL % 2); // skip the (fixed) elements of old tree (-m_item % 2 to get idx on the left side)                                        
    size_t idxInNext = (idxInCur / 2)  - (idxInCur % 2) ;     // / 2 since it is 2 times slower | %2 to get on the left side
    eevm::keccak_256(m_layers[idxL].dataAt(idxInCur), 2 * HASH_SIZE, m_layers[idxL + 1].dataAt(idxInNext));                       
}

/**
 * @brief It reduces the full current layer of history tree into the next (above) layer; including root hash (i.e., the highest layer)
 * It can be used for fast loading of data from disk by function loadTree()
 * 
 * @param idxL - index of the current layer to be reduced 
 */
void HistoryTreeHost::_fullReduceSingleLayer(int idxL){    
    assert(m_layers[idxL].size() % 2 == 0); // we always have even number of elements in the current layer

    // 1) resize the next layer if needed
    if(m_layers[idxL].size() / 2 > m_layers[idxL + 1].size()){
        m_layers[idxL + 1].resize(m_layers[idxL].size() / 2);
    }

    // store the reduced content of the current layer with 'idxL' to the layer with 'idxL' + 1 
    for (size_t i = 0; i < m_layers[idxL].size(); i += 2) {                        
            int idxInHigherLayer = i / 2; // twice slower than the lower layer            
            eevm::keccak_256(m_layers[idxL].dataAt(i), 2 * HASH_SIZE, m_layers[idxL + 1].dataAt(idxInHigherLayer));            
    }       
}

int HistoryTreeHost::buildIncProof(const size_t versionA, const  size_t versionB, std::vector<dev::h256> & proofFHs, std::vector<FHPositionNode> & proofFHPos){
    
    // 0) initial checks & allocation
    size_t curVer = getCurVersion();
    if(versionB < 2 || versionA < 1) { throw std::logic_error("Only inc proofs against the current version are supported."); }    
    if(versionB != curVer){ throw std::logic_error("Only inc proofs against the current version are supported."); }
    if(versionA >= versionB){ throw std::logic_error("Version A must be always smaller than version B."); }
    if(proofFHs.size() != 0 || proofFHPos.size() != 0) {throw std::logic_error("Non-zero size of output proofs.") ;}
    proofFHs.resize(3 * getHeight()); // reserve the max possible elements
    proofFHPos.resize(3 * getHeight()); // reserve the max possible elements

    // 1) find the rightmost item in the current FH cache (and position) that "covers" the last element of versionA
    size_t rangeStart, rangeEnd;
    size_t revIdx =  proofFHPos.size() - 1;
    size_t iOFH = m_FH_pos.size() - 1; // idx pointing to original FH
    for (; iOFH >= 0; iOFH--){
        
        // a) copy FH cache and their positions to output lists - copy to the end of the container
        proofFHs[revIdx] = dev::h256(const_cast<const uint8_t *>(m_FH_cache.dataAt(iOFH)), dev::h256::ConstructFromPointer);
        proofFHPos[revIdx] = m_FH_pos[iOFH];
        revIdx--; 
        assert(revIdx >= 0);
        
        // b) get range of idxes covered by a current FHNode
        rangeStart = pow(2, m_FH_pos[iOFH].idxL) *  m_FH_pos[iOFH].idxE;
        rangeEnd   = pow(2, m_FH_pos[iOFH].idxL) * (m_FH_pos[iOFH].idxE + 1) - 1;

        if(versionA >= rangeStart)
            break;
    }
    assert(rangeStart != rangeEnd);           

    // 3) unveil found FH Node (to a pair of FH Nodes) until the last element of versionA is not the rightmost covered element by some unveiled FH Node
    while(!_isRightMostItem(versionA, rangeEnd)){
        // update revIdx here !!!
        // proceed in trail towards versionA
        TODO
    }

    // 4) copy the remaining FH Nodes from the original FH cache, which are on the left from the target node
    for (size_t i = iOFH - 1; i >= 0; i--){
        proofFHs[revIdx] = dev::h256(const_cast<const uint8_t *>(m_FH_cache.dataAt(i)), dev::h256::ConstructFromPointer);
        proofFHPos[revIdx] = m_FH_pos[i];
        revIdx--;
        assert(revIdx >= 0);
    }
   
    return 0;
}

bool HistoryTreeHost::_isRightMostItem(const size_t versionA, size_t rangeEnd){
    return versionA == rangeEnd;
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
