const iomcSend = artifacts.require("iomcSend");
const iomcReceive = artifacts.require("iomcReceive");

module.exports = function (deployer, network, accounts) {
  const operator = accounts[0];

  deployer.deploy(iomcSend, { from: operator });
  deployer.deploy(iomcReceive, { from: operator });
};
