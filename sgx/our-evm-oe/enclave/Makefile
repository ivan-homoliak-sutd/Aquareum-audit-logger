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
EEVM_BUILD := ../common/eEVM/build/CMakeFiles/eevm.dir

CFLAGS=$(shell pkg-config oeenclave-$(C_COMPILER) --cflags)
CXXFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --libs)
CXX_STANDARD := c++1z

OE_SDK_INCLUDE := /opt/openenclave/include/
OE_SDK_3RDPARTY_INCLUDE_CC := ${OE_SDK_INCLUDE}openenclave/3rdparty/libc/bits/
# -I${OE_SDK_3RDPARTY_INCLUDE_CC} \

Enclave_Cpp_Files := \
	enc.cpp \
	../common/signing-PB/signing.cpp \
	sealing/sealing.cpp \
	sealing/keys.cpp \
	ecl/ecl.cpp

Enclave_Include_Paths := \
	-I$(OE_SDK_INCLUDE) \
	-I. \
	-I../include \
	-I../common/secp256k1/include \
	-I../common/signing-PB \
	-I../common/eEVM/3rdparty/intx/include \
	-I../common/eEVM/3rdparty \
	-I../common/eEVM/3rdparty/aleth-mp3 \
	-I../common/eEVM/3rdparty/aleth-mp3/database \
	-I../common/eEVM/include

EEVM_OBJECTS=$(shell find $(EEVM_BUILD) | grep "\.o" | sed  ':a;N;$!ba;s/\n/ /g')

Enclave_Link_Flags := $(LDFLAGS) \
	-L$(SECP256_LIB_DIR) -lsecp256k1
	# -lgmp # IH: if secp256k1 will be compiled with gmp support, enable also this

all:
	$(MAKE) build
	$(MAKE) keys
	$(MAKE) sign

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	rm -f ../data/*
	oeedger8r ../ecledger.edl --trusted
	# oeedger8r ../ecledger.edl --trusted --trusted-dir ../common
	$(CXX) -c -Wall -Werror $(CXXFLAGS) $(INCLUDES) $(Enclave_Include_Paths)  -DOE_API_VERSION=2 $(Enclave_Cpp_Files)
	$(CC) -c -Wall -Werror $(CFLAGS) $(CINCLUDES) $(Enclave_Include_Paths) -DOE_API_VERSION=2 ecledger_t.c -o ecledger_t.o
	$(CXX) -std=$(CXX_STANDARD) -o ecledger_enc ecledger_t.o enc.o signing.o sealing.o keys.o ecl.o $(EEVM_OBJECTS) $(Enclave_Link_Flags)

sign:
	oesign sign -e ecledger_enc -c ecledger.conf -k private.pem

clean:
	rm -f enc.o ecledger_enc ecledger.signed private.pem public.pem ecl.o ecledger_t.o ecledger_t.h ecledger_t.c ecledger_args.h ../data/*

keys:
	openssl genrsa -out private.pem -3 3072
	openssl rsa -in private.pem -pubout -out public.pem
