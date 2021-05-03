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
CXX_STANDARD := c++1z
CXX_LIB_DIR := /usr/lib/x86_64-linux-gnu/

OE_SDK_INCLUDE := /opt/openenclave/include/
SECP256_LIB_DIR := ../common/secp256k1/.libs/
EEVM_BUILD := ../common/eEVM/build-host/CMakeFiles/eevm.dir

EEVM_OBJECTS=$(shell find $(EEVM_BUILD) | grep "\.o" | sed  ':a;N;$!ba;s/\n/ /g')

Host_Link_Flags := $(CXX_LDFLAGS) \
	-L$(SECP256_LIB_DIR) -lsecp256k1 -lssl -lcrypto \
	-L$(CXX_LIB_DIR) -lstdc++ -lc

Host_Cpp_Files := \
	client.cpp \
	net.cpp \
	helper.cpp \
	../host/utils.cpp \
	../common/signing-PB/signing.cpp 

Host_Include_Paths := \
	-I. \
	-I../include \
	-I../common/secp256k1/include \
	-I../common/signing-PB \
	-I../common/eEVM/3rdparty \
	-I../common/eEVM/3rdparty/intx/include \
	-I../common/eEVM/include \
	-I../common/eEVM/3rdparty/aleth-mp3 \
	-I../common/eEVM/3rdparty/aleth-mp3/database \
	-I../common/eEVM/include \
	-I/usr/include/c++/7

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	$(CXX) -c -Wall -Werror $(CXX_FLAGS) $(Host_Include_Paths) -std=$(CXX_STANDARD) $(Host_Cpp_Files)
	$(CXX) -std=$(CXX_STANDARD) -o aqledger_client client.o net.o utils.o helper.o signing.o $(EEVM_OBJECTS) $(Host_Link_Flags)

clean:
	rm -f aqledger_client *.o
