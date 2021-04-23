# Install

Prerequisite
- [Truffle](https://github.com/trufflesuite/truffle)
- [Ganache](https://github.com/trufflesuite/ganache)

Install dependencies
```
npm install
```

# Run
Terminal 1
```
ganache-cli -p 8777 -l 250111555 -i 1234 --allowUnlimitedContractSize -a 10
```
Terminal 2
```
truffle test
```
