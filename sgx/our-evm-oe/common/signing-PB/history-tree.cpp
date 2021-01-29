#include "eEVM/util.h"
#include "history-tree.h"

    void HistoryTree::add(const eevm::KeccakHash& a)
    {
        m_FH_cache.push_back(a);
        m_itemsCnt++;        

        // reduce items of cache to get (+1) Incremental proof 
        int log = floor(log2(m_itemsCnt));
    	for (int i = 2; i <= pow(2,log); i *= 2) {
    		if (m_itemsCnt % i == 0) {                
                if (m_FH_cache.size() > 1) {
                    eevm::keccak_256(m_FH_cache.data() + (m_FH_cache.size() - 2) * HASH_SIZE, 2 * HASH_SIZE, m_FH_cache.data() + (m_FH_cache.size() - 2) * HASH_SIZE);
                    m_FH_cache.pop_back();  
                }              
    		}
    	}        

        // copy cache to local array
        HashesArray tmpCache;
        for (size_t i = 0; i < m_FH_cache.size(); ++i) {                        
            dev::h256 h = dev::h256(const_cast<const uint8_t *>(m_FH_cache.data() + i * HASH_SIZE), dev::h256::ConstructFromPointer);
            tmpCache.push_back(std::move(h));
        }
        
        // compute root hash from local array of FH cache
        uint8_t tmpH[HASH_SIZE];
        for (int i = m_FH_cache.size() - 2; i >= 0; i--) {            
                eevm::keccak_256(tmpCache.data() + i * HASH_SIZE, 2 * HASH_SIZE, tmpH);
                memcpy(tmpCache.data() + i * HASH_SIZE, tmpH, HASH_SIZE);            
        }        
        m_root = dev::h256(tmpCache.data(), dev::h256::ConstructFromPointer);        
    }
    
    void HistoryTree::printFHCache() {        
        std::cout << "FH cache: ";
        for(size_t i = 0; i < m_FH_cache.size(); i++){
            std::cout <<  m_FH_cache.toHex(i * HASH_SIZE)  << ", ";
        }        
        std::cout << "\n";
    }
