# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

.PHONY: all build clean run simulate host enc

OE_CRYPTO_LIB := mbedtls
export OE_CRYPTO_LIB

RepoDir := $(shell git rev-parse --show-toplevel)
SubPath := our-evm-oe

# MAIN_CXX_FLAGS := -DNDEBUG
MAIN_CXX_FLAGS := -DASSERTS_ENABLES
export MAIN_CXX_FLAGS

all: build

build:
	$(MAKE) -C enclave
	$(MAKE) -C host

clean:
	$(MAKE) -C enclave clean
	$(MAKE) -C host clean

run: export LD_LIBRARY_PATH=:$(RepoDir)/sgx/$(SubPath)/common/secp256k1/.libs/:$(RepoDir)/sgx/$(SubPath)/host/libs/intx/build/lib/intx/
run:
	mkdir -p ./data
	host/aqledger_host ./enclave/aqledger_enc.signed

host:
	$(MAKE) -C host

enc:
	$(MAKE) -C enclave


debug: export LD_LIBRARY_PATH=:$(RepoDir)/sgx/$(SubPath)/common/secp256k1/.libs/:$(RepoDir)/sgx/$(SubPath)/host/libs/intx/build/lib/intx/
debug:
	valgrind host/aqledger_host ./enclave/aqledger_enc.signed


sim: export LD_LIBRARY_PATH=:$(RepoDir)/sgx/$(SubPath)/common/secp256k1/.libs/:$(RepoDir)/sgx/$(SubPath)/host/libs/intx/build/lib/intx/
sim:
	host/aqledger_host ./enclave/aqledger_enc.signed --simulate

