# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# Detect C and C++ compiler options
# if not gcc and g++, default to clang-7
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


SECP256_LIB_DIR := ../common/secp256k1/.libs/
EEVM_LIB_DIR := ../common/eEVM/build/

CFLAGS=$(shell pkg-config oeenclave-$(COMPILER) --cflags)
CXXFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --libs)

OE_SDK_INCLUDE := /opt/openenclave/include/
OE_SDK_3RDPARTY_INCLUDE_CC := ${OE_SDK_INCLUDE}openenclave/3rdparty/libc/bits/
# -I${OE_SDK_3RDPARTY_INCLUDE_CC} \

Enclave_Cpp_Files := \
	enc.cpp \
	signing-PB/signing.cpp \
	sealing/sealing.cpp \
	ecl/ecl.cpp

Enclave_Include_Paths := \
	-I$(OE_SDK_INCLUDE) \
	-I. \
	-I../include \
	-I../common/secp256k1/include \
	-I../common/eEVM/3rdparty/intx/include -I../common/eEVM/3rdparty -I../common/eEVM/include

Enclave_Link_Flags := $(LDFLAGS) \
	-L$(SECP256_LIB_DIR) -lgmp -lsecp256k1 \
	-L$(EEVM_LIB_DIR) -leevm


all:
	$(MAKE) build
	$(MAKE) keys
	$(MAKE) sign

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	oeedger8r ../ecledger.edl --trusted
	# oeedger8r ../ecledger.edl --trusted --trusted-dir ../common
	$(CXX) -g -c $(CXXFLAGS) $(INCLUDES) $(Enclave_Include_Paths) -std=c++17 -DOE_API_VERSION=2 $(Enclave_Cpp_Files)
	$(CC) -g -c $(CFLAGS) $(CINCLUDES) $(Enclave_Include_Paths) -DOE_API_VERSION=2 ecledger_t.c -o ecledger_t.o
	$(CXX) -o ecledger_enc ecledger_t.o enc.o signing.o ecl.o $(Enclave_Link_Flags)

sign:
	oesign sign -e ecledger_enc -c ecledger.conf -k private.pem

clean:
	rm -f enc.o ecledger_enc ecledger.signed private.pem public.pem ecl.o ecledger_t.o ecledger_t.h ecledger_t.c ecledger_args.h

keys:
	openssl genrsa -out private.pem -3 3072
	openssl rsa -in private.pem -pubout -out public.pem
