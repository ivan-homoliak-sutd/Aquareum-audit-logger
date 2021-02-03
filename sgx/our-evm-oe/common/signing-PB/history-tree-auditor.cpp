#include "eEVM/tracing.h"
#include "eEVM/util.h"
#include "history-tree.h"

/**
 * @brief Do the full verification of incremental proof - i.e., the passed proof contains also out sceleton on the left of it.
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
    size_t myVersion = getCurVersion();
    if (versionNew <= 1)
        throw std::domain_error("Version of inc proof must be greater than 1.");
    if (myVersion >= versionNew)
        throw std::domain_error("Version of inc proof must be greater than auditor's one.");
    if (proofFHPos.size() != proofFHs.size())
        throw std::invalid_argument("Sizes of proof arrays differ.");

    // 0) check constrains on proofs - ranges are always increasing and the size of the proof is limited by some value (to prevent DoS on clients)

    // 1) find the end idx of the first part of the proof (i.e., related to our current version) & copy these proof items into temporary containers
    size_t idx = 0;
    HashesArray tmpSKNCache;
    std::vector<PositionNode> tmpSKNPos;
    auto endIdx = (0 != myVersion % 2) ? ver2Idx(myVersion) + 1 : ver2Idx(myVersion);  // if the current version has odd elements, increase the version range to cover right sibling
    while (endIdxRange(proofFHPos[idx]) <= endIdx && idx < proofFHPos.size()) {
        // a) compare inc proof nodes with our local skeleton 'm_SKN_cache' and 'm_SKN_pos' => return false if they differ
        if (idx < m_itemsCnt && (proofFHPos[idx] != m_SKN_pos[idx] || proofFHs[idx] != m_SKN_cache.at(idx)))  // do not check for extended version range
            return false;                                                                                     // this ensures consitency with the past

        // b) copy
        tmpSKNCache.push_back(proofFHs[idx]);
        tmpSKNPos.push_back(proofFHPos[idx]);
        idx++;
    }

    // 2) reduce the skeleton within local containers (in place)
    auto rootLeft = reduceSkeleton(tmpSKNCache, tmpSKNPos);

    // 3) use computed root 'rootLeft' as a left element of the remaining items in inc proof
    tmpSKNCache.clear();
    tmpSKNPos.clear();
    tmpSKNCache.push_back(rootLeft);
    tmpSKNPos.push_back({treeHeight() - 1, 0ul});  // create position node on the left side of the current height idx

    // 4) copy remaining items (i.e., after idx, inclusive) of inc proof to temporary containers
    for (size_t i = idx; i < proofFHs.size(); i++) {
        tmpSKNCache.push_back(proofFHs[i]);
        tmpSKNPos.push_back(proofFHPos[i]);
    }

    // 5) reduce the prepared skeleton within local containers (in place)
    auto offsetHeight = ceil(log2(versionNew)) + 1 - treeHeight();  // difference between our height and the height of the new version (to add stubs correctly)
    auto rootNewReduced = reduceSkeleton(tmpSKNCache, tmpSKNPos, offsetHeight);

    // 6) reject the proof if the reduced root 'rootNewReduced' is not equal to the passed one 'rootNew'
    if (rootNewReduced != rootNew)
        return false;

    if (updateSKN)
        _updateMySkeleton(rootLeft, versionNew, idx, proofFHs, proofFHPos);

    return true;
}

void HistoryTreeAuditor::_updateMySkeleton(const dev::h256 & rootLeft, uint64_t versionNew, size_t startIdx,
                                           const std::vector<dev::h256>& proofFHs, const std::vector<PositionNode>& proofFHPos)
{
    // 1) copy computed root 'rootLeft' to the first position of my skeleton (put it on left)
    m_SKN_cache.clear();
    m_SKN_pos.clear();
    m_SKN_cache.push_back(rootLeft);
    m_SKN_pos.push_back({treeHeight() - 1, 0});  // create position node on the left side of the current heigth idx

    // 2) copy the remaining FHNodes from passed proof to my skeleton
    for (size_t i = startIdx; i < proofFHs.size(); i++) {
        m_SKN_cache.push_back(proofFHs[i]);
        m_SKN_pos.push_back(proofFHPos[i]);
    }

    // 3) adjust 'm_itemsCnt'
    m_itemsCnt = versionNew;
}
