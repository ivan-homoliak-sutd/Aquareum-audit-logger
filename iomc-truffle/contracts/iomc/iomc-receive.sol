pragma solidity ^0.4.23;

contract iomcReceive {
    struct LockTransfers {
        address sender;
        address senderPbSC;
        address receiver;
        uint256 amount;
        uint256 hashlock;
        bool used; // coins received
    }

    /* ----------------------------------------------------------- */
    /* -------------------- Storage Variables -------------------- */
    /* ----------------------------------------------------------- */
    // Operator - creator of contract
    address public operator;

    // Array of hashlock transfers
    LockTransfers[] transfers;

    /* ----------------------------------------------------------- */
    /* ------------------------- Events -------------------------- */
    /* ----------------------------------------------------------- */
    event receiveInitialized(uint256 transferId);
    event notEnoughReserve(uint256 transferId);
    event successfulyClaimed(uint256 transferId);
    event funded();

    /* ----------------------------------------------------------- */
    /* ----------------------- Constructor ----------------------- */
    /* ----------------------------------------------------------- */
    constructor() public payable {
        operator = msg.sender;
    }

    /* ----------------------------------------------------------- */
    /* ------------------------ Modifiers ------------------------ */
    /* ----------------------------------------------------------- */
    modifier transferExists(uint256 _transferId) {
        require(haveTransfer(_transferId), "Contract does not exists");
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
        _;
    }

    /* ----------------------------------------------------------- */
    /* ------------------- External Functions -------------------- */
    /* ----------------------------------------------------------- */
    function receiveInitialize(
        address _sender,
        address _senderPbSC,
        uint256 _hashlock,
        uint256 _amount
    ) external returns (uint256) {
        require(_amount > 0, "Non-zero value.");

        uint256 newTransferId = transfers.length;

        // save to array
        transfers.push(
            LockTransfers(
                _sender,
                _senderPbSC,
                msg.sender,
                _amount,
                _hashlock,
                false
            )
        );

        emit receiveInitialized(newTransferId);

        return newTransferId;
    }

    /**
     * Argument sending to enclave but not signed by sender
     *  - h(_preimage) == hashlock
     *  - tx3 = tx sendCommit() of external client
     *  - tx3.rcp.externalTransferId == _transferId
     *  - incremental proof with LRoot, LRootPb (need to check with light client in enclave) 
     *  - membership proof with blk.header
     *  - merkle proof with receipt of tx3
     * 
     *  Before call this contract enclave need to check validity of proofs
     */
    function receiveClaim(uint256 _transferId, uint256 _preimage)
        external
        transferExists(_transferId)
        hashlockMatches(_transferId, _preimage)
        usable(_transferId)
        returns (bool successful)
    {
        LockTransfers storage t = transfers[_transferId];
        if (address(this).balance < t.amount) {
            emit notEnoughReserve(_transferId);
            return false;
        } else {
            t.used = true;

            // Mint coins
            t.receiver.transfer(t.amount);

            emit successfulyClaimed(_transferId);
            return true;
        }
    }

    // Adding funds to contract by operator
    function fund() external payable returns (uint256) {
        require(msg.sender == operator, "only operator can fund contract");

        emit funded();
        return msg.value;
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
