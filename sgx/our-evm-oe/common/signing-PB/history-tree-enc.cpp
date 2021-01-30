#include "eEVM/util.h"
#include "eEVM/tracing.h"
#include "history-tree.h"

    void HistoryTreeEnc::add(const eevm::KeccakHash& a)
    {
        // 1) add new element
        m_FH_cache.push_back(a);
        m_FH_pos.push_back({0, m_itemsCnt}); // insert as the last node in the bottom layer
        m_itemsCnt++;        
        
        // 2) reduce items of cache to get (+1) Incremental proof in it
        int log = floor(log2(m_itemsCnt));
    	for (int i = 2; i <= pow(2,log); i *= 2) {
    		if (m_itemsCnt % i == 0) {                
                if (m_FH_cache.size() > 1) {                    
                    int idxLast = m_FH_pos.size() - 1;
                    assert(m_FH_pos[idxLast].idxL == m_FH_pos[idxLast - 1].idxL); // two last nodes in FH positions must be in the same layer

                    // a) reduce positions of FH Nodes
                    m_FH_pos[idxLast - 1] = FHPositionNode({m_FH_pos[idxLast - 1].idxL + 1, m_FH_pos[idxLast - 1].idxE / 2}); // increase the layer and decrease the FHNode idx (by /2)
                    m_FH_pos.pop_back();

                    // b) reduce FH Nodes themeselves
                    auto * dest = m_FH_cache.data() + (m_FH_cache.size() - 2) * HASH_SIZE;
                    eevm::keccak_256(dest, 2 * HASH_SIZE, dest); // IH: src and dest location is the same - hope it is OK !!!
                    m_FH_cache.pop_back();  
                }             
    		}
    	}        
        assert(m_FH_pos.size() == m_FH_cache.size());

        // 3) compute the root hash from FH cache (and store it to m_root)
        computeRootFromFHs();        
    }

    /**
     * @brief It computes root hash from 'm_FH_cache' and stores it into m_root. 
     * It also adds stubs for odd size layers to be compatible with incremental proofs higher than (+1) - utilized in HistoryTreeHost
     * 
     */
    const dev::h256 & HistoryTreeEnc::computeRootFromFHs(){        
        if(0 == m_FH_cache.size()) return m_root; // root is initialized to  EMPTY_HASH_OBJ}
        
        // 1) copy FH cache to local array
        HashesArray tmpFHCache;
        std::vector<FHPositionNode> tmpFHPos;
        for (size_t i = 0; i < m_FH_cache.size(); ++i) {                                    
            tmpFHCache.push_back(std::move(dev::h256(const_cast<const uint8_t *>(m_FH_cache.data() + i * HASH_SIZE), dev::h256::ConstructFromPointer)));
            tmpFHPos.push_back(m_FH_pos[i]);
        }
        
        // 2) compute the root hash from the tmp array of FHs and their positions - insert stubs according to positions        
        auto & lowestFHPosNode = tmpFHPos[tmpFHPos.size() - 1];
        for(size_t iL = lowestFHPosNode.idxL; iL < treeHeight(); iL++) { // start at the position of the current layer taken from the last FHPositionNode (since it is the lowest one)                                    
            
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
            auto* dest = tmpFHCache.data() + (tmpFHCache.size() - 2)  * HASH_SIZE;
            eevm::keccak_256(dest, 2 * HASH_SIZE, dest); // IH: src and dest location is the same - hope it is OK !!!                                        
            tmpFHCache.pop_back();

            // d) update the lowest FH position node
            lowestFHPosNode = tmpFHPos[tmpFHPos.size() - 1];
        }        
        m_root = dev::h256(tmpFHCache.data(), dev::h256::ConstructFromPointer);                
        return m_root;
    }

    // void HistoryTreeEnc::computeRootFromFHsOld(){
        
    //     // 1) copy FH cache to local array
    //     HashesArray tmpFHCache;
    //     for (size_t i = 0; i < m_FH_cache.size(); ++i) {                                    
    //         tmpFHCache.push_back(std::move(dev::h256(const_cast<const uint8_t *>(m_FH_cache.data() + i * HASH_SIZE), dev::h256::ConstructFromPointer)));
    //     }
        
    //     // 2) compute the root hash from the local array
    //     uint8_t tmpH[HASH_SIZE];
    //     for (int i = m_FH_cache.size() - 2; i >= 0; i--) {            
    //             eevm::keccak_256(tmpFHCache.data() + i * HASH_SIZE, 2 * HASH_SIZE, tmpH);
    //             memcpy(tmpFHCache.data() + i * HASH_SIZE, tmpH, HASH_SIZE);            
    //     }        
    //     m_root = dev::h256(tmpFHCache.data(), dev::h256::ConstructFromPointer);                
    // }
    
    void HistoryTreeEnc::printFHCache() {        
        std::cout << "FH cache: ";
        for(size_t i = 0; i < m_FH_cache.size(); i++){
            std::cout <<  m_FH_cache.toHex(i).substr(0, 6) << m_FH_pos[i].str() << ", ";
        }        
        std::cout << "\n";
    }