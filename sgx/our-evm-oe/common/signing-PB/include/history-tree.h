#pragma once

#include "aleth-mp3/FixedHash.h"
#include "merkle-tree.h"
#include <list>

class PositionNode {
public:
    PositionNode(const size_t _idxL, const uint64_t _idxE)
    {
        // std::cerr << "PositionNode::def_cons\n";
        idxL = _idxL;
        idxE = _idxE;
    }

    // PositionNode(const PositionNode& other)
    //   : idxL(other.idxL), idxE(other.idxE)
    // {
    //     std::cerr << "PositionNode::copy_cons"
    //               << "\n";
    // }
    // PositionNode(PositionNode&& other)
    // {
    //     std::cerr << "PositionNode::move_cons"
    //               << "\n";
    //     this->idxL = std::exchange(other.idxL, 9999);  // move idxL, while leaving 9999 in other.idxL
    //     this->idxE = std::exchange(other.idxE, 9999);
    // }
    // PositionNode(PositionNode& other) = delete;                    // copy assignment operator with copy-and-swap idiom => disabled
    // PositionNode& operator=(const PositionNode& other) = default;  // copy assignment operator
    // PositionNode& operator=(PositionNode&& other) noexcept         // move assignment operator
    // {
    //     std::cerr << "PositionNode::move_assgn_oper"
    //               << "\n";
    //     if (this != &other) {
    //         this->idxL = std::exchange(other.idxL, 9999);  // move idxL, while leaving 9999 in other.idxL
    //         this->idxE = std::exchange(other.idxE, 9999);
    //     }
    //     return *this;
    // }

    size_t idxL;    // index of layer of the node from the bottom (starting by 0)
    uint64_t idxE;  // index of node within the layer (starting by 0)

    inline std::string str()
    {
        std::stringstream ss;
        ss << " [" << idxL << "," << idxE << "]";
        return ss.str();
    }

    bool operator==(const PositionNode& other) const
    {
        return other.idxL == this->idxL && other.idxE == this->idxE;
    }
    bool operator!=(const PositionNode& other) const
    {
        return !(other == *this);
    }
};

class HistoryTreeEnc {
public:
    HistoryTreeEnc()
      : m_itemsCnt(0), m_root(EMPTY_HASH_OBJ)
    {}

    void add(const eevm::KeccakHash& a, bool recomputeRoot = true);

    inline virtual const dev::h256& getRoot() { return m_root; }  // the root hash - note it is valid only after calling computeRootFromFH()

    inline const size_t treeHeight() const { return (m_itemsCnt != 1) ? ceil(log2(m_itemsCnt)) + 1 : 2; }  // includes also stub nodes (if any)

    inline uint64_t getCurVersion() { return m_itemsCnt; }

    void printSKNCache();

    void printElements();

    inline PositionNode& getSKNPositionNodeRef(int idx)
    {
        assert(m_SKN_pos.size() >= (size_t)abs(idx));  // range check
        idx = (idx < 0) ? m_SKN_pos.size() + idx : idx;
        return m_SKN_pos[idx];
    }

    void updateSKNCache();  // reduces FHCache (called after adding a new item AND after updating auditor's data from inc. proof)

    void updateSKNs(uint64_t elemsCnt, HashesArray& SKNodes, std::vector<PositionNode>& SKNPos);

protected:
    const dev::h256& computeRootFromSKNs(size_t offsetHeight = 0);  // store root into m_root and returs its reference

    dev::h256 reduceSkeleton(HashesArray& skeletonNodes, std::vector<PositionNode>& skeletonPos, size_t offsetHeight = 0);

    HashesArray m_SKN_cache;              // cache of skeleton hashes of tree - all of them are already fixed (used mostly by E)
    std::vector<PositionNode> m_SKN_pos;  // positions of SKN nodes within the tree (it corresponds to the above array)
    uint64_t m_itemsCnt;                  // the number of items in the history tree
    dev::h256 m_root;
};

class HistoryTreeAuditor : public HistoryTreeEnc {
public:
    HistoryTreeAuditor(const eevm::KeccakHash& genesisE)
      : HistoryTreeEnc()
    {
        add(genesisE);  // set up genesis element; this also updates the root hash
    }

    bool verifyIncProofFull(uint64_t versionNew, const dev::h256& rootNew, const std::vector<dev::h256>& proofFHs,
                            const std::vector<PositionNode>& proofFHPos, bool updateSKN = false);

    friend uint64_t endIdxRange(const PositionNode& pos);
    friend size_t ver2Height(uint64_t version);
    friend bool isLeft(const PositionNode& pos);

private:
    void _updateMySkeleton(const dev::h256& rootNew, uint64_t versionNew,
                           const std::vector<dev::h256>& proofFHs, const std::vector<PositionNode>& proofFHPos);

    void _reduceIncProof(std::list<dev::h256>& proofFHs, std::list<PositionNode>& proofFHPos, uint64_t versionNew);

    void _reduceIncProofStartingAt(std::list<dev::h256>& proofFHs, std::list<PositionNode>& proofFHPos, uint64_t versionNew, size_t startAtIdx);
};

class HistoryTreeHost : public HistoryTreeEnc {
public:
    enum ReduceType { FULL,
                      PARTIAL };

    HistoryTreeHost(ReduceType r = PARTIAL)
      : HistoryTreeEnc(), m_layers(1), m_reduceType(r)  // the first layer will store elements
    {}

    void add(const eevm::KeccakHash& a);

    int buildIncProof(const uint64_t versionA, const uint64_t versionB, std::vector<dev::h256>& proofFHs, std::vector<PositionNode>& proofFHPos);

    inline HashesArray& getElements() { return m_layers[0]; }  // excluding stub (if any)

    inline size_t getSizeElements() { return m_layers[0].size(); }  // excluding stub (if any)

    inline size_t getHeight() { return m_layers.size(); }

    inline const dev::h256& getRoot() override
    {
        m_root = m_layers[m_layers.size() - 1].at(0);
        return m_root;
    }

    inline dev::h256 getNode(int idxLayer, int64_t idxElem)
    {
        assert(m_layers[idxLayer].size() >= (size_t)((idxElem >= 0) ? idxElem : -idxElem));  // range check
        idxElem = (idxElem < 0) ? m_layers[idxLayer].size() + idxElem : idxElem;
        return m_layers[idxLayer].at(idxElem);
    }

    inline const uint8_t* getNodeData(int idxLayer, int64_t idxElem)
    {
        assert(m_layers[idxLayer].size() >= (size_t)((idxElem >= 0) ? idxElem : -idxElem));  // range check
        idxElem = (idxElem < 0) ? m_layers[idxLayer].size() + idxElem : idxElem;
        return m_layers[idxLayer].dataAt(idxElem);
    }

    inline const std::vector<HashesArray>& getLayers() { return m_layers; }

    inline void loadTree() { throw std::logic_error("Not implememented."); }  // enable to load data from disk quickly (not by add(), which is slow)

    void printLayers();
    void printElements();
    void printIncProof(const uint64_t versionA, const uint64_t versionB,
                       std::vector<dev::h256>& proofFHs, std::vector<PositionNode>& proofFHPos);

    friend size_t ver2Idx(size_t version);

private:
    void _updateLayersAndRoot();

    inline void _reduceSingleLayer(size_t idxL)
    {  // wrapper for the following two
        if (FULL == m_reduceType) {
            _fullReduceSingleLayer(idxL);
        } else if (PARTIAL == m_reduceType) {
            _partialReduceSingleLayer(idxL);
        } else
            throw std::logic_error("Unknown reduction mode of a layer.");
    }
    void _fullReduceSingleLayer(int idxL);     // it passes the full layer and reduces it to the next one (not optimal)
    void _partialReduceSingleLayer(int idxL);  // it passes reduces only changed parts of the layer (optimal)

    std::vector<HashesArray> m_layers;  // cached layers of non-terminal nodes of the tree - they serve for fast provision of proofs to C
    ReduceType m_reduceType;
};

// converts domain of versions to domain of indices in tree
inline uint64_t ver2Idx(uint64_t version)
{
    assert(version >= 1);
    return version - 1;
}

inline uint64_t endIdxRange(const PositionNode& pos)
{
    return pow(2, pos.idxL) * (pos.idxE + 1) - 1;
}

inline size_t ver2Height(uint64_t version)
{
    assert(version != 0);
    return (version != 1) ? ceil(log2(version)) + 1 : 2;
}

inline bool isLeft(const PositionNode& pos)
{
    return 1 == (pos.idxE + 1) % 2;
}