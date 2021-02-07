#include "eEVM/tracing.h"
#include "eEVM/util.h"
#include "history-tree.h"
#include <list>

/**
 * @brief Do the full verification of incremental proof - i.e., the passed proof contains also our skeleton on the left part of it.
 *
 * @param versionNew
 * @param rootNew - this might be obtained from blockchain - i.e., not necessarily from proover
 * @param proofFHs - inc proof hashes
 * @param proofFHPos - inc proof positions
 * @param updateSKN - update auditor's skeleton and root hash if true
 */
bool HistoryTreeAuditor::verifyIncProofFull(uint64_t versionNew, const dev::h256& rootNew, const std::vector<dev::h256>& proofFHs,
                                            const std::vector<PositionNode>& proofFHPos, bool updateSKN)
{
    // 0) do some basic checks
    size_t myVersion = getCurVersion();
    if (versionNew < 1)
        throw std::domain_error("Version of inc proof must be greater or equal to 1.");
    if (myVersion > versionNew)
        throw std::domain_error("Version of inc proof must be greater than auditor's one.");
    if (proofFHPos.size() != proofFHs.size())
        throw std::invalid_argument("Sizes of proof arrays differ.");
    if (0 == proofFHPos.size()) {
        if (versionNew == myVersion)
            return true;  // for empty proof & no version difference, return true
        else
            return false;
    }

    // 1) checks the monotonicity of position ranges in proofs and the max lenght of proof
    for (size_t i = 1; i < proofFHPos.size(); i++) {  // ranges are always increasing
        if (endIdxRange(proofFHPos[i - 1]) >= endIdxRange(proofFHPos[i]))
            throw std::invalid_argument("End indices of proof positions nodes must be strictly increasing.");
    }
    if (proofFHPos.size() > 2 * (ver2Height(versionNew) - 1)) {  // IH: max size of the proof should be 'tight' (the orig paper states 3*height) - test it as now
        throw std::invalid_argument("Max size of inc. proof was surpassed.");
    }

    // 2) verify the match of our skeleton with the initial part of the proof and copy the proof to linked list containers
    std::list<PositionNode> tmpFHPos;
    std::list<dev::h256> tmpFHs;
    for (size_t idx = 0; idx < proofFHPos.size(); idx++) {
        // a) compare inc proof nodes with our local skeleton 'm_SKN_cache' and 'm_SKN_pos' => return false if they differ
        if (idx < m_itemsCnt && (proofFHPos[idx] != m_SKN_pos[idx] || proofFHs[idx] != m_SKN_cache.at(idx)))  // do not check for extended version range by new part of proof
            return false;                                                                                     // this ensures consitency with the past

        // b) copy to linked lists
        tmpFHs.push_back(proofFHs[idx]);
        tmpFHPos.push_back(proofFHPos[idx]);
    }

    // 3) reduce the incremental proof and store the computed root as the only element of lists passed
    size_t rightStartRevIdx;
    dev::h256 leftRoot = _reduceIncProof(tmpFHs, tmpFHPos, versionNew, &rightStartRevIdx);

    // 4) reject the proof if the reduced root is not equal to the passed one 'rootNew'
    if (tmpFHs.front() != rootNew)
        return false;

    // 5) update the local skeleton
    if (updateSKN && versionNew != myVersion)
        _updateMySkeleton(std::move(leftRoot), rootNew, versionNew, rightStartRevIdx, proofFHs, proofFHPos);

    return true;
}

void HistoryTreeAuditor::_updateMySkeleton(dev::h256&& rootLeft, const dev::h256& rootNew, uint64_t versionNew, size_t startRIdx,
                                           const std::vector<dev::h256>& proofFHs, const std::vector<PositionNode>& proofFHPos)
{
    // 1) copy computed root 'rootLeft' to the first position of my skeleton (put it on left)
    m_SKN_cache.clear();
    m_SKN_pos.clear();
    m_SKN_cache.push_back(rootLeft);
    m_SKN_pos.push_back(PositionNode(treeHeight() - 1, 0));  // create position node on the left side of the current heigth idx

    // 2) copy the remaining FHNodes from passed proof to my skeleton
    for (size_t i = proofFHs.size() - startRIdx; i < proofFHs.size(); i++) {
        m_SKN_cache.push_back(proofFHs[i]);
        m_SKN_pos.push_back(proofFHPos[i]);
    }

    // 3) adjust 'm_itemsCnt'
    m_itemsCnt = versionNew;

    // 4) update my root (without recomputation)
    m_root = rootNew;
    assert(rootNew == HistoryTreeEnc::computeRootFromSKNs());
}

/**
 * @brief Reduces the proof in place, while leaving the resulting root node in the input lists.
 * 
 * @param proofFHs 
 * @param proofFHPos 
 * @param versionNew 
 */
dev::h256 HistoryTreeAuditor::_reduceIncProof(std::list<dev::h256>& proofFHs, std::list<PositionNode>& proofFHPos, uint64_t versionNew,
                                              size_t* rightStartRevIdx)
{
    // 1) process the left part of the proof (corresponding to our current version) until the new height - 1 is not reached
    _reduceIncProofStartingAt(proofFHs, proofFHPos, versionNew, m_SKN_pos.size() - 1);

    // 2) store data for updating of local skeleton (the rest will be copied from original proof bu we need to store a position from which to copy)
    dev::h256 leftRoot = *proofFHs.begin();
    *rightStartRevIdx = proofFHs.size() - 1;

    // 3) process the right part of the proof (corresponding to a new version) until the new height - 1 is not reached
    _reduceIncProofStartingAt(proofFHs, proofFHPos, versionNew, proofFHPos.size() - 1);

    // 4) combine left and right parts to a root hash ajnd store it as the first element in proofFHs. Do not modify anything else.
    assert(2 == proofFHs.size() && 2 == proofFHPos.size());
    uint8_t srcBuf[2 * HASH_SIZE];
    memcpy(srcBuf, proofFHs.begin()->data(), HASH_SIZE);
    memcpy(srcBuf + HASH_SIZE, std::next(proofFHs.begin())->data(), HASH_SIZE);
    eevm::keccak_256(srcBuf, 2 * HASH_SIZE, proofFHs.begin()->data());
    return leftRoot;
}

void HistoryTreeAuditor::_reduceIncProofStartingAt(std::list<dev::h256>& proofFHs, std::list<PositionNode>& proofFHPos, uint64_t versionNew, size_t startAtIdx)
{
    // 1) find iterators pointing on the last element of our skeleton 'm_SKN_pos' and 'm_SKN_cache'
    auto itCurPos = std::next(proofFHPos.begin(), startAtIdx);
    auto itCurFH = std::next(proofFHs.begin(), startAtIdx);

    // 2) process a selected part of the proof (by 'startAtIdx') until the new height - 1 is not reached
    int maxLayerIdx = ver2Height(versionNew) - 2;
    assert(maxLayerIdx >= 0);
    size_t curLayerIdx = itCurPos->idxL;
    while (curLayerIdx < size_t(maxLayerIdx)) {
        // reduce the current node with its left/right sibling based on its index in the layer
        if (isLeft(*itCurPos)) {
            // a1) append stub if we reached the end of the proof
            if (proofFHPos.end() == std::next(itCurPos)) {
                proofFHPos.push_back(PositionNode(itCurPos->idxL, itCurPos->idxE + 1));
                proofFHs.push_back(EMPTY_HASH_OBJ);
            }

            // a2) check the validity of position in the proof
            if (*std::next(itCurPos) != PositionNode(itCurPos->idxL, itCurPos->idxE + 1))
                throw std::invalid_argument("Invalid position in (reduced) proof - expected sibling at the same layer.");

            // a3) reduce 2 position nodes
            *itCurPos = PositionNode(itCurPos->idxL + 1, itCurPos->idxE / 2);  // replace the current position node by a reduction of siblings
            proofFHPos.erase(std::next(itCurPos));                               // remove the right sibling

            // a4) reduce 2 hashes of FHs
            uint8_t srcBuf[2 * HASH_SIZE];
            memcpy(srcBuf, itCurFH->data(), HASH_SIZE);
            memcpy(srcBuf + HASH_SIZE, std::next(itCurFH)->data(), HASH_SIZE);
            eevm::keccak_256(srcBuf, 2 * HASH_SIZE, itCurFH->data());
            proofFHs.erase(std::next(itCurFH));
        } else {
            // b1) check the validity of position in the proof
            if (*std::prev(itCurPos) != PositionNode(itCurPos->idxL, itCurPos->idxE - 1))
                throw std::invalid_argument("Invalid position in (reduced) proof - expected sibling at the same layer.");

            // b2) reduce 2 positions nodes
            *std::prev(itCurPos) = PositionNode(itCurPos->idxL + 1, itCurPos->idxE / 2);  // replace the current position node by a reduction of siblings
            itCurPos = std::prev(proofFHPos.erase(itCurPos));                               // remove the current position node => we need to update the current iterator

            // b3) reduce 2 hashes of FHs
            uint8_t srcBuf[2 * HASH_SIZE];
            memcpy(srcBuf, std::prev(itCurFH)->data(), HASH_SIZE);
            memcpy(srcBuf + HASH_SIZE, itCurFH->data(), HASH_SIZE);
            eevm::keccak_256(srcBuf, 2 * HASH_SIZE, std::prev(itCurFH)->data());
            itCurFH = std::prev(proofFHs.erase(itCurFH));  // remove the current FH node => we need to update the current iterator
        }
        curLayerIdx++;
    }
    assert(proofFHPos.size() == proofFHs.size());
}


// auto itCurPos = std::find_if(std::begin(proofFHPos), std::end(proofFHPos), [](PositionNode& p) { return lastSKN == endIdxRange(p); });

// OLD one below
//////////////////////
// 1) find the end idx of the first part of the proof (i.e., related to our current version) & copy these proof items into temporary containers
// size_t idx = 0;
// HashesArray tmpSKNCache;
// std::vector<PositionNode> tmpSKNPos;
// bool myVersionIsOdd = 0 != myVersion % 2;
// auto endIdx = (myVersionIsOdd) ? ver2Idx(myVersion) + 1 : ver2Idx(myVersion);  // if the current version has odd elements, increase the version range to cover right sibling
// while (endIdxRange(proofFHPos[idx]) <= endIdx && idx < proofFHPos.size()) {
//     // a) compare inc proof nodes with our local skeleton 'm_SKN_cache' and 'm_SKN_pos' => return false if they differ
//     if (idx < m_itemsCnt && (proofFHPos[idx] != m_SKN_pos[idx] || proofFHs[idx] != m_SKN_cache.at(idx)))  // do not check for extended version range by new part of proof
//         return false;                                                                                     // this ensures consitency with the past

//     // b) copy
//     tmpSKNCache.push_back(proofFHs[idx]);
//     tmpSKNPos.push_back(proofFHPos[idx]);
//     idx++;
// }
// if (tmpSKNPos.size() == m_SKN_pos.size())  // the special case when the inc proof has the same version as our version
//     return true;

// // 2) reduce the skeleton within local containers (in place)
// auto rootLeft = reduceSkeleton(tmpSKNCache, tmpSKNPos);

// // 3) use computed root 'rootLeft' as a left element of the remaining items in inc proof
// tmpSKNCache.clear();
// tmpSKNPos.clear();
// tmpSKNCache.push_back(rootLeft);
// tmpSKNPos.push_back({treeHeight() - 1, 0ul});  // create position node on the left side of the current height idx

// // 4) copy remaining items (i.e., after idx, inclusive) of inc proof to temporary containers
// for (size_t i = idx; i < proofFHs.size(); i++) {
//     tmpSKNCache.push_back(proofFHs[i]);
//     tmpSKNPos.push_back(proofFHPos[i]);
// }

// // 5) reduce the prepared skeleton within local containers (in place)
// auto offsetHeight = ver2Height(versionNew) - treeHeight();  // difference between our height and the height of the new version (to add stubs correctly)
// auto rootNewReduced = reduceSkeleton(tmpSKNCache, tmpSKNPos, offsetHeight);

// // 6) reject the proof if the reduced root 'rootNewReduced' is not equal to the passed one 'rootNew'
// if (rootNewReduced != rootNew)
//     return false;

// if (updateSKN && versionNew != myVersion)
//     _updateMySkeleton(rootLeft, versionNew, idx, proofFHs, proofFHPos);

// return true;
