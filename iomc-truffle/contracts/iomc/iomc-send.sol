pragma solidity ^0.4.23;

contract iomcSend {
    struct LockTransfers {
        address sender;
        address receiver;
        address receiverPbSC;
        uint256 amount;
        uint256 hashlock;
        uint256 timelock;
        bool used; // coins sended
        bool reverted; // coins refunded
    }

    /* ----------------------------------------------------------- */
    /* -------------------- Storage Variables -------------------- */
    /* ----------------------------------------------------------- */
    // Array of hash-timelock transfers
    LockTransfers[] transfers;

    /* ----------------------------------------------------------- */
    /* ------------------------- Events -------------------------- */
    /* ----------------------------------------------------------- */
    event sendInitialized(uint256 contractId);
    event sendCommited(uint256 contractId);
    event sendReverted(uint256 contractId);

    /* ----------------------------------------------------------- */
    /* ------------------------ Modifiers ------------------------ */
    /* ----------------------------------------------------------- */
    modifier contractExists(uint256 _transferId) {
        require(haveContract(_transferId), "Contract does not exists");
        _;
    }

    modifier hashlockMatches(uint256 _transferId, uint256 _preimage) {
        require(
            transfers[_transferId].hashlock ==
                uint256(keccak256(abi.encodePacked(_preimage))),
            "hashlock hash does not match"
        );
        _;
    }

    modifier usable(uint256 _transferId) {
        require(transfers[_transferId].used == false, "usable: already used");
        require(
            transfers[_transferId].timelock > block.timestamp,
            "usable: already after timelock"
        );
        _;
    }

    modifier revertable(uint256 _transferId) {
        require(
            transfers[_transferId].used == false,
            "revertable: already used"
        );
        require(
            transfers[_transferId].reverted == false,
            "revertable: already reverted"
        );
        require(
            transfers[_transferId].timelock <= block.timestamp,
            "revertable: to soon to revert, wait for timelock"
        );
        _;
    }

    /* ----------------------------------------------------------- */
    /* ------------------- External Functions -------------------- */
    /* ----------------------------------------------------------- */
    function sendInitialize(
        address _receiver,
        address _receiverPbSC,
        uint256 _hashlock
    ) external payable returns (uint256) {
        require(msg.value > 0, "Non-zero value.");

        uint256 _timelock = block.timestamp + 20; // now + 20 seconds
        // uint256 _timelock = block.timestamp + 60 * 5; // now + 5 minuites

        uint256 newContractId = transfers.length;

        // save to array
        transfers.push(
            LockTransfers(
                msg.sender,
                _receiver,
                _receiverPbSC,
                msg.value,
                _hashlock,
                _timelock,
                false,
                false
            )
        );

        emit sendInitialized(newContractId);

        return newContractId;
    }

    /**
     * Argument sending to enclave but not signed by sender
     *  - tx2 = tx receiveInit() of external client
     *  - incremental proof with LRoot, LRootPb (need to check with light client in enclave) 
     *  - membership proof with blk.header
     *  - merkle proof with receipt of tx2
     * 
     *  Before call this contract enclave need to check validity of proofs
     */
    function sendCommit(uint256 _transferId, uint256 _preimage)
        external
        contractExists(_transferId)
        hashlockMatches(_transferId, _preimage)
        usable(_transferId)
        returns (bool) // Aquareum must have return value
    {
        LockTransfers storage c = transfers[_transferId];
        c.used = true;

        // Burn coins
        address sink = address(0x0);
        sink.transfer(c.amount);

        emit sendCommited(_transferId);

        return true;
    }

    function sendRevert(uint256 _transferId)
        external
        contractExists(_transferId)
        revertable(_transferId)
        returns (bool) // Aquareum must have return value
    {
        LockTransfers storage c = transfers[_transferId];

        // Return coins to the initiator
        c.sender.transfer(c.amount);
        c.reverted = true;

        emit sendReverted(_transferId);
  
        return true;
    }

    /* ----------------------------------------------------------- */
    /* ------------------- Internal Functions -------------------- */
    /* ----------------------------------------------------------- */
    function haveContract(uint256 _transferId)
        internal
        view
        returns (bool exists)
    {
        exists = (transfers[_transferId].sender != address(0));
    }
}
