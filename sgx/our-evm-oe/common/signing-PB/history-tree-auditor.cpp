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
    if (proofFHPos.size() > 2 * (ver2Height(versionNew))) {  // IH: max size of the proof should be 'tight' (the orig paper states 3*height) - test it as now
        throw std::invalid_argument("Max size of inc. proof was surpassed.");
    }

    // 2) verify the match of our skeleton with the initial part of the proof and copy the proof to linked list containers
    std::list<PositionNode> tmpFHPos;
    std::list<dev::h256> tmpFHs;
    for (size_t idx = 0; idx < proofFHPos.size(); idx++) {
        // a) compare inc proof nodes with our local skeleton 'm_SKN_cache' and 'm_SKN_pos' => return false if they differ
        if (idx < m_SKN_pos.size() && (proofFHPos[idx] != m_SKN_pos[idx] || proofFHs[idx] != m_SKN_cache.at(idx)))  // do not check for extended version range by new part of proof
            return false;                                                                                           // this ensures consitency with the past

        // b) copy to linked lists
        tmpFHs.push_back(proofFHs[idx]);
        tmpFHPos.push_back(proofFHPos[idx]);
    }

    // 3) reduce the incremental proof and store the computed root as the only element of lists passed
    _reduceIncProof(tmpFHs, tmpFHPos, versionNew);

    // 4) reject the proof if the reduced root is not equal to the passed one 'rootNew'
    if (tmpFHs.front() != rootNew)
        return false;

    // 5) update the local skeleton
    if (updateSKN && versionNew != myVersion)
        _updateMySkeleton(rootNew, versionNew, proofFHs, proofFHPos);

    return true;
}

void HistoryTreeAuditor::_updateMySkeleton(const dev::h256& rootNew, uint64_t versionNew,
                                           const std::vector<dev::h256>& proofFHs, const std::vector<PositionNode>& proofFHPos)
{
    // 1) adjust 'm_itemsCnt' => treeHeight()
    m_itemsCnt = versionNew;

    // 2) copy all FHNodes of the original proof
    m_SKN_cache.clear();
    m_SKN_pos.clear();
    for (size_t i = 0; i < proofFHs.size(); i++) {
        m_SKN_cache.push_back(proofFHs[i]);
        m_SKN_pos.push_back(proofFHPos[i]);
    }

    // 4) (try to) reduce the compound skeleton
    updateSKNCache();

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
void HistoryTreeAuditor::_reduceIncProof(std::list<dev::h256>& proofFHs, std::list<PositionNode>& proofFHPos, uint64_t versionNew)
{
    // 1) process the left part of the proof (corresponding to our current version) until there is no missing sibling
    _reduceIncProofStartingAt(proofFHs, proofFHPos, versionNew, m_SKN_pos.size() - 1);

    // 2) finish if one reduction was enought to get the root
    if (1 == proofFHs.size() && 1 == proofFHPos.size())
        return;

    // 3) reduce the proof again to get the root
    _reduceIncProofStartingAt(proofFHs, proofFHPos, versionNew, proofFHPos.size() - 1);

    assert(1 == proofFHs.size() && 1 == proofFHPos.size());
}

void HistoryTreeAuditor::_reduceIncProofStartingAt(std::list<dev::h256>& proofFHs, std::list<PositionNode>& proofFHPos, uint64_t versionNew, size_t startAtIdx)
{
    // 1) find iterators pointing on the last element of our skeleton 'm_SKN_pos' and 'm_SKN_cache'
    auto itCurPos = std::next(proofFHPos.begin(), startAtIdx);
    auto itCurFH = std::next(proofFHs.begin(), startAtIdx);

    // 2) process a selected part of the proof (from 'startAtIdx') until there is no matching sibling
    int maxLayerIdx = ver2Height(versionNew) - 1;
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

            // a2) check the presence of sibling in the proof => if not present, then break (and let the other run of this function to finish reduction)
            if (*std::next(itCurPos) != PositionNode(itCurPos->idxL, itCurPos->idxE + 1))
                break;

            // a3) reduce 2 position nodes
            *itCurPos = PositionNode(itCurPos->idxL + 1, itCurPos->idxE / 2);  // replace the current position node by a reduction of siblings
            proofFHPos.erase(std::next(itCurPos));                             // remove the right sibling

            // a4) reduce 2 hashes of FHs
            uint8_t srcBuf[2 * HASH_SIZE];
            memcpy(srcBuf, itCurFH->data(), HASH_SIZE);
            memcpy(srcBuf + HASH_SIZE, std::next(itCurFH)->data(), HASH_SIZE);
            eevm::keccak_256(srcBuf, 2 * HASH_SIZE, itCurFH->data());
            proofFHs.erase(std::next(itCurFH));
        } else {
            // b1) check the presence of sibling in the proof => if not present, then break (and let the next run of this function to finish reduction)
            if (*std::prev(itCurPos) != PositionNode(itCurPos->idxL, itCurPos->idxE - 1))
                break;

            // b2) reduce 2 positions nodes
            *std::prev(itCurPos) = PositionNode(itCurPos->idxL + 1, itCurPos->idxE / 2);  // replace the current position node by a reduction of siblings
            itCurPos = std::prev(proofFHPos.erase(itCurPos));                             // remove the current position node => we need to update the current iterator

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
