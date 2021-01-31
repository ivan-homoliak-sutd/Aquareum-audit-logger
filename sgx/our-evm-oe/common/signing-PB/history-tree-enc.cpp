#include "eEVM/util.h"
#include "eEVM/tracing.h"
#include "history-tree.h"

    /**
     * @brief It consistently adds element to the history tree, but does not store it. 
     * It stores only incremental proof (i.e., m_FH_cache), and thus saving the space
     * 
     * @param a - hash of the element to add 
     */
    void HistoryTreeEnc::add(const eevm::KeccakHash& a, bool recomputeRoot)
    {
        // 1) add new element
        m_FH_cache.push_back(a);
        m_FH_pos.push_back({0, m_itemsCnt}); // insert as the last node in the bottom layer
        m_itemsCnt++;        
        
        // 2) reduce items of FH cache to get (+1) Incremental proof in it
        _updateFHCache();          

        // 3) compute the root hash from FH cache (and store it to m_root)
        if(recomputeRoot) computeRootFromFHs();        
    }

    /**
     * @brief After adding the entry to m_FH_cache, we have to call this function, which updates the current FHCache (i.e., most recent incremental proof)     
     */
    void HistoryTreeEnc::_updateFHCache(){        
        int log = floor(log2(m_itemsCnt));
    	for (int i = 2; i <= pow(2,log); i *= 2) {
    		if (m_itemsCnt % i == 0) {   // do reduction only when we "complete" some power of 2
                if (m_FH_cache.size() > 1) {                                        
                    // always reduce two last elements into penultimate one
                    int idxLast = m_FH_pos.size() - 1;
                    assert(m_FH_pos[idxLast].idxL == m_FH_pos[idxLast - 1].idxL); // two last nodes in FH positions must be in the same layer

                    // a) reduce 2 last positions of FH Nodes
                    m_FH_pos[idxLast - 1] = FHPositionNode({m_FH_pos[idxLast - 1].idxL + 1, m_FH_pos[idxLast - 1].idxE / 2}); // increase the layer and decrease the FHNode idx (by /2)
                    m_FH_pos.pop_back();

                    // b) reduce 2 last FH Nodes themeselves
                    auto * dest = m_FH_cache.dataAt(m_FH_cache.size() - 2);
                    eevm::keccak_256(dest, 2 * HASH_SIZE, dest); // IH: src and dest location is the same - hope it is OK !!!
                    m_FH_cache.pop_back();  
                }             
    		}
    	}
        assert(m_FH_pos.size() == m_FH_cache.size());      
    }

    /**
     * @brief It computes root hash from 'm_FH_cache' and stores it into m_root. 
     * It also adds stubs for odd size layers to be compatible with incremental proofs higher than (+1) - utilized in HistoryTreeHost
     * 
     */
    const dev::h256 & HistoryTreeEnc::computeRootFromFHs(){        
        if(0 == m_FH_cache.size()) return m_root; // root is initialized to  EMPTY_HASH_OBJ}
        
        // 1) copy FH cache and their positions to local arrays
        HashesArray tmpFHCache;
        std::vector<FHPositionNode> tmpFHPos;
        for (size_t i = 0; i < m_FH_cache.size(); ++i) {                                    
            tmpFHCache.push_back(std::move(dev::h256(const_cast<const uint8_t *>(m_FH_cache.dataAt(i)), dev::h256::ConstructFromPointer)));
            tmpFHPos.push_back(m_FH_pos[i]);
        }
        
        // 2) compute the root hash from the tmp array of FHs and their positions - insert stubs if odd (according to positions)        
        auto & lowestFHPosNode = tmpFHPos[tmpFHPos.size() - 1];
        for(size_t iL = lowestFHPosNode.idxL; iL < treeHeight() - 1; iL++) { // start at the position of the current layer taken from the last FHPositionNode (since it is the lowest one)                                                            
           
            // a) add stub FHNode and its position if there is an odd number of elements in the layer
            if(0 != (lowestFHPosNode.idxE + 1) % 2){ 
                tmpFHCache.push_back(EMPTY_HASH_OBJ);
                tmpFHPos.push_back({lowestFHPosNode.idxL, lowestFHPosNode.idxE + 1});                
            }

            // b) reduce  positions of FHNodes (into the above layer)
            int idxLast = tmpFHPos.size() - 1;
            tmpFHPos[idxLast - 1] = FHPositionNode({tmpFHPos[idxLast - 1].idxL + 1, tmpFHPos[idxLast - 1].idxE / 2}); // increase the layer and decrease the FHNode idx (by /2)
            tmpFHPos.pop_back();

            // c) reduce FHNodes themselves (into the above layer)
            auto* dest = tmpFHCache.dataAt(tmpFHCache.size() - 2);
            eevm::keccak_256(dest, 2 * HASH_SIZE, dest); // IH: src and dest location is the same - hope it is OK !!!                                        
            tmpFHCache.pop_back();

            // d) update the lowest FH position node
            lowestFHPosNode = tmpFHPos[tmpFHPos.size() - 1];            
        }        
        m_root = dev::h256(tmpFHCache.data(), dev::h256::ConstructFromPointer);                
        return m_root;
    }

    void HistoryTreeEnc::printFHCache() {        
        std::cout << "FH cache: ";
        for(size_t i = 0; i < m_FH_cache.size(); i++){
            std::cout <<  m_FH_cache.toHex(i).substr(0, 6) << m_FH_pos[i].str() << ", ";
        }        
        std::cout << "\n";
    }