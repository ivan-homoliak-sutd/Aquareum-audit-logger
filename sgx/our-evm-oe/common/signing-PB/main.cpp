#include <iostream>
#include <string>

#include "history-tree.h"
#include "merkle-tree.h"
// #include "stacktrace.h"

using namespace std;
using namespace eevm;


/**
 * @brief Compares reduce of skeleton nodes of 'HistoryTreeEnc' with full reduce and partial reduce in 'HistoryTreeHost' on equality.
 *
 * @param seedStr
 * @param ITERS
 */
void testReduce(const std::string& seedStr, uint64_t ITERS)
{
    cout << "TEST 1\n";
    dev::h256 root1, root2, root3;

    // Enclave reduction of SKNs (without stubs)
    cout << "HistoryTreeEnc...\n";
    HistoryTreeEnc htree;
    for (uint64_t i = 0; i < ITERS; i++) {
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "]"
             << " adding: " << s << "\n";
        KeccakHash ehash = keccak_256(s);
        htree.add(ehash);
        root1 = htree.getRoot();
        cout << "root: " << htree.getRoot().hex().substr(0, 6) << endl;
        htree.printSKNCache();
        cout << "------------------\n";
    }
    cout << "========================================\n";

    // Host history tree reduction of FHs (with stubs)
    cout << "HistoryTreeHost (full reduce)\n";
    HistoryTreeHost htree2(HistoryTreeHost::ReduceType::FULL);
    for (uint64_t i = 0; i < ITERS; i++) {
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "]"
             << " adding: " << s << "\n";
        htree2.add(keccak_256(s));
        root2 = htree2.getRoot();
        cout << "root: " << htree2.getRoot().hex().substr(0, 6) << endl;
        htree2.printLayers();
        // htree2.printSKNCache();
        cout << "------------------\n";
    }
    assert(root1 == root2);
    cout << "========================================\n";

    //  test of Host history tree reduction of FHs (with stubs)
    cout << "HistoryTreeHost (partial reduce)\n";
    HistoryTreeHost proverTree(HistoryTreeHost::ReduceType::PARTIAL);
    for (uint64_t i = 0; i < ITERS; i++) {
        string s = seedStr + std::to_string(i);

        cout << "[i = " << i << "]"
             << " adding: " << s << "\n";
        proverTree.add(keccak_256(s));
        root3 = proverTree.getRoot();
        cout << "root: " << proverTree.getRoot().hex().substr(0, 6) << endl;
        proverTree.printLayers();
        // proverTree.printSKNCache();
        cout << "------------------\n";
    }
    assert(root2 == root3);
    cout << "========================================\n";
}

void testVerificationOfIncProofs(const std::string& seedStr, uint64_t ITERS, const std::string& genesisData)
{
    cout << "TEST 2\n";
    dev::h256 rootProover;
    eevm::KeccakHash genesisHash = eevm::keccak_256(reinterpret_cast<const uint8_t*>(genesisData.c_str()), genesisData.size());

    cout << "HistoryTree[Host|Auditor] (<1-N> incremental proof generation + verification)\n";
    const size_t verifVersion = 1;  // the verifier1 does not update its skeleton
    std::vector<dev::h256> incProofFHs;
    std::vector<PositionNode> incProofFHPos;
    HistoryTreeHost proverTree(HistoryTreeHost::ReduceType::PARTIAL);
    HistoryTreeAuditor verifierTree(genesisHash);
    proverTree.add(genesisHash);  // add the same genesis element as in verifierTree

    for (uint64_t i = 1; i < ITERS; i++) {  // start from 1, since genesis element was already added to 'verifierTree' and 'prooverTree'
        assert(verifVersion == 1);
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "] adding: " << s << "\n";
        size_t proverVer = proverTree.getCurVersion();

        // a) [not updating verifier] generation & verification of Inc Proof - do not update verifier's skeleton if correct
        incProofFHs.clear();
        incProofFHPos.clear();
        proverTree.buildIncProof(verifVersion, proverVer, incProofFHs, incProofFHPos);
        proverTree.printIncProof(verifVersion, proverVer, incProofFHs, incProofFHPos);
        if (!verifierTree.verifyIncProofFull(i, proverTree.getRoot(), incProofFHs, incProofFHPos, false)) {
            throw logic_error("Incorrect inc. proof provided to verifier.");
        }

        proverTree.add(keccak_256(s));  // increase version of proover by adding a new element
        cout << "------------------\n";
    }
    cout << "========================================\n";
}

void testVerificationOfIncProofs2(const std::string& seedStr, uint64_t ITERS, const std::string& genesisData)
{
    cout << "TEST 3\n";
    dev::h256 rootProover;
    eevm::KeccakHash genesisHash = eevm::keccak_256(reinterpret_cast<const uint8_t*>(genesisData.c_str()), genesisData.size());

    cout << "HistoryTree[Host|Auditor] (<(N-1)-N>incremental proof generation + verification)\n";
    size_t verif2Version = 1;  // the verifier1 does not update its skeleton
    std::vector<dev::h256> incProofFHs;
    std::vector<PositionNode> incProofFHPos;
    HistoryTreeHost proverTree(HistoryTreeHost::ReduceType::PARTIAL);
    HistoryTreeAuditor verifierTree2(genesisHash);  // does updates of skeleton
    proverTree.add(genesisHash);                    // add the same genesis element as in verifierTree

    for (uint64_t i = 1; i < ITERS; i++) {  // start from 1, since genesis element was already added to 'verifierTree' and 'prooverTree'
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "] adding: " << s << "\n";
        proverTree.add(keccak_256(s));  // increase version of proover by adding a new element

        size_t proverVer = proverTree.getCurVersion();
        verif2Version = verifierTree2.getCurVersion();

        // generation & verification of Inc Proof - does update verifier's skeleton if correct | the proofs should be empty
        incProofFHs.clear();
        incProofFHPos.clear();
        proverTree.buildIncProof(verif2Version, proverVer, incProofFHs, incProofFHPos);
        proverTree.printIncProof(verif2Version, proverVer, incProofFHs, incProofFHPos);
        verifierTree2.printSKNCache();
        if (!verifierTree2.verifyIncProofFull(proverVer, proverTree.getRoot(), incProofFHs, incProofFHPos, true)) {
            throw logic_error("Incorrect inc. proof provided to verifier2.");
        }
        cout << "------------------\n";
    }
    cout << "========================================\n";
}

void testVerificationOfIncProofs3(const std::string& seedStr, uint64_t ITERS, const std::string& genesisData, size_t deltaVersions)
{
    cout << "TEST 4."<< deltaVersions << "\n";
    dev::h256 rootProover;
    eevm::KeccakHash genesisHash = eevm::keccak_256(reinterpret_cast<const uint8_t*>(genesisData.c_str()), genesisData.size());

    cout << "HistoryTree[Host|Auditor] (<N-(N+" << deltaVersions << ")>incremental proof generation + verification)\n";
    size_t verif2Version = 1;  // the verifier1 does not update its skeleton
    std::vector<dev::h256> incProofFHs;
    std::vector<PositionNode> incProofFHPos;
    HistoryTreeHost proverTree(HistoryTreeHost::ReduceType::PARTIAL);
    HistoryTreeAuditor verifierTree2(genesisHash);  // does updates of skeleton
    proverTree.add(genesisHash);                    // add the same genesis element as in verifierTree

    for (uint64_t i = 1; i < ITERS; i++) {  // start from 1, since genesis element was already added to 'verifierTree' and 'prooverTree'
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "] adding: " << s << "\n";
        proverTree.add(keccak_256(s));  // increase version of proover by adding a new element

        size_t proverVer = proverTree.getCurVersion();
        verif2Version = verifierTree2.getCurVersion();

        // generation & verification of Inc Proof - does update verifier's skeleton if correct | the proofs should be empty
        incProofFHs.clear();
        incProofFHPos.clear();
        proverTree.buildIncProof(verif2Version, proverVer, incProofFHs, incProofFHPos);
        proverTree.printIncProof(verif2Version, proverVer, incProofFHs, incProofFHPos);
        verifierTree2.printSKNCache();
        if (1 == (i % deltaVersions)) {
            cout << "proof verification\n";
            if (!verifierTree2.verifyIncProofFull(proverVer, proverTree.getRoot(), incProofFHs, incProofFHPos, true)) {
                throw logic_error("Incorrect inc. proof provided to verifier2.");
            }
            verifierTree2.printSKNCache();
        }
        cout << "------------------\n";
    }
    cout << "========================================\n";
}

int main()
{
    uint64_t ITERS = 30;

    std::string seedStr = "test ";
    std::string genesisData = seedStr + "0";

    try {
        testReduce(seedStr, ITERS);
        testVerificationOfIncProofs(seedStr, ITERS, genesisData);
        testVerificationOfIncProofs2(seedStr, ITERS, genesisData);
        testVerificationOfIncProofs3(seedStr, ITERS, genesisData, 2);
        testVerificationOfIncProofs3(seedStr, ITERS, genesisData, 3);
        testVerificationOfIncProofs3(seedStr, ITERS, genesisData, 4);
        testVerificationOfIncProofs3(seedStr, ITERS, genesisData, 5);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        // auto s = backtrace();
        // std::cerr << "backtrace(): \n"
        //   << s << '\n';
    }


    // cout << "HistoryTree[Host|Auditor] (incremental proof generation + verification)\n";
    // HistoryTreeAuditor auditTree(genesisHash);
    // curVer = proverTree.getCurVersion();
    // for (uint64_t i = 1; i < curVer; i++) {
    //     incProofFHs.clear();
    //     incProofFHPos.clear();
    //     proverTree.buildIncProof(i, curVer, incProofFHs, incProofFHPos);
    //     proverTree.printIncProof(i, curVer, incProofFHs, incProofFHPos);

    //     // verification of Inc Proof
    //     assert(auditTree.verifyIncProofFull(curVer, proverTree.getRoot(), incProofFHs, incProofFHPos, false));
    //     cout << "------------------\n";
    // }
    // cout << "========================================\n";

    return 0;
}
