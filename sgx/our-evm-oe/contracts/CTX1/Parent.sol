// pragma solidity ^0.4.0;
pragma solidity ^0.5.8;

import "./Kid.sol";

contract Parent {
  uint256 max_kids_count;
  address[] kids; // addresses of a spawned kid
  address owner;

  event KidCreated(address indexed from, address kid);

  constructor(uint256 kids_cnt) public
  {
    owner = msg.sender;
    max_kids_count = kids_cnt;
  }

  function getKidsCount() public view returns (uint256)
  {
    return max_kids_count;
  }

  function spawnKid() public returns (bool) {
    if(kids.length == max_kids_count){
      return false;
    }
    Kid c = new Kid();
    kids.push(address(c));
    emit KidCreated(msg.sender, address(c));
    return true;
  }

  function getkid (uint256 idx) public view returns (address){
    require(idx < kids.length, "IDX not in range");
    return kids[idx];
  }

}
