// pragma solidity ^0.4.0;
pragma solidity ^0.5.8;

contract Kid {
  address parent;

  constructor() public
  {
    parent = msg.sender;
  }

  // function setKid (address _kid) public {
  //   require(msg.sender == owner, "Sender does not match the owner.");
  //   kid = _kid;
  //   emit KidAdjusted(kid);
  // }

  function getParent () public view returns (address){
    return parent;
  }

}
