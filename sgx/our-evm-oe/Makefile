# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

.PHONY: all build clean run simulate host enc

all: build

build:
	$(MAKE) -C enclave
	$(MAKE) -C host

clean:
	$(MAKE) -C enclave clean
	$(MAKE) -C host clean

run: export LD_LIBRARY_PATH=:/home/ihomoliak/Documents/SUTD/centralized-ledger-impl/sgx/our-evm-oe/common/secp256k1/.libs/:/home/ihomoliak/Documents/SUTD/centralized-ledger-impl/sgx/our-evm-oe/host/libs/intx/build/lib/intx/
run:
	host/ecledger_host ./enclave/ecledger_enc.signed

host:
	$(MAKE) -C host

enc:
	$(MAKE) -C enclave

sim: export LD_LIBRARY_PATH=:/home/ihomoliak/Documents/SUTD/centralized-ledger-impl/sgx/our-evm-oe/common/secp256k1/.libs/:/home/ihomoliak/Documents/SUTD/centralized-ledger-impl/sgx/our-evm-oe/host/libs/intx/build/lib/intx/
sim:
	host/ecledger_host ./enclave/ecledger_enc.signed --simulate

