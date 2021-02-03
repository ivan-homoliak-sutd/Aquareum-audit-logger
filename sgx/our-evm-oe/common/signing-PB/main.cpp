#include <iostream>
#include <string>

#include "merkle-tree.h"
#include "history-tree.h"

using namespace std;
using namespace eevm;

int main() {            
    int ITERS = 19;    

    dev::h256 root1, root2, root3;    
    std::string seedStr = "test ";

    // test of Enclave reduction of SKNs (without stubs)
    cout << "HistoryTreeEnc...\n"; 
    HistoryTreeEnc htree;
    for(int i = 0; i < ITERS; i++){        
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "]" << " adding: " << s << "\n";        
        KeccakHash ehash = keccak_256(s);
        htree.add(ehash);        
        root1 = htree.getRoot();
        cout << "root: " << htree.getRoot().hex().substr(0, 6) << endl;
        htree.printSKNCache();
        cout << "------------------\n";
    }        
    cout << "========================================\n";    

    //  test of Host history tree reduction of FHs (wit stubs)
    cout << "HistoryTreeHost (full reduce)\n";
    HistoryTreeHost htree2(HistoryTreeHost::ReduceType::FULL);
    for(int i = 0; i < ITERS; i++){
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "]" << " adding: " << s << "\n";        
        htree2.add(keccak_256(s));        
        root2 = htree2.getRoot();
        cout << "root: " << htree2.getRoot().hex().substr(0, 6) << endl;        
        htree2.printLayers();
        // htree2.printSKNCache();
        cout << "------------------\n";
    }    
    assert(root1 == root2);
    cout << "========================================\n";        

    //  test of Host history tree reduction of FHs (wit stubs)
    cout << "HistoryTreeHost (partial reduce)\n";
    HistoryTreeHost htree3(HistoryTreeHost::ReduceType::PARTIAL);
    for(int i = 0; i < ITERS; i++){
        string s = seedStr + std::to_string(i);
        cout << "[i = " << i << "]" << " adding: " << s << "\n";        
        htree3.add(keccak_256(s));        
        root3 = htree3.getRoot();
        cout << "root: " << htree3.getRoot().hex().substr(0, 6) << endl;        
        htree3.printLayers();
        // htree3.printSKNCache();
        cout << "------------------\n";
    }    
    assert(root2 == root3);
    cout << "========================================\n";    
    
    cout << "HistoryTreeHost (incremental proof generation)\n";
    std::vector<dev::h256> incProofFHs;
    std::vector<PositionNode> incProofFHPos;
    size_t curVer = htree3.getCurVersion();
    for (size_t i = 1; i < curVer; i++) {        
        incProofFHs.clear();
        incProofFHPos.clear();

        htree3.buildIncProof(i, curVer, incProofFHs, incProofFHPos);
        htree3.printIncProof(i, curVer, incProofFHs, incProofFHPos);     
        cout << "------------------\n";           
    }
    return 0;
}
