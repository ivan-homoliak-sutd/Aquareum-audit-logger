pragma solidity ^0.5.16;

contract imsc {
    struct InstanceInfo {
        address operator;
        bool joined;
        uint256 approvedCount;
        mapping(address => bool) approved;
    }

    /* ----------------------------------------------------------- */
    /* -------------------- Storage Variables -------------------- */
    /* ----------------------------------------------------------- */
    // Array of addresses of instances that supports interoperability
    mapping(address => InstanceInfo) instances;
    uint256 public instancesCount;

    /* ----------------------------------------------------------- */
    /* ------------------------- Events -------------------------- */
    /* ----------------------------------------------------------- */
    event joinRequested(address);
    event joined(address);

    /* ----------------------------------------------------------- */
    /* ----------------------- Constructor ----------------------- */
    /* ----------------------------------------------------------- */
    constructor(
        address _ipsc1,
        address operator1,
        address _ipsc2,
        address operator2,
        address _ipsc3,
        address operator3
    ) public {
        instances[_ipsc1] = InstanceInfo(operator1, true, 0);
        instances[_ipsc2] = InstanceInfo(operator2, true, 0);
        instances[_ipsc3] = InstanceInfo(operator3, true, 0);
        instancesCount = 3;
    }

    /* ----------------------------------------------------------- */
    /* ------------------------ Modifiers ------------------------ */
    /* ----------------------------------------------------------- */
    modifier _notRequested(address _ipsc) {
        require(instances[_ipsc].operator == address(0), "Already requested");
        _;
    }

    modifier _requested(address _ipsc) {
        require(instances[_ipsc].operator != address(0), "Not yet requested");
        _;
    }

    modifier _notJoined(address _ipsc) {
        require(
            (instances[_ipsc].operator == address(0) ||
                instances[_ipsc].joined == false),
            "Already joined"
        );
        _;
    }

    modifier _isJoined(address _operator, address _ipsc) {
        require(instances[_ipsc].operator == _operator, "Not even requested");
        require(instances[_ipsc].joined == true, "Not joined");
        _;
    }

    /* ----------------------------------------------------------- */
    /* ------------------- External Functions -------------------- */
    /* ----------------------------------------------------------- */
    function joinRequest(address _ipsc) external _notRequested(_ipsc) {
        // external check if address of operator is in ipsc address

        instances[_ipsc] = InstanceInfo(msg.sender, false, 0);

        emit joinRequested(_ipsc);
    }

    function approveRequest(address _myIpsc, address _approvingIpsc)
        external
        _isJoined(msg.sender, _myIpsc)
        _requested(_approvingIpsc)
    {
        InstanceInfo storage r = instances[_approvingIpsc];
        r.approved[msg.sender] = true;
        r.approvedCount++;
        if ((r.joined == false) && (r.approvedCount > instancesCount / 2)) {
            // approved instance
            r.joined = true;
            instancesCount++;

            emit joined(_approvingIpsc);
        }
    }

    function isJoined(address _ipsc) external view returns (bool) {
        return ((instances[_ipsc].operator != address(0)) &&
            (instances[_ipsc].joined == true));
    }
}
