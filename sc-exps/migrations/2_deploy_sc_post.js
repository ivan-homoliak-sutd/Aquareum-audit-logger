var W3 = require('web3');
var PostingSC = artifacts.require("PostingSC");
var TEE = require("../lib/tee.js");
var tee = new TEE(1);


module.exports = function (deployer, network, accounts) {
    var operator;

    if(network == "mainnet") {
        throw "Halt. Sanity check. Not ready for deployment to mainnet.";
    }
    else if(network == "ropsten"){
        // operator = "0x41bE05ee8D89c0Bc9cA87faC4488ad6e6A06D97E"        
    }else{ // development & test networks
        console.log("Deploying contract to network: ", network);
        operator = accounts[0];                
    }

    console.log('Deploying PostingSC to network', network, 'from', operator);
    console.log("\t --operator's address is ", operator,
                ";\n\t --PK_E_TEE is ", tee.PK_E_TEE,                                
                ";\n\t --E's PB address is ", accounts[tee.account_idx],                                
    );

    result = deployer.deploy(PostingSC, tee.PK_E_TEE, accounts[tee.account_idx], { from: operator, gas: 6 * 1000 * 1000 } // gas: 6 * 1000 * 1000
    ).then(() => {
        console.log('Deployed PostingSC with address', PostingSC.address);
        console.log("\t \\/== Default gas estimate:", PostingSC.class_defaults.gas); //class_defaults
    });
};


// NOTES
//
// var W3 = require('web3');
//
// PostingSC.deployed().then(function(instance){return instance.someFunction()});
// PostingSC.deployed().then(function(instance){return instance.someFunction.call()});
// PostingSC.deployed().then(function(instance){return instance.operator()});
// PostingSC.deployed().then(function(instance){return instance.someFunction()}).then(function(value){return value.toNumber()});

// Access migrated instance of contract
// PostingSC.deployed().then(function(instance) {console.log(instance); });
//
// Get its balance
// W3.utils.fromWei(web3.eth.getBalance('0x82d50ad3c1091866e258fd0f1a7cc9674609d254').toString(), 'ether');