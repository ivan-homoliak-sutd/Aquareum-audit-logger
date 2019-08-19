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

contract('PostingSC - TEST SUITE 2 [Posting a new ledger  root]:', function(accounts) {  
  var contract;

  it("Post a new ledger root 1st time (correct signature)", async () => {        
    contract = await PostingSC.deployed();    
    const initialRoot = await contract.LRoot_PB.call()    
    assert.equal(initialRoot, tee.LRoot_PB);

    var ledgerTransition = tee.nextLedgerTransition();
    // console.log("\t \\/== ledger transition is: ", ledgerTransition)
    var receipt = await contract.postLRoot(...ledgerTransition, {from: accounts[tee.account_idx]});    
    console.log(`\t \\/== Gas used in postLRoot:`, receipt.receipt.gasUsed);

    const newRoot = await contract.LRoot_PB.call()
    // console.log("newRoot is", newRoot)
    assert.equal(initialRoot, ledgerTransition[0]);
    assert.equal(newRoot, ledgerTransition[1]);
  });

  it("Post a new ledger root 2nd time (correct signature)", async () => {        
    contract = await PostingSC.deployed();    
    const initialRoot = await contract.LRoot_PB.call()    
    assert.equal(initialRoot, tee.LRoot_PB);

    var ledgerTransition = tee.nextLedgerTransition();
    // console.log("\t \\/== ledger transition is: ", ledgerTransition)
    var receipt = await contract.postLRoot(...ledgerTransition, {from: accounts[tee.account_idx]});    
    console.log(`\t \\/== Gas used in postLRoot:`, receipt.receipt.gasUsed);

    const newRoot = await contract.LRoot_PB.call()
    // console.log("newRoot is", newRoot)
    assert.equal(initialRoot, ledgerTransition[0]);
    assert.equal(newRoot, ledgerTransition[1]);
  });

  
  it("Post a new ledger root (incorrect signature)", async () => {        
    contract = await PostingSC.deployed();
    const initialRoot = await contract.LRoot_PB.call()   
    assert.equal(initialRoot, tee.LRoot_PB); 
    
    var ledgerTransition = [tee.LRoot_PB, h(tee.LRoot_PB)]
    
    try {
      var receipt = await contract.postLRoot(...ledgerTransition, {from: accounts[0]});    
      assert.fail('Expected revert not received');
    } catch (error) {
      const revertFound = error.message.search('revert') >= 0;
      assert(revertFound, `Expected "revert", got ${error} instead`);
    }    
  });

  it("Post a new ledger root (correct signature & wrong transition)", async () => {        
    contract = await PostingSC.deployed();
    const initialRoot = await contract.LRoot_PB.call()    
    assert.equal(initialRoot, tee.LRoot_PB);        
    
    try {
      var receipt = await contract.postLRoot("0x012345", "0x012345", {from: accounts[tee.account_idx]});    
      assert.fail('Expected revert not received');
    } catch (error) {
      const revertFound = error.message.search('revert') >= 0;
      assert(revertFound, `Expected "revert", got ${error} instead`);
    }    
  });

});

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