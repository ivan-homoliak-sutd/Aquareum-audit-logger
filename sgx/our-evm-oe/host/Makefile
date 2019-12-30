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
CXX_STANDARD := c++1z
CXX_LIB_DIR := /usr/lib/x86_64-linux-gnu/
# CXX_LIB_DIR := /usr/lib/gcc/x86_64-linux-gnu/7/

SECP256_LIB_DIR := ../common/secp256k1/.libs/
EEVM_BUILD := ../common/eEVM/build-host/CMakeFiles/eevm.dir
# INTX_LIB_DIR := ./libs/intx/build/lib/intx/

EEVM_OBJECTS=$(shell find $(EEVM_BUILD) | grep "\.o" | sed  ':a;N;$!ba;s/\n/ /g')

Host_Link_Flags := $(CXX_LDFLAGS) \
	-L$(SECP256_LIB_DIR) -lgmp -lsecp256k1 \
	-L$(CXX_LIB_DIR) -lstdc++ -lc
	# -L$(INTX_LIB_DIR) -lintx

Host_Include_Paths := \
	-I. \
	-I../include \
	-I../common/secp256k1/include \
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
	oeedger8r ../ecledger.edl --untrusted
	$(CXX) -c $(CXX_FLAGS) $(Host_Include_Paths) -std=$(CXX_STANDARD) host.cpp utils.cpp ./ecl/operator.cpp ./ecl/ecl-host.cpp
	$(CC) -c $(CFLAGS) $(Host_Include_Paths) $(CINCLUDES) ecledger_u.c
	$(CXX) -std=$(CXX_STANDARD) -o ecledger_host ecledger_u.o host.o utils.o operator.o ecl-host.o $(EEVM_OBJECTS) $(Host_Link_Flags)

clean:
	rm -f ecledger_host host.o ecledger_u.o ecledger_u.c ecledger_u.h ecledger_args.h
