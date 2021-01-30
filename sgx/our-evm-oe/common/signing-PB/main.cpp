#include <iostream>
#include <string>

#include "merkle-tree.h"
#include "history-tree.h"

using namespace std;
using namespace eevm;

int main() {    
    
    
    int ITERS = 10;

    // test of Enclave reduction of FHs (without stubs)
    // HistoryTreeEnc htree;
    // for(int i = 0; i < ITERS; i++){
    //     cout << "[i = " << i << "]\n";
    //     string s = "test-" + i;
    //     KeccakHash ehash = keccak_256(s);
    //     htree.add(ehash);        
    //     cout << "root: " << htree.getRoot().hex().substr(0, 6) << endl;
    //     htree.printFHCache();
    //     cout << "\n";
    // }    

    // test of Host history tree reduction of FHs (wit stubs)
    HistoryTreeHost htree;
    for(int i = 0; i < ITERS; i++){
        string s = "test " + std::to_string(i);
        cout << "[i = " << i << "]" << " adding: " << s << "\n";        
        htree.add(keccak_256(s));        
        cout << "root: " << htree.getRoot().hex().substr(0, 6) << endl;
        // htree.printElements();
        htree.printLayers();
        // htree.printFHCache();
        cout << "------------------\n";
    }    

    return 0;
}
