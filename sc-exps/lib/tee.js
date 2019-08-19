var W3 = require('web3');
function h(a) { return W3.utils.soliditySha3({v: a, t: "bytes", encoding: 'hex' }); }

PK_E_TEE_SEED = "0x0123"

var TEE = function (account = 1) {
    this._PK_E_TEE = h(PK_E_TEE_SEED); 
    this._PK_E_PB_account = account;    
}

Object.defineProperty(TEE.prototype, 'PK_E_TEE', {
    get: function () {
      return this._PK_E_TEE;
    }
})
Object.defineProperty(TEE.prototype, 'account_idx', {
    get: function () {
      return this._PK_E_PB_account;
    }
})

///// AUX Functions /////


function concatB32(a, b) {
    if (typeof(a) != 'string' || typeof(b) != 'string' || a.substr(0, 2) != '0x' || b.substr(0, 2) != '0x') {
        console.log("a, b = ", a, b)
        throw new Error("ConcatB32 supports only hex string arguments");
    }
    a = hexToBytes(a);
    b = hexToBytes(b);
    var res = []
    if (a.length != b.length || a.length != 16 || b.length != 16 ) {
        throw new Error("ConcatB32 supports only equally-long (16B) arguments.");
   } else {
        for (var i = 0; i < a.length; i++) {
            res.push(a[i])
        }
        for (var i = 0; i < b.length; i++) {
            res.push(b[i])
        }
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

Number.prototype.padLeft = function(size) {
    var s = this.toString(16)
    while (s.length < (size || 2)) {
      s = "0" + s;
    }
    return s;
  }

module.exports = TEE;
