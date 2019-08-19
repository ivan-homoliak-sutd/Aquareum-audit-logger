var PostingSC = artifacts.require("PostingSC");
var Web3 = require('web3');
var W3 = new Web3();
function h(a) { return W3.utils.soliditySha3({v: a, t: "bytes", encoding: 'hex' }).substring(0, 34); }

var TEE = require("../lib/tee.js");
var tee = new TEE(1);


// describe.skip("Skipped ", function(){

contract('PostingSC - TEST SUITE 1 [Initial checks]', function(accounts) {

  it("Zero Ballance", function(){
    return PostingSC.deployed()
    .then(function(instance) {
      // console.log(instance.contract._address);
      return web3.eth.getBalance(instance.contract._address);
    })
    .then(function(result) {
      assert.equal(0, result);
    });
  });

  it("Operator is account[0]", function(){
    return PostingSC.deployed()
    .then(function(instance) {
      operator = instance.contract.methods.PK_O().call();      
      return operator;
    })
    .then(function(operator) {
      // console.log(operator)
      assert.equal(accounts[0], operator);
    });
  });
});

// });//

// describe.skip("Skipped ", function(){

// contract('WaletHandle - TEST SUITE 2 [Deplete child tree OTPs, init new child tree ; deplete parent OTPs ; new parent tree]:', function(accounts) {
//   var owner = web3.eth.accounts[0];
//   var amount2Send = Number(W3.utils.toWei('0.1', 'ether'));
//   var receiver = web3.eth.accounts[1];
//   var contract;

//   it("Bootstrap / send 1 Eth at contract", async () => {
//     sender = accounts[1];
//     var initialAmount = Number(W3.utils.toWei('1', 'ether'));
//     var senderBalanceBefore = web3.eth.getBalance(sender);
//     contract = await WalletHandle.deployed();
//     txHash = await web3.eth.sendTransaction({from: sender, to: contract.contract.address, value: initialAmount});

//     const tx = await web3.eth.getTransaction(txHash);
//     const receipt = await web3.eth.getTransactionReceipt(txHash);
//     console.log(`\t \\/== Gas used: `, receipt.gasUsed);
//     const gasCost = tx.gasPrice.mul(receipt.gasUsed);

//     var expectedBallance = initialAmount;
//     assert.equal(web3.eth.getBalance(contract.contract.address).toNumber(), expectedBallance);

//     var senderBalanceAfter = web3.eth.getBalance(sender)
//     assert.equal(
//       senderBalanceBefore.toString(),
//       senderBalanceAfter.plus(gasCost).plus(initialAmount).toString()
//     );
//   });
// });

// });//

///// AUX Functions /////

const increaseTime = addSeconds => {
  web3.currentProvider.send({
      jsonrpc: "2.0",
      method: "evm_increaseTime",
      params: [addSeconds], id: 0
  })
}

const decreaseTime = addSeconds => {
  web3.currentProvider.send({
      jsonrpc: "2.0",
      method: "evm_decreaseTime",
      params: [addSeconds], id: 0
  })
}

var arr = {
  variance: function(array) {
    var mean = arr.mean(array);
    return arr.mean(array.map(function(num) {
      return Math.pow(num - mean, 2);
    }));
  },

  stddev: function(array) {
    return Math.sqrt(arr.variance(array));
  },

  mean: function(array) {
    return arr.sum(array) / array.length;
  },

  sum: function(array) {
    var num = 0;
    for (var i = 0, l = array.length; i < l; i++) num += array[i];
    return num;
  },
};

function hexToBytes(hex) { // Convert a hex string to a byte array
  var bytes = [];
  for (c = 2; c < hex.length; c += 2)
      bytes.push(parseInt(hex.substr(c, 2), 16));
  return bytes;
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

function round(x) {
  return Number.parseFloat(x).toFixed(2);
}

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