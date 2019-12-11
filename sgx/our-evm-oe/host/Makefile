# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# Detect C and C++ compiler options
# if not gcc, default to clang-7

C_COMPILER=$(notdir $(CC))
ifeq ($(C_COMPILER), gcc)
        CXX_COMPILER=$(notdir $(CXX))
        USE_GCC = true
endif

ifeq ($(USE_GCC),)
        CC = clang-7
        CXX = clang++-7
        C_COMPILER=clang
        CXX_COMPILER=clang++
endif


CXX_FLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --cflags)
CXX_LDFLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --libs)
CFLAGS=$(shell pkg-config oehost-$(C_COMPILER) --cflags)

SECP256_LIB_DIR := ../common/secp256k1/.libs/

Host_Link_Flags := $(CXX_LDFLAGS) \
	-L$(SECP256_LIB_DIR) -lgmp -lsecp256k1 \
	-L./libs/intx/build/lib/intx/ -lintx
	# -L$(EEVM_LIB_DIR) -leevm

Host_Include_Paths := \
	-I./libs/intx/include/intx \
	-I../include \
	-I../common/secp256k1/include \
	-I../common/eEVM/3rdparty/intx/include -I../common/eEVM/3rdparty -I../common/eEVM/include

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	oeedger8r ../ecledger.edl --untrusted
	$(CXX) -g -c $(CXX_FLAGS) $(Host_Include_Paths) host.cpp
	$(CC) -g -c $(CFLAGS) $(CINCLUDES) $(Host_Include_Paths) ecledger_u.c
	$(CXX) -o ecledger_host ecledger_u.o host.o $(Host_Link_Flags)

clean:
	rm -f ecledger_host host.o ecledger_u.o ecledger_u.c ecledger_u.h ecledger_args.h
