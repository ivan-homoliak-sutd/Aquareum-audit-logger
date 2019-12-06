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

Enclave_Cpp_Files := enc.cpp signing-PB/signing.cpp ecl/ledger.cpp
Enclave_Include_Paths := \
	-I$(OE_SDK_INCLUDE) \
	-I. \
	-I../include \
	-I../common/secp256k1/include \
	-I../common/eEVM/3rdparty/intx/include -I../common/eEVM/3rdparty -I../common/eEVM/include

Enclave_Link_Flags := $(LDFLAGS) \
	-L$(SECP256_LIB_DIR) -lgmp -lsecp256k1 -L$(EEVM_LIB_DIR) -leevm


all:
	$(MAKE) build
	$(MAKE) keys
	$(MAKE) sign

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	oeedger8r ../helloworld.edl --trusted
	# oeedger8r ../helloworld.edl --trusted --trusted-dir ../common
	$(CXX) -g -c $(CXXFLAGS) $(INCLUDES) $(Enclave_Include_Paths) -std=c++14 -DOE_API_VERSION=2 $(Enclave_Cpp_Files)
	$(CC) -g -c $(CFLAGS) $(CINCLUDES) $(Enclave_Include_Paths) -DOE_API_VERSION=2 helloworld_t.c -o helloworld_t.o
	$(CXX) -o helloworldenc helloworld_t.o enc.o signing.o ledger.o  $(Enclave_Link_Flags)

sign:
	oesign sign -e helloworldenc -c helloworld.conf -k private.pem

clean:
	rm -f enc.o helloworldenc helloworldenc.signed private.pem public.pem helloworld_t.o helloworld_t.h helloworld_t.c helloworld_args.h

keys:
	openssl genrsa -out private.pem -3 3072
	openssl rsa -in private.pem -pubout -out public.pem
