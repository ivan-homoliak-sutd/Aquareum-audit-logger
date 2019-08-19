var W3 = require('web3');
const provider = new W3.providers.HttpProvider('http://localhost:9545')
const web3 = new W3(provider)
var Account = require("eth-lib/lib/account");

function h(a) { return W3.utils.soliditySha3({v: a, t: "bytes", encoding: 'hex' }); }

PK_E_TEE_SEED = "0x0123"


var TEE = function (eth_account) {
    // console.log("eth_account = ", eth_account);
    this._eth_account_E_PB = eth_account;

    this._PK_E_TEE = h(PK_E_TEE_SEED);
    this._PK_E_PB_address = eth_account.address;
    this._SK_E_PB = eth_account.privateKey;

    this._LRoot_PB = "0x0000000000000000000000000000000000000000000000000000000000000000"; // the first root of the empty ledger
}

Object.defineProperty(TEE.prototype, 'PK_E_TEE', {
    get: function () {
      return this._PK_E_TEE;
    }
})
Object.defineProperty(TEE.prototype, 'PK_E_PB_address', {
    get: function () {
      return this._PK_E_PB_address;
    }
})
Object.defineProperty(TEE.prototype, 'SK_E_PB', {
    get: function () {
      return this._SK_E_PB;
    }
})
Object.defineProperty(TEE.prototype, 'LRoot_PB', {
    get: function () {
      return this._LRoot_PB;
    }
})
Object.defineProperty(TEE.prototype, 'eth_account_E_PB', {
    get: function () {
      return this._eth_account_E_PB;
    }
})


TEE.prototype.nextLedgerTransition = function(){
    // Transition and signature are only emulated
    // Signature is emulated through using sender address in local Ethereum network (accounts array)
    var nextLRoot = h(this.LRoot_PB);
    var ledger_transition = [this.LRoot_PB, nextLRoot];

    // do the ledger stransition
    this._LRoot_PB = nextLRoot;

    return ledger_transition;
}

TEE.prototype.makeTicket = function(clientAddr, expiration){
    console.log("clientAddr= ", clientAddr)
    console.log("expiration= ", parseInt(expiration))

    var ticket = web3.eth.abi.encodeParameters(['address','uint256'], [clientAddr, parseInt(expiration)]);
    console.log("ticket= ", ticket)

    // sign ticket by SK_E_PB
    var msgHash = h(ticket);
    // console.log("hash of msg = ", msgHash);
    // var sig = W3.eth.accounts.sign(msgStr, this.privKey); // this prepends some  bull-string, so I've checked the lib and fetched only what is needed
    var sig = Account.sign(msgHash, this._SK_E_PB);
    sig = Account.decodeSignature(sig);
    sig = {r: sig[1], s: sig[2], v: sig[0]};
    console.log("sig = ", sig);

    return [ticket, [W3.utils.toDecimal(sig.v.substring(2)), sig.r, sig.s]];
}


///// AUX Functions /////


function concat(a, b) {
    if (typeof(a) != 'string' || typeof(b) != 'string' || a.substr(0, 2) != '0x' || b.substr(0, 2) != '0x') {
        console.log("a, b = ", a, b)
        throw new Error("Concat supports only hex string arguments");
    }
    console.log("a, b = ", a, b)
    a = hexToBytes(a);
    b = hexToBytes(b);
    var res = []

    for (var i = 0; i < a.length; i++) {
        res.push(a[i])
    }
    for (var i = 0; i < b.length; i++) {
        res.push(b[i])
    }

   return bytesToHex(res);
}

// Convert a byte array to a hex string
function bytesToHex(bytes) {
    var hex = [];
    for (i = 0; i < bytes.length; i++) {
        hex.push((bytes[i] >>> 4).toString(16));
        hex.push((bytes[i] & 0xF).toString(16));
    }
    // console.log("0x" + hex.join(""));
    return "0x" + hex.join("");
}

// Convert a hex string to a byte array
function hexToBytes(hex) {
    var bytes = [];
    for (c = 2; c < hex.length; c += 2)
        bytes.push(parseInt(hex.substr(c, 2), 16));
    return bytes;
}

function cloneArray(arr) {
    ret = []
    for (let i = 0; i < arr.length; i++) {
        ret.push(arr[i])
    }
    return ret
}

function hex2ascii(_hex) {
    var hex = _hex.toString(); // force conversion
    var str = '';
    for (var i = 2; (i < hex.length && hex.substr(i, 2) !== '00'); i += 2)
        str += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
    return str;
}

Number.prototype.padLeft = function(size) {
    var s = this.toString(16)
    while (s.length < (size || 2)) {
      s = "0" + s;
    }
    return s;
  }

module.exports = TEE;
