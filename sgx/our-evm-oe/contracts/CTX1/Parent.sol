// pragma solidity ^0.4.0;
pragma solidity ^0.5.8;

import "./Kid.sol";

contract Parent {
  uint256 kids_count;
  address[] kids; // addresses of a spawned kid
  address owner;


// event KidCreated(string message, address indexed kid);

  constructor(uint256 kids_cnt) public
  {
    owner = msg.sender;
    kids_count = kids_cnt;
  }

  function getKidsCount() public view returns (uint256)
  {
    return kids_count;
  }

  function spawnKids() public {
    Kid c = new Kid();
    kids.push(address(c));
    // emit KidCreated("spawnKid successfull aaaaaa",address(c));
  }

  function getkid (uint256 idx) public view returns (address){
    require(idx < kids.length, "IDX not in range");
    return kids[idx];
  }

}
