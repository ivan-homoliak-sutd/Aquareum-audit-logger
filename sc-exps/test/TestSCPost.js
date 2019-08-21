var PostingSC = artifacts.require("PostingSC");
var Web3 = require('web3');
var W3 = new Web3();
function h(a) { return W3.utils.soliditySha3({v: a, t: "bytes", encoding: 'hex' }); }

var TEE = require("../lib/tee.js");
var tee = new TEE(web3.eth.accounts.privateKeyToAccount("0x7a9f9c5137014611cef2171e4f3895ada5163dc42355dde85f3c1a8dbde53a9a"));

var CENS_RESOLUTION = Object.freeze({"NONE": 0, "PROCESSED": 1,  "ERROR" : 2});
var CENS_TYPE = Object.freeze({"WRITE": 0,  "READ" : 1});

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
    var receipt = await contract.postLRoot(...ledgerTransition, {from: tee.PK_E_PB_address});
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
    var receipt = await contract.postLRoot(...ledgerTransition, {from: tee.PK_E_PB_address});
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
      var receipt = await contract.postLRoot("0x012345", "0x012345", {from: tee.PK_E_PB_address});
      assert.fail('Expected revert not received');
    } catch (error) {
      const revertFound = error.message.search('revert') >= 0;
      assert(revertFound, `Expected "revert", got ${error} instead`);
    }
  });

});

// });//

// describe.skip("Skipped ", function(){

contract('PostingSC - TEST SUITE 3 [Censored WRITE TXs and resolution]:', function(accounts) {
  var contract;
  var client = accounts[2];

  it("Post a new censored WRITE request by C (correct signature & valid ticket)", async () => {
    contract = await PostingSC.deployed();
    var censTxsCnt = await contract.getCntOfCensTxs.call()
    assert.equal(censTxsCnt, 0);

    const expiration =  Date.now() / 1000 + 3600; // valid for 1 hour
    var args = tee.makeTicket(client, expiration); // [ticket, signature]
    // console.log("ticket=", args[0])
    // console.log("signature=", ...args[1])
    var txString = "0xdeadbeef";
    var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

    var receipt = await contract.submitCensTx(CENS_TYPE.WRITE, censTxBytes, "0x00", args[0], ...args[1], {from: client});
    console.log(`\t \\/== Gas used in submitCensTx:`, receipt.receipt.gasUsed);

    censTxsCnt = await contract.getCntOfCensTxs.call()
    assert.equal(censTxsCnt, 1);

    var censTx = await contract.censTXs.call(0);
    assert.equal(censTx[0], CENS_RESOLUTION.NONE);
    assert.equal(censTx[1], CENS_TYPE.WRITE);
  });

  it("Post a new request by C (invalid ticket => wrong PK_C)", async () => {
    contract = await PostingSC.deployed();
    var censTxsCnt = await contract.getCntOfCensTxs.call()
    assert.equal(censTxsCnt, 1);

    const expiration =  Date.now() / 1000 + 3600; // valid for 1 hour
    var args = tee.makeTicket(accounts[3], expiration); // [ticket, signature]
    var censTxBytes = "0xdeadbeef";

    try {
      var receipt = await contract.submitCensTx(CENS_TYPE.WRITE, censTxBytes, "0x00", args[0], ...args[1], {from: client});
      assert.fail('Expected revert not received');
    } catch (error) {
      const revertFound = error.message.search('revert') >= 0;
      assert(revertFound, `Expected "revert", got ${error} instead`);
    }
  });

  it("Post a new censored WRITE request by C (invalid ticket => expired time)", async () => {
    contract = await PostingSC.deployed();
    var censTxsCnt = await contract.getCntOfCensTxs.call()
    assert.equal(censTxsCnt, 1);

    const expiration =  Date.now() / 1000 - 1000; // expired time
    var args = tee.makeTicket(client, expiration); // [ticket, signature]
    var censTxBytes = "0xdeadbeef";

    try {
      var receipt = await contract.submitCensTx(CENS_TYPE.WRITE, censTxBytes, "0x00", args[0], ...args[1], {from: client});
      assert.fail('Expected revert not received');
    } catch (error) {
      const revertFound = error.message.search('revert') >= 0;
      assert(revertFound, `Expected "revert", got ${error} instead`);
    }
  });

  it("Resolve censored WRITE TX with idx = 0", async () => {
    contract = await PostingSC.deployed();
    var censTxsCnt = await contract.getCntOfCensTxs.call()
    assert.equal(censTxsCnt, 1);

    const idx_tx = 0;
    var txString = "0xdeadbeef";
    var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

    // console.log("censTxBytes = ", censTxBytes);
    // console.log("h(censTxBytes) = ", h(censTxBytes));

    var receipt = await contract.resolveCensTx(idx_tx, h(censTxBytes), "0x00", CENS_RESOLUTION.PROCESSED, {from: tee.PK_E_PB_address});
    console.log(`\t \\/== Gas used in resolveCensTx:`, receipt.receipt.gasUsed);

    var censTx = await contract.censTXs.call(0);
    assert.equal(censTx[0], CENS_RESOLUTION.PROCESSED);
  });
});

// });//

// describe.skip("Skipped ", function(){

  contract('PostingSC - TEST SUITE 4 [Censored READ TXs and resolution]:', function(accounts) {
    var contract;
    var client = accounts[2];
    var REPEAT_READ = 5;

    it("Post a sequence of censored READ requests by C", async () => {
      contract = await PostingSC.deployed();
      var censTxsCnt = await contract.getCntOfCensTxs.call()
      assert.equal(censTxsCnt, 0);

      const expiration =  Date.now() / 1000 + 3600; // valid for 1 hour
      var args = tee.makeTicket(client, expiration); // [ticket, signature]

      for (let i = 0; i < REPEAT_READ; i++) {
        var txString = "0x" + "deadbeefff".repeat(10); // tx size of 100B
        var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

        var receipt = await contract.submitCensTx(CENS_TYPE.READ, "0x00", h(censTxBytes), args[0], ...args[1], {from: client});
        console.log(`\t \\/== Gas used in submitCensTx[${i}]:`, receipt.receipt.gasUsed);

        censTxsCnt = await contract.getCntOfCensTxs.call()
        assert.equal(censTxsCnt, i + 1);

        var censTx = await contract.censTXs.call(i);
        assert.equal(censTx[0], CENS_RESOLUTION.NONE);
        assert.equal(censTx[2], h(censTxBytes));
        assert.equal(censTx[1], CENS_TYPE.READ);
      }
    });

    it("Resolve a sequence of censored READ TX", async () => {
      contract = await PostingSC.deployed();
      var censTxsCnt = await contract.getCntOfCensTxs.call()
      assert.equal(censTxsCnt, REPEAT_READ);

      for (let i = 0; i < REPEAT_READ; i++) {
        var txString = "0x" + "deadbeefff".repeat(10); // tx size of 100B
        var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

        var receipt = await contract.resolveCensTx(i, "0x00", censTxBytes, CENS_RESOLUTION.PROCESSED, {from: tee.PK_E_PB_address});
        console.log(`\t \\/== Gas used in resolveCensTx[${i}]:`, receipt.receipt.gasUsed);

        var censTx = await contract.censTXs.call(i);
        assert.equal(censTx[0], CENS_RESOLUTION.PROCESSED);
      }
    });
  });

  // });//

  // describe.skip("Skipped ", function(){

    contract('PostingSC - TEST SUITE 5 [Censored WRITE influenced by a size of TX]:', function(accounts) {
      var contract;
      var client = accounts[2];
      var REPEAT_WRITE = 101;
      const STEP_SIZE = 50; // Bytes

      it("Post a new censored WRITE request by C (correct signature & valid ticket)", async () => {
        contract = await PostingSC.deployed();
        var censTxsCnt = await contract.getCntOfCensTxs.call()
        assert.equal(censTxsCnt, 0);

        const expiration =  Date.now() / 1000 + 3600; // valid for 1 hour
        var args = tee.makeTicket(client, expiration); // [ticket, signature]

        for (let i = 0; i < REPEAT_WRITE; i++) {
          var txString = "0x" + "e".repeat(STEP_SIZE * i) + i.toString(); // tx size
          var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

          var receipt = await contract.submitCensTx(CENS_TYPE.WRITE, censTxBytes, "0x00", args[0], ...args[1], {from: client});
          console.log(`\t \\/== Gas used in submitCensTx[${i}]:`, receipt.receipt.gasUsed);

          censTxsCnt = await contract.getCntOfCensTxs.call()
          assert.equal(censTxsCnt, i + 1);

          var censTx = await contract.censTXs.call(i);
          assert.equal(censTx[0], CENS_RESOLUTION.NONE);
          assert.equal(censTx[2], h(censTxBytes));
          assert.equal(censTx[1], CENS_TYPE.WRITE);
        }
      });

      it("Resolve a sequence of censored WRITE TX", async () => {
        contract = await PostingSC.deployed();
        var censTxsCnt = await contract.getCntOfCensTxs.call()
        assert.equal(censTxsCnt, REPEAT_WRITE);

        for (let i = 0; i < REPEAT_WRITE; i++) {
          var txString = "0x" + "e".repeat(STEP_SIZE * i) + i.toString(); // tx size
          var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

          var receipt = await contract.resolveCensTx(i, h(censTxBytes), "0x00", CENS_RESOLUTION.PROCESSED, {from: tee.PK_E_PB_address});
          console.log(`\t \\/== Gas used in resolveCensTx[${i}]:`, receipt.receipt.gasUsed);

          var censTx = await contract.censTXs.call(i);
          assert.equal(censTx[0], CENS_RESOLUTION.PROCESSED);
        }
      });
    });

    // });//

    // describe.skip("Skipped ", function(){

    contract('PostingSC - TEST SUITE 5 [Censored READ influenced by a size of TX]:', function(accounts) {
      var contract;
      var client = accounts[2];
      var REPEAT_READ = 101;
      const STEP_SIZE = 50; // Bytes

      it("Post a new censored READ request by C (correct signature & valid ticket)", async () => {
        contract = await PostingSC.deployed();
        var censTxsCnt = await contract.getCntOfCensTxs.call()
        assert.equal(censTxsCnt, 0);

        const expiration =  Date.now() / 1000 + 3600; // valid for 1 hour
        var args = tee.makeTicket(client, expiration); // [ticket, signature]

        for (let i = 0; i < REPEAT_READ; i++) {
          var txString = "0x" + "e".repeat(STEP_SIZE * i) + i.toString(); // tx size
          var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

          var receipt = await contract.submitCensTx(CENS_TYPE.READ, "0x00", h(censTxBytes), args[0], ...args[1], {from: client});
          console.log(`\t \\/== Gas used in submitCensTx[${i}]:`, receipt.receipt.gasUsed);
          censTxsCnt = await contract.getCntOfCensTxs.call()
          assert.equal(censTxsCnt, i + 1);

          var censTx = await contract.censTXs.call(i);
          assert.equal(censTx[0], CENS_RESOLUTION.NONE);
          assert.equal(censTx[2], h(censTxBytes));
          assert.equal(censTx[1], CENS_TYPE.READ);
        }
      });

      it("Resolve a sequence of censored READ TX", async () => {
        contract = await PostingSC.deployed();
        var censTxsCnt = await contract.getCntOfCensTxs.call()
        assert.equal(censTxsCnt, REPEAT_READ);

        for (let i = 0; i < REPEAT_READ; i++) {
          var txString = "0x" + "e".repeat(STEP_SIZE * i) + i.toString(); // tx size
          var censTxBytes = web3.eth.abi.encodeParameter('bytes', txString);

          var receipt = await contract.resolveCensTx(i, "0x00", censTxBytes, CENS_RESOLUTION.PROCESSED, {from: tee.PK_E_PB_address});
          console.log(`\t \\/== Gas used in resolveCensTx[${i}]:`, receipt.receipt.gasUsed);

          var censTx = await contract.censTXs.call(i);
          assert.equal(censTx[0], CENS_RESOLUTION.PROCESSED);
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

Number.prototype.padLeft = function(size) {
  var s = this.toString(16)
  while (s.length < (size || 2)) {
    s = "0" + s;
  }
  return s;
}