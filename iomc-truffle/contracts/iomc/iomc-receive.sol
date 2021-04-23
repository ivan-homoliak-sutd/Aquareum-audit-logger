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
    event receiveInitialized(uint256 contractId);
    event notEnoughReserve(uint256 contractId);
    event successfulyClaimed(uint256 contractId);

    /* ----------------------------------------------------------- */
    /* ----------------------- Constructor ----------------------- */
    /* ----------------------------------------------------------- */
    constructor() public payable {
        operator = msg.sender;
    }

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

        uint256 newContractId = transfers.length;

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

        emit receiveInitialized(newContractId);

        return newContractId;
    }

    /**
     *  TODO what happend beefore calling contract in enclave
     */
    function receiveClaim(uint256 _transferId, uint256 _preimage)
        external
        contractExists(_transferId)
        hashlockMatches(_transferId, _preimage)
        usable(_transferId)
        returns (bool successful)
    {
        LockTransfers storage c = transfers[_transferId];
        if (address(this).balance < c.amount) {
            emit notEnoughReserve(_transferId);
            return false;
        } else {
            c.used = true;

            // Mint coins
            c.receiver.transfer(c.amount);

            emit successfulyClaimed(_transferId);
            return true;
        }
    }

    // Adding funds to contract by operator
    function fund() external payable returns (uint256) {
        require(msg.sender == operator, "only operator can fund contract");

        return msg.value;
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
