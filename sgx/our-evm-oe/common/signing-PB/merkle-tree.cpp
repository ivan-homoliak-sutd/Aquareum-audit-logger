#include <iostream>
#include <string>

#include "merkle-tree.h"
#include "history-tree.h"

using namespace std;
using namespace eevm;

int main() {
    // MerkleTreeArray objtst;
    HistoryTree objtst;

    string s = "test1";
    KeccakHash rcpHash = keccak_256(reinterpret_cast<const uint8_t*>(&s[0]), s.size());
    objtst.add(rcpHash);
    dev::h256 c = objtst.getRoot();
	cout << c.hex() << endl;
    objtst.print_tree();
    cout << endl;

    s = "test2";
    rcpHash = keccak_256(reinterpret_cast<const uint8_t*>(&s[0]), s.size());
    objtst.add(rcpHash);
    c = objtst.getRoot();
	cout << c.hex() << endl;
    objtst.print_tree();
    cout << endl;

    s = "test3";
    rcpHash = keccak_256(reinterpret_cast<const uint8_t*>(&s[0]), s.size());
    objtst.add(rcpHash);
    c = objtst.getRoot();
	cout << c.hex() << endl;
    objtst.print_tree();
    cout << endl;

    s = "btc";
    rcpHash = keccak_256(reinterpret_cast<const uint8_t*>(&s[0]), s.size());
    objtst.add(rcpHash);
    c = objtst.getRoot();
	cout << c.hex() << endl;
    objtst.print_tree();
    cout << endl;

    return 0;
}
