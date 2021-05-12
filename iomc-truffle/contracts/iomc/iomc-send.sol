pragma solidity ^0.4.23;

contract iomcSend {
    struct LockTransfer {
        address sender;
        address receiver;
        address receiverIPSC;
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
    LockTransfer[] transfers;

    /* ----------------------------------------------------------- */
    /* ------------------------- Events -------------------------- */
    /* ----------------------------------------------------------- */
    event sendInitialized(uint256 transferId);
    event sendCommited(
        uint256 transferId,
        uint256 externalTransferId,
        address receiver,
        address receiverIPSC,
        uint256 uint256amount
    );
    event sendReverted(uint256 transferId);

    /* ----------------------------------------------------------- */
    /* ------------------------ Modifiers ------------------------ */
    /* ----------------------------------------------------------- */
    modifier transferExists(uint256 _transferId) {
        require(haveTransfer(_transferId), "Transfer does not exists");
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
        address _receiverIPSC,
        uint256 _hashlock
    ) external payable returns (uint256) {
        require(msg.value > 0, "Non-zero value.");

        // uint256 _timelock = block.timestamp + 20; // now + 20 seconds - for demonstration purposes
        uint256 _timelock = block.timestamp + 60 * 60 * 24; // now + 24 hours

        uint256 newTransferId = transfers.length;

        // save to array
        transfers.push(
            LockTransfer(
                msg.sender,
                _receiver,
                _receiverIPSC,
                msg.value,
                _hashlock,
                _timelock,
                false,
                false
            )
        );

        emit sendInitialized(newTransferId);

        return newTransferId;
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
    function sendCommit(
        uint256 _transferId,
        uint256 _preimage,
        uint256 _externalTransferId
    )
        external
        transferExists(_transferId)
        hashlockMatches(_transferId, _preimage)
        usable(_transferId)
        returns (
            bool // Aquareum must have return value
        )
    {
        LockTransfer storage t = transfers[_transferId];
        t.used = true;

        // Burn coins
        address sink = address(0);
        sink.transfer(t.amount);

        emit sendCommited(
            _transferId,
            _externalTransferId,
            t.receiver,
            t.receiverIPSC,
            t.amount
        );

        return true;
    }

    function sendRevert(uint256 _transferId)
        external
        transferExists(_transferId)
        revertable(_transferId)
        returns (
            bool // Aquareum must have return value
        )
    {
        LockTransfer storage t = transfers[_transferId];

        // Return coins to the initiator
        t.sender.transfer(t.amount);
        t.reverted = true;

        emit sendReverted(_transferId);

        return true;
    }

    /* ----------------------------------------------------------- */
    /* ------------------- Internal Functions -------------------- */
    /* ----------------------------------------------------------- */
    function haveTransfer(uint256 _transferId)
        internal
        view
        returns (bool exists)
    {
        exists = (transfers[_transferId].sender != address(0));
    }
}
