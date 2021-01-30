#include <iostream>
#include <string>

#include "merkle-tree.h"
#include "history-tree.h"

using namespace std;
using namespace eevm;

int main() {    
    
    
    int ITERS = 8;    

    //    test of Host history tree reduction of FHs (wit stubs)
    HistoryTreeHost htree2;
    for(int i = 0; i < ITERS; i++){
        string s = "test " + std::to_string(i);
        cout << "[i = " << i << "]" << " adding: " << s << "\n";        
        htree2.add(keccak_256(s));        
        cout << "root: " << htree2.getRoot().hex().substr(0, 6) << endl;        
        htree2.printLayers();
        // htree2.printFHCache();
        cout << "------------------\n";
    }    
    cout << "========================================\n";
    cout << "========================================\n";

    // test of Enclave reduction of FHs (without stubs)
    HistoryTreeEnc htree;
    for(int i = 0; i < ITERS; i++){
        cout << "[i = " << i << "]\n";
        string s = "test-" + i;
        KeccakHash ehash = keccak_256(s);
        htree.add(ehash);        
        cout << "root: " << htree.getRoot().hex().substr(0, 6) << endl;
        htree.printFHCache();
        cout << "\n";
    }        

    return 0;
}
