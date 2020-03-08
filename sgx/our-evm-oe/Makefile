# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

.PHONY: all build clean run simulate host enc


RepoDir := /home/ihomoliak/Documents/centralized-ledger-impl/

all: build

build:
	$(MAKE) -C enclave
	$(MAKE) -C host

clean:
	$(MAKE) -C enclave clean
	$(MAKE) -C host clean

run: export LD_LIBRARY_PATH=:$(RepoDir)/sgx/our-evm-oe/common/secp256k1/.libs/:$(RepoDir)/sgx/our-evm-oe/host/libs/intx/build/lib/intx/
run:
	host/ecledger_host ./enclave/ecledger_enc.signed

host:
	$(MAKE) -C host

enc:
	$(MAKE) -C enclave


debug: export LD_LIBRARY_PATH=:$(RepoDir)/sgx/our-evm-oe/common/secp256k1/.libs/:$(RepoDir)/sgx/our-evm-oe/host/libs/intx/build/lib/intx/
debug:
	valgrind host/ecledger_host ./enclave/ecledger_enc.signed


sim: export LD_LIBRARY_PATH=:$(RepoDir)/sgx/our-evm-oe/common/secp256k1/.libs/:$(RepoDir)/sgx/our-evm-oe/host/libs/intx/build/lib/intx/
sim:
	host/ecledger_host ./enclave/ecledger_enc.signed --simulate

