
compilation by solidity newer than 0.4.20

solc --evm-version homestead --combined-json bin,hashes --pretty-json --optimize ERC20.sol > ERC20_combined.json 


Then, it is necessary to add parameters for constructor of contract into its definition file *.json

    E.g., for ERC20 contract:
    
    "ctor": [
        {
          "name": "totalSupply",
          "type": "uint256",
          "value": "1023"
       }
      ]
