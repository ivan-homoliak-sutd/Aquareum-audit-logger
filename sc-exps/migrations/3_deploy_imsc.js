const imsc = artifacts.require("imsc");

module.exports = function (deployer, network, accounts) {
    deployer.deploy(imsc, accounts[1], accounts[1], accounts[2], accounts[2], accounts[3], accounts[3], { from: accounts[0] });
};
