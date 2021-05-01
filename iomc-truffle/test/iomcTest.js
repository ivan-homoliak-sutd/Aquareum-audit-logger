var iomcSend = artifacts.require("iomcSend");
var iomcReceive = artifacts.require("iomcReceive");

const helper = require('../lib/utils.js');
var Web3 = require('web3');
var W3 = new Web3();
const BN = W3.utils.BN;
W3.setProvider(new W3.providers.HttpProvider("http://localhost:8777"));

var iomcSendContract;
var iomcReceiveContract;

var operator;
var clientA;
var clientB;

const PREIMAGE = 10;
const INCORRECT_PREIMAGE = 9999;
const HASHLOCK_STRING = web3.utils.soliditySha3({ type: 'uint256', value: PREIMAGE });
const AMOUNT = new BN(Math.pow(10, 17).toString(), 10);
const HASHLOCK = new BN(HASHLOCK_STRING.substring(2), 16);

const SECONDS_IN_DAY = 86400;
const TIME_SHIFT = 60 * 5; // 5 minutes

contract('IOMC - TEST SUITE 1 [Initial checks and setup]', function (accounts) {

  operator = accounts[0];
  clientA = accounts[1];
  clientB = accounts[2];

  it("Deploying iomcSend contract", async () => {
    iomcSendContract = await iomcSend.deployed();
  });

  it("Deploying iomcReceive contract", async () => {
    iomcReceiveContract = await iomcReceive.deployed();
  });
});

contract('IOMC - TEST SUITE 2 [iomc normal protocol]', function (accounts) {

  var senderContractId;
  var receiverContractId;
  var result;

  before('deploy iomc contracts', async () => {
    iomcSendContract = await iomcSend.deployed();
    iomcReceiveContract = await iomcReceive.deployed();
  });

  it("TX sendInitialize by clientA (account[1])", async () => {
    let balanceClientA = new BN(await web3.eth.getBalance(clientA));
    let balanceIomcSendContract = new BN(await web3.eth.getBalance(iomcSendContract.address));

    // Call IOMC
    result = await iomcSendContract.sendInitialize(clientB, clientB, HASHLOCK, { from: clientA, value: AMOUNT });
    senderContractId = result.logs[0].args.contractId;

    // Check if event occur
    assert.equal(
      "sendInitialized",
      result.logs[0].event,
      "Event sendInitialized not occurred"
    );

    // Coins are now on IOMC contract
    assert.equal(
      await web3.eth.getBalance(iomcSendContract.address),
      balanceIomcSendContract.add(AMOUNT),
      "Coins are not sended to IOMC contract"
    );
    // Coins 
    assert.equal(
      true,
      balanceClientA.sub(AMOUNT).gte(new BN(await web3.eth.getBalance(clientA))),
      "Sender should have less coins than before callign sendInitialize"
    );
  });

  it("TX receiveInitialize by clientB (account[2])", async () => {
    result = await iomcReceiveContract.receiveInitialize(clientA, clientA, HASHLOCK, AMOUNT, { from: clientB });
    receiverContractId = result.logs[0].args.contractId;

    // Check if event occur
    assert.equal(
      "receiveInitialized",
      result.logs[0].event,
      "Event receiveInitialized not occurred"
    );
  });

  it("TX sendCommit by clientA (account[1])", async () => {
    let balanceIomcSendContract = new BN(await web3.eth.getBalance(iomcSendContract.address));

    // Call sendCommit
    result = await iomcSendContract.sendCommit(senderContractId, new BN(PREIMAGE), { from: clientA });

    // Check if event occur
    assert.equal(
      "sendCommited",
      result.logs[0].event,
      "Event sendCommited not occurred"
    );

    // Coins shold be burn
    assert.equal(
      await web3.eth.getBalance(iomcSendContract.address),
      balanceIomcSendContract.sub(AMOUNT),
      "Coins are not burned in IOMC contract"
    );
  });

  it("Insufficient coins in receiving contract", async () => {
    let balanceClientB = new BN(await web3.eth.getBalance(clientB));
    let balanceIomcReceiveContract = new BN(await web3.eth.getBalance(iomcReceiveContract.address));

    // Call receiveClaim
    result = await iomcReceiveContract.receiveClaim(receiverContractId, new BN(PREIMAGE), { from: clientB });

    // Check if event occur
    assert.equal(
      "notEnoughReserve",
      result.logs[0].event,
      "Event notEnoughReserve not occurred"
    );

    // Balance of receiving IOMC contract is without change
    assert.equal(
      await web3.eth.getBalance(iomcReceiveContract.address),
      balanceIomcReceiveContract,
      "Balance of contract is not the same as before call"
    );
    // Receiver have not got coins
    assert.equal(
      true,
      balanceClientB.gte(new BN(await web3.eth.getBalance(clientB))),
      "Receiver balance is greated than expected"
    );
  });

  it("Fund iomcReceive by operator (account[0])", async () => {
    let balanceIomcReceiveContract = new BN(await web3.eth.getBalance(iomcReceiveContract.address));

    // fund contract by operator before claim
    result = await iomcReceiveContract.fund({ from: operator, value: AMOUNT });

    // Coins are funded on IOMC receiving contract
    assert.equal(
      await web3.eth.getBalance(iomcReceiveContract.address),
      balanceIomcReceiveContract.add(AMOUNT),
      "Coins are not funded to IOMC receiving contract by operator"
    );
  });

  it("TX receiveClaim by clientB (account[2])", async () => {
    let balanceClientB = new BN(await web3.eth.getBalance(clientB));
    let balanceIomcReceiveContract = new BN(await web3.eth.getBalance(iomcReceiveContract.address));

    // Call receiveClaim
    result = await iomcReceiveContract.receiveClaim(receiverContractId, new BN(PREIMAGE), { from: clientB });

    // Get transaction fee
    let txFee = new BN(result.receipt.gasUsed).mul(new BN(await web3.eth.getGasPrice()));

    // Check if event occur
    assert.equal(
      "successfulyClaimed",
      result.logs[0].event,
      "Event successfulyClaimed not occurred"
    );

    // Coins are no longer on IOMC receiving contract
    assert.equal(
      await web3.eth.getBalance(iomcReceiveContract.address),
      balanceIomcReceiveContract.sub(AMOUNT),
      "Coins are still on IOMC receiving contract"
    );

    // Receiver gets coins
    assert.equal(
      await web3.eth.getBalance(clientB),
      balanceClientB.add(AMOUNT).sub(txFee),
      "Receiver does not receive the coins"
    );
  });
});

contract('IOMC - TEST SUITE 3 [sender revert timelock contract]', function (accounts) {

  var senderContractId;

  var snapshotId;

  before('deploy iomc contracts', async () => {
    // Snapshot
    snapShot = await helper.takeSnapshot();
    snapshotId = snapShot['result'];

    iomcSendContract = await iomcSend.deployed();
    iomcReceiveContract = await iomcReceive.deployed();

    result = await iomcSendContract.sendInitialize(clientB, clientB, HASHLOCK, { from: clientA, value: AMOUNT });
    senderContractId = result.logs[0].args.contractId;
  });

  it("TX sendRevert before timelock (revert tx)", async () => {
    let balanceClientA = new BN(await web3.eth.getBalance(clientA));
    let balanceIomcSendContract = new BN(await web3.eth.getBalance(iomcSendContract.address));

    // Before timelock
    await helper.expectThrow(
      iomcSendContract.sendRevert(senderContractId, { from: clientA })
    );

    // IOMC sending contract coins are the same
    assert.equal(
      await web3.eth.getBalance(iomcSendContract.address),
      balanceIomcSendContract,
      "IOMC sending contract coins are NOT the same as was before reverting"
    );
    // ClientsA does not get coins  
    assert.equal(
      true,
      balanceClientA.gt(new BN(await web3.eth.getBalance(clientA))),
      "Sender has more coins than before"
    );
  });

  it("TX sendRevert after timelock", async () => {
    // Time Shift
    await helper.advanceTimeAndBlock(TIME_SHIFT);

    let balanceClientA = new BN(await web3.eth.getBalance(clientA));
    let balanceIomcSendContract = new BN(await web3.eth.getBalance(iomcSendContract.address));

    let result = await iomcSendContract.sendRevert(senderContractId, { from: clientA });

    // Get transaction fee
    let txFee = new BN(result.receipt.gasUsed).mul(new BN(await web3.eth.getGasPrice()));

    // Check if event occur
    assert.equal(
      "sendReverted",
      result.logs[0].event,
      "Event sendReverted not occurred"
    );

    // Unsended coins are withdrawn from sending IOMC contract 
    assert.equal(
      await web3.eth.getBalance(iomcSendContract.address),
      balanceIomcSendContract.sub(AMOUNT),
      "Coins are not withdrawned"
    );
    // Clients balance 
    assert.equal(
      await web3.eth.getBalance(clientA),
      balanceClientA.add(AMOUNT).sub(txFee),
      "Sender have not got reverted money"
    );
  });

  after('revert to snapshot', async () => {
    // Revert time blockchain 
    await helper.revertToSnapShot(snapshotId);
  });

});

contract('IOMC - TEST SUITE 4 [Not allow actions]', function (accounts) {

  var senderContractId;
  var receiverContractId;
  var result;

  before('deploy iomc contracts', async () => {
    iomcSendContract = await iomcSend.deployed();
    iomcReceiveContract = await iomcReceive.deployed();

    result = await iomcSendContract.sendInitialize(clientB, clientB, HASHLOCK, { from: clientA, value: AMOUNT });
    senderContractId = result.logs[0].args.contractId;

    result = await iomcReceiveContract.receiveInitialize(clientA, clientA, HASHLOCK, AMOUNT, { from: clientB });
    receiverContractId = result.logs[0].args.contractId;
  });

  it("Incorrect preimage in sendCommit by clientA (account[1])", async () => {
    let balanceIomcSendContract = new BN(await web3.eth.getBalance(iomcSendContract.address));

    // Call sendCommit
    await helper.expectThrow(
      iomcSendContract.sendCommit(senderContractId, new BN(INCORRECT_PREIMAGE), { from: clientA })
    );

    // Balance of sending IOMC contract sould be unchanged
    assert.equal(
      await web3.eth.getBalance(iomcSendContract.address),
      balanceIomcSendContract,
      "Balance of contract is not the same as before call"
    );
  });

  it("Incorrect preimage in receiveClaim by clientB (account[0])", async () => {
    let balanceClientB = new BN(await web3.eth.getBalance(clientB));
    let balanceIomcReceiveContract = new BN(await web3.eth.getBalance(iomcReceiveContract.address));

    // Call receiveClaim
    await helper.expectThrow(
      iomcReceiveContract.receiveClaim(receiverContractId, new BN(INCORRECT_PREIMAGE), { from: clientB })
    );

    // Balance of receiving IOMC contract is without change
    assert.equal(
      await web3.eth.getBalance(iomcReceiveContract.address),
      balanceIomcReceiveContract,
      "Balance of contract is not the same as before call"
    );
    // Receiver have not got coins
    assert.equal(
      true,
      balanceClientB.gte(new BN(await web3.eth.getBalance(clientB))),
      "Receiver balance is greated than expected"
    );
  });

  it("Fund by someone else than operator", async () => {
    let balanceClientA = new BN(await web3.eth.getBalance(clientA));
    let balanceIomcReceiveContract = new BN(await web3.eth.getBalance(iomcReceiveContract.address));

    // Call receiveClaim
    await helper.expectThrow(
      iomcReceiveContract.fund({ from: clientA, value: AMOUNT })
    );

    // Balance of receiving IOMC contract is without change
    assert.equal(
      await web3.eth.getBalance(iomcReceiveContract.address),
      balanceIomcReceiveContract,
      "Balance of contract is not the same as before call"
    );
    // Client did not lose money
    assert.equal(
      true,
      balanceClientA.sub(AMOUNT).lt(new BN(await web3.eth.getBalance(clientA))),
      "Client balance is lower than expected"
    );
  });

});
