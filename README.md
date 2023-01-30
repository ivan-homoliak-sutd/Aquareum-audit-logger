
# Instalation of Aquareum (AQ) ledger

## SGX ENCLAVE (trusted computig part)
    0) Build and install open enclave into /opt/openenclave
        -https://github.com/openenclave/openenclave/blob/master/docs/GettingStartedDocs/Contributors/SGX1GettingStarted.md
        -https://github.com/openenclave/openenclave/blob/master/docs/GettingStartedDocs/Contributors/LinuxInstallInfo.md

    1) Build ./common/ stuff:
        a) secp256k1,
             $ sudo apt-get install build-essential
             $ sudo apt-get install libltdl7 libtool
            ./autogen.sh
            ./configure --with-bignum=no  --enable-module-recovery
            make
            make check

        b) eEVM (cmake from folder build-host builds eEVM for host and from folder 'build' builds eEVM for enlcave)
            $ mkdir build
            $ mkdir build-host
            $ cd build
            $ cmake ..
            -- note that once in a while (especially for linker not-found errors) rerun pkg-config in Cmake file and update flags of that file


    2) Build host and enclave
        a) UBUNTU dependecies for AQ ledger:
            sudo apt-get install libssl-dev libboost-dev

        b) Set up 'RepoDir' variable in Makefile:
            RepoDir := {INSTALL_DIR}/centralized-ledger-impl/

        c) Build AQ ledger
            . /opt/openenclave/share/openenclave/openenclaverc   
	    # or  . ~/openenclave-install/share/openenclave/openenclaverc
            $ make

    3) Run AQ ledger
        $ make run


## Smart contract part
  TODO

## Client SW
  TODO



## Notes

-------- 
/opt/openenclave/include/openenclave/3rdparty/libcxx/__config:11:8: error: expected identifier or ‘(’ before string constant
   11 | extern "C" long long strtoll_l(
      |        ^~~
/opt/openenclave/include/openenclave/3rdparty/libcxx/__config:14:8: error: expected identifier or ‘(’ before string constant
   14 | extern "C" unsigned long long int strtoull_l(
      |        ^~~
/opt/openenclave/include/openenclave/3rdparty/libcxx/__config:17:8: error: expected identifier or ‘(’ before string constant
   17 | extern "C" unsigned int arc4random(void);
      |        ^~~
Solution: 
In file /opt/openenclave/include/openenclave/3rdparty/libcxx/__config, add these:
\#ifdef __cplusplus  
extern "C" 
\#endif 


https://github.com/openenclave/openenclave/issues/2054

-------- 
We have SGX1+FLC on pchomoliak2 since it has i5-10400 @ 3.4Gz with 12M cache
> oesgx
The output of oeasgx should be as follows (if not, ENABLE it in BIOS - SW-controlled is not OK)
CPU supports SGX_FLC:Flexible Launch Control
CPU supports Software Guard Extensions:SGX1
MaxEnclaveSize_64: 2^(36)
CPU supports Key Sharing & Separation (KSS): false
EPC size on the platform: 98041856

This means that we can follow this - https://github.com/openenclave/openenclave/blob/master/docs/GettingStartedDocs/install_oe_sdk-Ubuntu_20.04.md
- Although I compiled my own open-enclave and installed it to /opt/openenclave, enabling us to make modifications to its codebased