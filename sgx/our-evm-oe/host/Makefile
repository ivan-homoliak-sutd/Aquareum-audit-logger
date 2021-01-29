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
        CC = gcc
        CXX = g++
        C_COMPILER = gcc
        CXX_COMPILER = g++
endif


CXX_FLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --cflags)
CXX_LDFLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --libs)
CFLAGS=$(shell pkg-config oehost-$(C_COMPILER) --cflags)
CXX_STANDARD := c++1z
CXX_LIB_DIR := /usr/lib/x86_64-linux-gnu/

OE_SDK_INCLUDE := $(shell pkg-config oehost-$(C_COMPILER) --variable=includedir)
SECP256_LIB_DIR := ../common/secp256k1/.libs/
EEVM_BUILD := ../common/eEVM/build-host/CMakeFiles/eevm.dir
# INTX_LIB_DIR := ./libs/intx/build/lib/intx/

EEVM_OBJECTS=$(shell find $(EEVM_BUILD) | grep "\.o" | sed  ':a;N;$!ba;s/\n/ /g')

Host_Link_Flags := $(CXX_LDFLAGS) \
	-L$(SECP256_LIB_DIR) -lsecp256k1 -lssl -lcrypto \
	-L$(CXX_LIB_DIR) -lstdc++ -lc
	# -L$(INTX_LIB_DIR) -lintx

Host_Cpp_Files := \
	utils.cpp \
	./ledger/operator.cpp \
	./ledger/ledger-host.cpp \
	../common/signing-PB/signing.cpp \	
	host.cpp

Host_Include_Paths := \
	-I. \
	-I../include \
	-I../common/secp256k1/include \
	-I../common/signing-PB/include \
	-I../common/eEVM/3rdparty \
	-I../common/eEVM/3rdparty/intx/include \
	-I../common/eEVM/include \
	-I../common/eEVM/3rdparty/aleth-mp3 \
	-I../common/eEVM/3rdparty/aleth-mp3/database \
	-I../common/eEVM/include \
	-I/usr/include/c++/7
	# -I./libs/intx/include \

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	rm -f ../data/*
	oeedger8r ../aqledger.edl --untrusted \
			--search-path $(OE_SDK_INCLUDE) \
			--search-path $(OE_SDK_INCLUDE)/openenclave/edl/sgx
	$(CXX) -c -Wall -Werror $(MAIN_CXX_FLAGS) $(CXX_FLAGS) $(Host_Include_Paths) -std=$(CXX_STANDARD) $(Host_Cpp_Files)
	$(CC) -c -Wall -Werror $(MAIN_CXX_FLAGS) $(CFLAGS) $(Host_Include_Paths) $(CINCLUDES) aqledger_u.c
	$(CXX) -std=$(CXX_STANDARD) -o aqledger_host aqledger_u.o host.o utils.o operator.o ledger-host.o signing.o $(EEVM_OBJECTS) $(Host_Link_Flags)

clean:
	rm -f aqledger_host *.o aqledger_u.c aqledger_u.h aqledger_args.h
