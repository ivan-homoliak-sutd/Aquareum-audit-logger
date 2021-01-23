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
        CC = gcc
        CXX = g++
        C_COMPILER = gcc
        CXX_COMPILER = g++
endif

CRYPTO_LDFLAGS := $(shell pkg-config oeenclave-$(CXX_COMPILER) --variable=${OE_CRYPTO_LIB}libs)

ifeq ($(LVI_MITIGATION), ControlFlow)
        ifeq ($(LVI_MITIGATION_BINDIR),)
                $(error LVI_MITIGATION_BINDIR is not set)
        endif
        # Only run once.
        ifeq (,$(findstring $(LVI_MITIGATION_BINDIR),$(CC)))
                CC := $(LVI_MITIGATION_BINDIR)/$(CC)
        endif
        COMPILER := $(COMPILER)-lvi-cfg
        CRYPTO_LDFLAGS := $(shell pkg-config oeenclave-$(COMPILER) --variable=${OE_CRYPTO_LIB}libslvicfg)
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
	ledger/aq_ledger.cpp

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

# lcxxrt is for stacktrace.h
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
	oeedger8r ../aqledger.edl --trusted --search-path $(OE_SDK_INCLUDE)
	# oeedger8r ../aqledger.edl --trusted --trusted-dir ../common
	$(CXX) -c -Wall -Werror $(CXXFLAGS) $(INCLUDES) $(Enclave_Include_Paths) -DENCLAVE_BUILD -DOE_API_VERSION=2 $(Enclave_Cpp_Files)
	$(CC) -c -Wall -Werror $(CFLAGS) $(CINCLUDES) $(Enclave_Include_Paths) -DOE_API_VERSION=2 aqledger_t.c -o aqledger_t.o
	$(CXX) -std=$(CXX_STANDARD) -o aqledger_enc aqledger_t.o enc.o signing.o sealing.o keys.o aq_ledger.o $(EEVM_OBJECTS) $(Enclave_Link_Flags) $(CRYPTO_LDFLAGS)

sign:
	oesign sign -e aqledger_enc -c aqledger.conf -k private.pem

clean:
	rm -f *.o aqledger_enc aqledger.signed private.pem public.pem aqledger_t.h aqledger_t.c aqledger_args.h ../data/*

keys:
	openssl genrsa -out private.pem -3 3072
	openssl rsa -in private.pem -pubout -out public.pem
