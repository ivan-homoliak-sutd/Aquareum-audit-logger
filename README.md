
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
            -- note that once in a while (especialy for linker not-found errors) rerun pkg-config in Cmake file and update flags of that file


    2) Build host and enclave
        a) UBUNTU dependecies for AQ ledger:
            sudo apt-get install libssl-dev
            sudo apt-get install libboost-dev

        b) Set up 'RepoDir' variable in Makefile:
            RepoDir := {INSTALL_DIR}/centralized-ledger-impl/

        c) Build AQ ledger
            . /opt/openenclave/share/openenclave/openenclaverc
            $ make

    3) Run AQ ledger
        $ make run


## Smart contract part
  TODO

## Client SW
  TODO