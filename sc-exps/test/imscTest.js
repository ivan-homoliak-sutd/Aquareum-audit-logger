var imsc = artifacts.require("imsc");

var imscContract;
var operatorA;  // join at creation
var operatorA_IPSC;
var operatorB;  // join at creation
var operatorB_IPSC;
var operatorC;  // join at creation
var operatorC_IPSC;
var operatorD;  // not joined
var operatorD_IPSC;
var operatorE;  // not joined
var operatorE_IPSC;

contract('IMSC - TEST SUITE 1 [Initial checks and setup]', function (accounts) {

    operatorA = accounts[1];
    operatorA_IPSC = operatorA;
    operatorB = accounts[2];
    operatorB_IPSC = operatorB;
    operatorC = accounts[3];
    operatorC_IPSC = operatorC;
    operatorD = accounts[4];
    operatorD_IPSC = operatorD;
    operatorE = accounts[5];
    operatorE_IPSC = operatorE;

    it("Deploying imsc contract", async () => {
        var imscContract = await imsc.deployed();
    });

});

contract('IMSC - TEST SUITE 2 [Normal protocol]', function (accounts) {

    before('deploy imsc contract', async () => {
        imscContract = await imsc.deployed();
    });

    it("Join request by operator D instance", async () => {

        // Call contract
        result = await imscContract.joinRequest(operatorD_IPSC, { from: operatorD });

        // Check if event occur
        assert(result.logs[0], "Event not emited");
        assert.equal(
            "joinRequested",
            result.logs[0].event,
            "Event joinRequested not occurred"
        );
    });

    it("Approve request", async () => {

        // Approve 1/2
        result = await imscContract.approveRequest(operatorA_IPSC, operatorD_IPSC, { from: operatorA });

        // Approve 2/2
        result = await imscContract.approveRequest(operatorB_IPSC, operatorD_IPSC, { from: operatorB });

        // Check if event occur
        assert(result.logs[0], "Event not emited");
        assert.equal(
            "joined",
            result.logs[0].event,
            "Event joined not occurred"
        );

        result = await imscContract.isJoined(operatorD_IPSC, { from: operatorA });
        assert.equal(
            true,
            result,
            "Operator D should have been joined"
        );

        result = await imscContract.isJoined(operatorE_IPSC, { from: operatorA });
        assert.equal(
            false,
            result,
            "Operator E should not have been joined"
        );

    });

    it("Join request by operatorE instance", async () => {

        // Call contract
        result = await imscContract.joinRequest(operatorE_IPSC, { from: operatorE });

        // Check if event occur
        assert(result.logs[0], "Event not emited");
        assert.equal(
            "joinRequested",
            result.logs[0].event,
            "Event joinRequested not occurred"
        );
    });

    it("Approve request", async () => {

        // Approve 1/3
        result = await imscContract.approveRequest(operatorB_IPSC, operatorE_IPSC, { from: operatorB });

        // Approve 2/3
        result = await imscContract.approveRequest(operatorC_IPSC, operatorE_IPSC, { from: operatorC });

        // Approve 2/3
        result = await imscContract.approveRequest(operatorD_IPSC, operatorE_IPSC, { from: operatorD });

        // Check if event occur
        assert(result.logs[0], "Event not emited");
        assert.equal(
            "joined",
            result.logs[0].event,
            "Event joined not occurred"
        );

        result = await imscContract.isJoined(operatorE_IPSC, { from: operatorA });
        assert.equal(
            true,
            result,
            "Operator E should have been joined"
        );

    });

});
