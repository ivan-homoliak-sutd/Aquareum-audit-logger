pragma solidity >=0.5.8 <0.6.0;

contract PostingSC {
  address public PK_O; // address of operator O
  address[] public PK_E_PB;
  bytes32[] public PK_E_TEE; // TODO: later change type to fit the size of PK in Sigma_TEE

  bytes32 public LRoot_PB;

  TxInfo[] public censTXs;

   struct TxInfo {
        bytes trx;
        string status;
   }

  ///////////// Events for Client ////////////////
  event RootUpdated(bytes32 root_A, bytes32 root_B);
  // event MsgToRecover(bytes message);

  ///////////// Modifiers ////////////////
  modifier verifySigEncPB_native() {
      // Verify signature made by E (using native ETH method)
      require(msg.sender == PK_E_PB[PK_E_PB.length - 1], "Signature made by SK_E_PB is invalid");
      _;
  }
  modifier verifySigEncPB_explicit(bytes memory data, uint8 sig_v, bytes32 sig_r, bytes32 sig_s) {
      // Verify signature made by E (using explicit method)
      // emit MsgToRecover(data);

      require(
          _validSignature(data, PK_E_PB[PK_E_PB.length - 1], sig_v, sig_r, sig_s),
          "Ecrecover: signature of E (under Sigma.PB) is invalid"
      );
      _;
  }
  modifier verifySigOperator() {
      require(msg.sender == PK_O, "Signature made by SK_O_PB is invalid");
      _;
  }


  ///////////// Transaction-Based Methods ////////////////

  constructor(bytes32 _PK_E_TEE, address _PK_E_PB) public {
    PK_O = msg.sender;
    PK_E_PB.push(_PK_E_PB);
    PK_E_TEE.push(_PK_E_TEE);
  }

  function postLRoot(bytes32 root_A, bytes32 root_B) public
    verifySigEncPB_native()
  {
    if(LRoot_PB == root_A){
      LRoot_PB = root_B; // Verify whether a log version transition extends the last one.
    }
    emit RootUpdated(root_A, root_B);
  }

  function updatePKsTEE(bytes32 _PK_E_TEE, address _PK_E_PB) public
    verifySigOperator()
  {
    PK_E_PB.push(_PK_E_PB);
    PK_E_TEE.push(_PK_E_TEE); // Clients watch it and make new remote attestation.
  }

  /**
   * This method is called by the clients. Clients must first prove that they are legitimate users of the service.
   * For this purpose, we use subscription tickets that are bounded to PK of the clients and datetime of the subscription expiration, signed by E.
   * Hence, we need to check signature made by E on a ticket using non-native verfication, while native signature verification can be used for
   * checking the signature of the current transaction.
   *
   * ticket is abi.encoded (packed) and contains (sender_address, expiration_timestamp}
   */
  function submitCensTx(bytes memory trx, bytes memory ticket, uint8 sig_v, bytes32 sig_r, bytes32 sig_s) public
    verifySigEncPB_explicit(ticket, sig_v, sig_r, sig_s)
  {
    (address subscriber, uint expire_time) = abi.decode(ticket, (address, uint));
    require(msg.sender == subscriber, "Signature made by sender of the message does not correspond to the ticket.");
    require(block.timestamp < expire_time, "Subscription ticket is already expired.");

    TxInfo memory ti = TxInfo(trx, "");
    censTXs.push(ti);
  }

  /**
   * The function is called by operator to prove that a censored transaction was processed.
   */
  function resolveCensTx(uint idx, bytes32 htrx, string memory status) public
    verifySigEncPB_native()
  {
    require(idx < censTXs.length, "Idx of censored TX is out of range.");
    TxInfo storage ti = censTXs[idx];

    require(htrx == keccak256(ti.trx), "Tx hash of submited proof is invalid.");
    ti.status = status; // Update the status from the E. It might be ERROR or INCLUDED.
  }


  ///////////// Call-Based Methods (not modifying the state) ////////////////

  function _validSignature(bytes memory data, address PK, uint8 sig_v, bytes32 sig_r, bytes32 sig_s) private pure returns (bool) {
        bytes32 message = _messageToRecover(data);
        if (PK == ecrecover(message, sig_v, sig_r, sig_s)){
          return true;
        }else{
          return false;
        }
    }

  function _messageToRecover(bytes memory message) private pure returns (bytes32) {
        bytes32 hashedMessage = keccak256(abi.encodePacked(message));
        bytes memory prefix = "\x19Ethereum Signed Message:\n32";
        return keccak256(abi.encodePacked(prefix, hashedMessage));
  }

  // function _messageToRecover(bytes memory message) private pure returns (bytes32) {
  //       bytes memory messageBytes = _msgToAscii(message);
  //       bytes memory prefix = "\x19Ethereum Signed Message:\n";
  //       string memory length = uint2str(messageBytes.length);
  //       return keccak256(abi.encode(prefix, length, messageBytes));
  // }

  // Construct the byte representation of the ascii-encoded hashed message written in hex.
  function _hashToAscii(bytes32 hash) private pure returns (bytes memory) {
      bytes memory s = new bytes(64);
      for (uint i = 0; i < 32; i++) {
        byte b = hash[i];
        byte hi = byte(uint8(b) / 16);
        byte lo = byte(uint8(b) - 16 * uint8(hi));
        s[2*i] = _char(hi);
        s[2*i+1] = _char(lo);
      }
      return s;
  }

  function _msgToAscii(bytes memory message) private pure returns (bytes memory) {
      bytes memory s = new bytes(64);
      for (uint i = 0; i < message.length; i++) {
        byte b = message[i];
        byte hi = byte(uint8(b) / 16);
        byte lo = byte(uint8(b) - 16 * uint8(hi));
        s[2*i] = _char(hi);
        s[2*i+1] = _char(lo);
      }
      return s;
  }

  // Convert from byte to ASCII of 0-f  // http://www.unicode.org/charts/PDF/U0000.pdf
  function _char(byte b) private pure returns (byte c) {
    if (uint8(b) < uint8(10))
        return byte(uint8(b) + 0x30);
    else
        return byte(uint8(b) + 0x57);
  }

  function getCntOfCensTxs() public view returns (uint) {
        return censTXs.length;
    }
}
