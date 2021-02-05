#include "eEVM/tracing.h"
#include "eEVM/util.h"
#include "history-tree.h"

/**
 * @brief It consistently adds element to the history tree, but does not store it.
 * It stores only incremental proof (i.e., m_SKN_cache), and thus saving the space
 *
 * @param a - hash of the element to add
 */
void HistoryTreeEnc::add(const eevm::KeccakHash& a, bool recomputeRoot)
{
    // 1) add new element
    m_SKN_cache.push_back(a);
    m_SKN_pos.push_back({0, m_itemsCnt});  // insert as the last node in the bottom layer
    m_itemsCnt++;

    // 2) reduce items of SKN cache to get (+1) Incremental proof in it
    _updateSKNCache();

    // 3) compute the root hash from SKN cache (and store it to m_root)
    if (recomputeRoot)
        computeRootFromSKNs();
}

/**
 * @brief After adding the entry to m_SKN_cache, we have to call this function, which updates the current SKNCache (i.e., most recent incremental proof)
 */
void HistoryTreeEnc::_updateSKNCache()
{
    int log = floor(log2(m_itemsCnt));
    for (int i = 2; i <= pow(2, log); i *= 2) {
        if (m_itemsCnt % i == 0) {  // do reduction only when we "complete" some power of 2
            if (m_SKN_cache.size() > 1) {
                // always reduce two last elements into penultimate one
                int idxLast = m_SKN_pos.size() - 1;
                assert(m_SKN_pos[idxLast].idxL == m_SKN_pos[idxLast - 1].idxL);  // two last nodes in SKN positions must be in the same layer

                // a) reduce 2 last positions of SKN nodes
                m_SKN_pos[idxLast - 1] = PositionNode({m_SKN_pos[idxLast - 1].idxL + 1, m_SKN_pos[idxLast - 1].idxE / 2});  // increase the layer and decrease the FHNode idx (by /2)
                m_SKN_pos.pop_back();

                // b) reduce 2 last SKN nodes themeselves
                auto* dest = m_SKN_cache.dataAt(m_SKN_cache.size() - 2);
                eevm::keccak_256(dest, 2 * HASH_SIZE, dest);  // IH: src and dest location is the same - hope it is OK !!!
                m_SKN_cache.pop_back();
            }
        }
    }
    assert(m_SKN_pos.size() == m_SKN_cache.size());
}

/**
 * @brief It computes root hash from skeleton 'm_SKN_cache' and stores it into m_root.
 *
 * @param offsetHeight [default = 0] - shifting of the tree height by this number (useful in proof verification with higher trees than ours)
 * @return const dev::h256& - new root hash
 */
const dev::h256& HistoryTreeEnc::computeRootFromSKNs(size_t offsetHeight)
{
    if (0 == m_SKN_cache.size())
        return m_root;  // root is initialized to  EMPTY_HASH_OBJ

    // 1) copy SKN cache and their positions to local temporary containers
    HashesArray tmpSKNCache;
    std::vector<PositionNode> tmpSKNPos;
    for (size_t i = 0; i < m_SKN_cache.size(); ++i) {
        tmpSKNCache.push_back(std::move(dev::h256(const_cast<const uint8_t*>(m_SKN_cache.dataAt(i)), dev::h256::ConstructFromPointer)));
        tmpSKNPos.push_back(m_SKN_pos[i]);
    }

    // 2) reduce the skeleton wihtin local arrays (in place)
    m_root = reduceSkeleton(tmpSKNCache, tmpSKNPos, offsetHeight);
    return m_root;
}

/**
 * @brief It reduces (in place) passed skeleton arrays to the root.
 * It also adds stubs for odd size layers to be compatible with incremental proofs higher than (+1) - utilized in HistoryTreeHost
 *
 * @param skeletonNodes
 * @param skeletonPos
 * @param offsetHeight [default = 0] - shifting of the tree height by this number
 * @return dev::h256
 */
dev::h256 HistoryTreeEnc::reduceSkeleton(HashesArray& skeletonNodes, std::vector<PositionNode>& skeletonPos, size_t offsetHeight)
{
    // 1) compute the root hash from the tmp array of SKNs and their positions - insert stubs if odd (according to positions)
    auto height = treeHeight() + offsetHeight;
    auto& lowestSKNPosNode = skeletonPos[skeletonPos.size() - 1];
    for (size_t iL = lowestSKNPosNode.idxL; iL < height - 1; iL++) {  // start at the position of the current layer taken from the last PositionNode (since it is the lowest one)

        // a) add stub SKNNode and its position if there is an odd number of elements in the layer
        if (0 != (lowestSKNPosNode.idxE + 1) % 2) {
            skeletonNodes.push_back(EMPTY_HASH_OBJ);
            skeletonPos.push_back({lowestSKNPosNode.idxL, lowestSKNPosNode.idxE + 1});
        }

        // b) reduce positions of SKNNodes (into the above layer)
        int idxLast = skeletonPos.size() - 1;
        skeletonPos[idxLast - 1] = PositionNode({skeletonPos[idxLast - 1].idxL + 1, skeletonPos[idxLast - 1].idxE / 2});  // increase the layer and decrease the FHNode idx (by /2)
        skeletonPos.pop_back();

        // c) reduce SKNNodes themselves (into the above layer)
        auto* dest = skeletonNodes.dataAt(skeletonNodes.size() - 2);
        eevm::keccak_256(dest, 2 * HASH_SIZE, dest);  // IH: src and dest location is the same - hope it is OK !!!
        skeletonNodes.pop_back();

        // d) update the lowest SKN position node
        lowestSKNPosNode = skeletonPos[skeletonPos.size() - 1];
    }
    auto root = dev::h256(skeletonNodes.data(), dev::h256::ConstructFromPointer);
    return root;
}


void HistoryTreeEnc::printSKNCache()
{
    std::cout << "SKN cache: ";
    for (size_t i = 0; i < m_SKN_cache.size(); i++) {
        std::cout << m_SKN_cache.toHex(i).substr(0, 6) << m_SKN_pos[i].str() << ", ";
    }
    std::cout << "\n";
}