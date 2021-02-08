#include "eEVM/tracing.h"
#include "eEVM/util.h"
#include "history-tree.h"
#include <list>

/**
 * @brief Adds entry to m_layers and updates the full tree in m_layers as well.
 * Additionally, calls parent method to update SKN cache (i.e., current incremental proof)
 *
 * @param a
 */
void HistoryTreeHost::add(const eevm::KeccakHash& a)
{
    // insert new element at the end of the 0th layer (i.e., data hashes layer)
    m_layers[0].push_back(a);
    _updateLayersAndRoot();
    HistoryTreeEnc::add(a, false);  // update SKN cache and SKN positions by utilizing data of parent history tree (for enclave); skip recomputation of E's m_root
}

/**
 * @brief It updates all (cached) layers of the tree, including root. It inserts temporary stubs, which are removed after processing.
 *
 */
void HistoryTreeHost::_updateLayersAndRoot()
{
    int newLayerIdx = (getSizeElements() > 1) ? ceil(log2(getSizeElements())) : 1;  // idx of the new layer from bottom of the tree (note that log2(1) needs special handling)
    int maxLayerIdx = getHeight() - 1;
    if (maxLayerIdx < newLayerIdx) {
        m_layers.push_back(HashesArray());  // add a new layer if the depth of the currently extended tree was increased
    }

    // reduce the data layer into other layers (including root)
    bool stubAdded = false;
    for (size_t idxL = 0; idxL < getHeight() - 1; idxL++) {
        // a) insert stub element for odd size layers
        if (m_layers[idxL].size() % 2 != 0) {
            m_layers[idxL].push_back(EMPTY_HASH_OBJ);  // align the elements to even number
            stubAdded = true;
        }

        _reduceSingleLayer(idxL);  // uses default reduction mode

        // b) remove the last stub element of the layer idxL (if any)
        if (stubAdded) {
            m_layers[idxL].pop_back();
            stubAdded = false;
        }
    }
}

/**
 * @brief In contrast to _fullReduceSingleLayer Optimized by skipping of computations that were already done before (using SKN cache).
 *
 * @param idxL - index of the current layer to be reduced
 */
void HistoryTreeHost::_partialReduceSingleLayer(int idxL)
{
    assert(m_layers[idxL].size() % 2 == 0);  // we always have even number of elements in the current layer

    // 1) resize the next layer if needed
    if (m_layers[idxL].size() / 2 > m_layers[idxL + 1].size()) {
        m_layers[idxL + 1].resize(m_layers[idxL].size() / 2);
    }

    // 2) store the PARTIAL reduced content of the layer with 'idxL' to the layer with 'idxL' + 1
    size_t cntNodesInCurL = m_layers[idxL].size() - 1;        // -1 to not assume curently added element by add()
    size_t idxInCur = cntNodesInCurL - (cntNodesInCurL % 2);  // skip the (fixed) elements of old tree (-m_item % 2 to get idx on the left side)
    size_t idxInNext = (idxInCur / 2) - (idxInCur % 2);       // / 2 since it is 2 times slower | %2 to get on the left side
    eevm::keccak_256(m_layers[idxL].dataAt(idxInCur), 2 * HASH_SIZE, m_layers[idxL + 1].dataAt(idxInNext));
}

/**
 * @brief It reduces the full current layer of history tree into the next (above) layer; including root hash (i.e., the highest layer)
 * It can be used for fast loading of data from disk by function loadTree()
 *
 * @param idxL - index of the current layer to be reduced
 */
void HistoryTreeHost::_fullReduceSingleLayer(int idxL)
{
    assert(m_layers[idxL].size() % 2 == 0);  // we always have even number of elements in the current layer

    // 1) resize the next layer if needed
    if (m_layers[idxL].size() / 2 > m_layers[idxL + 1].size()) {
        m_layers[idxL + 1].resize(m_layers[idxL].size() / 2);
    }

    // store the reduced content of the current layer with 'idxL' to the layer with 'idxL' + 1
    for (size_t i = 0; i < m_layers[idxL].size(); i += 2) {
        int idxInHigherLayer = i / 2;  // twice slower than the lower layer
        eevm::keccak_256(m_layers[idxL].dataAt(i), 2 * HASH_SIZE, m_layers[idxL + 1].dataAt(idxInHigherLayer));
    }
}

int HistoryTreeHost::buildIncProof(const uint64_t versionA, const uint64_t versionB,
                                   std::vector<dev::h256>& proofFHs, std::vector<PositionNode>& proofFHPos)
{
    // 0) initial checks & allocation
    size_t curVer = getCurVersion();
    if (versionB < 1 || versionA < 1)
        throw std::domain_error("Version A and version B in inc proof must greater than 1 and current.");
    if (versionB != curVer)
        throw std::logic_error("Only inc proofs against the current version are supported.");
    if (proofFHs.size() != 0 || proofFHPos.size() != 0)
        throw std::invalid_argument("Non-zero size of output proofs.");
    if (versionA == versionB) {  // return empty inc proof for the same versions
        return 0;
    }

    // 1) [LEFT FROM TARGET] - find the item in the current SKN cache (and position) that "covers" the last element of versionA
    size_t rangeStart, rangeEnd;
    size_t iOFH = 0;  // idx pointing to original skelton nodes SKN
    for (; iOFH < m_SKN_pos.size(); iOFH++) {
        // a) get range of idxes covered by a current SKNode - and break if it already covers version A - note that versions are by +1 greater then indices
        rangeStart = pow(2, m_SKN_pos[iOFH].idxL) * m_SKN_pos[iOFH].idxE;
        rangeEnd = pow(2, m_SKN_pos[iOFH].idxL) * (m_SKN_pos[iOFH].idxE + 1) - 1;
        if (ver2Idx(versionA) >= rangeStart && ver2Idx(versionA) <= rangeEnd) {  // shift ranges to indices (that are by 1 bigger)
            break;
        }

        // b) copy the (left-positioned fixed or the target unfixed)  skeleton node SKNode and its position to output proofs
        proofFHs.push_back(std::move(dev::h256(const_cast<const uint8_t*>(m_SKN_cache.dataAt(iOFH)), dev::h256::ConstructFromPointer)));
        proofFHPos.push_back(m_SKN_pos[iOFH]);
    }
    assert(rangeStart != rangeEnd);

    // 3) [TARGET] - descend the target node - unfold found SKNode (to a pair of SKNodes) until the last element of version A is not the rightmost covered element by some unfolded FHNode
    auto* targetNode = &m_SKN_pos[iOFH];                                           // target node to unfold
    std::list<PositionNode> tmpPos;  // temporary list to keep unfolded positions in (it extends and shrinks)
    tmpPos.push_back(*targetNode);
    auto targetIt = tmpPos.end();                                                 // point before the element to insert into list
    while (rangeEnd != ver2Idx(versionA)) {
        // proceed in trail towards versionA

        // a) unfold the Position Node and insert it into proof as 2 new positions Nodes of the lower layer (while replacing the current one)
        uint64_t rightIdxInLower = 2 * targetNode->idxE + 1;                                  // idx of right node in the lower layer (2x faster indexing)
        tmpPos.insert(targetIt, PositionNode(targetNode->idxL - 1, rightIdxInLower));         // inserts at target iterator (right Position node)
        *std::prev(targetIt, 2) = PositionNode(targetNode->idxL - 1, rightIdxInLower - 1);  // replace the penultimate node - it is just unfolded   (left Position node)

        // b) get right ranges of indices covered by a left and right currently unfolded nodes
        auto rangeEndLeft = pow(2, std::prev(targetIt)->idxL) * rightIdxInLower - 1;
        auto rangeEndRight = pow(2, std::prev(targetIt)->idxL) * (rightIdxInLower + 1) - 1;

        // c) if we are not at bottom yet, then continue in descent
        if (ver2Idx(versionA) <= rangeEndLeft) {
            rangeEnd = rangeEndLeft;
            targetNode = &*std::prev(targetIt, 2);  // descend to left
            targetIt--;                            // set the iterator just after the target node
        } else {
            rangeEnd = rangeEndRight;
            targetNode = &*std::prev(targetIt);  // descend to right
            // targetIt -= 0; // iterator is already set just after the target node
        }

        // d) if we reached rightmost node that is complete in terms of powers
        // if (rangeEnd == ver2Idx(versionA))
        //     break;
    }

    // 4) tmpPos now contains unfolded elements that need to be copied to output proofs
    for (auto&& t : tmpPos) {
        proofFHs.emplace(proofFHs.end(), const_cast<const uint8_t*>(getNodeData(t.idxL, t.idxE)), dev::h256::ConstructFromPointer);
        proofFHPos.push_back(t);
    }

    // 5) [RIGHT FROM TARGET] - copy the remaining SKN nodes from the original SKN cache, which are on the right from the target SKNode
    iOFH++;  // adjust the idx to all next FHNodes that can be directly copied
    for (; iOFH < m_SKN_pos.size(); iOFH++) {
        proofFHs.push_back(std::move(dev::h256(const_cast<const uint8_t*>(m_SKN_cache.dataAt(iOFH)), dev::h256::ConstructFromPointer)));
        proofFHPos.push_back(m_SKN_pos[iOFH]);
    }
    return 0;
}

void HistoryTreeHost::printElements()
{
    auto& elems = const_cast<HashesArray&>(getElements());
    std::cout << "Elements are as follows "
              << "[size: " << elems.size() << "]:";
    for (size_t i = 0; i < elems.size(); i++) {
        std::cout << elems.toHex(i).substr(0, 6) << ", ";
    }
    std::cout << "\n";
}

void HistoryTreeHost::printLayers()
{
    for (int idxL = getLayers().size() - 1; idxL >= 0; idxL--) {
        auto& elems = const_cast<HashesArray&>(getLayers()[idxL]);

        std::cout << "[Layer: " << idxL << "]: ";
        for (size_t i = 0; i < elems.size(); i++) {
            std::cout << elems.toHex(i).substr(0, 6) << ", ";
        }
        std::cout << "\n";
    }
}

void HistoryTreeHost::printIncProof(const uint64_t versionA, const uint64_t versionB,
                                    std::vector<dev::h256>& proofFHs, std::vector<PositionNode>& proofFHPos)
{
    std::cout << "Printing Inc proof <" << versionA << "-" << versionB << ">:\n";
    std::cout << "Proof nodes: ";
    for (size_t i = 0; i < proofFHs.size(); i++) {
        std::cout << proofFHs[i].hex().substr(0, 6) << ", ";
    }
    std::cout << "\n";

    std::cout << "Proof positions: ";
    for (size_t i = 0; i < proofFHPos.size(); i++) {
        std::cout << proofFHPos[i].str() << ", ";
    }
    std::cout << "\n";
}
