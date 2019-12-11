# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# Detect C and C++ compiler options
# if not gcc, default to clang-7

COMPILER=$(notdir $(CC))
ifeq ($(COMPILER), gcc)
        USE_GCC = true
endif
ifeq ($(USE_GCC),)
        CC = clang-7
        COMPILER=clang
endif

CFLAGS=$(shell pkg-config oehost-$(COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oehost-$(COMPILER) --libs)

SECP256_LIB_DIR := ../common/secp256k1/.libs/

Host_Link_Flags := $(LDFLAGS) \
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
	oeedger8r ../helloworld.edl --untrusted
	$(CC) -g -c $(CFLAGS) $(Host_Include_Paths) host.c
	$(CC) -g -c $(CFLAGS) $(Host_Include_Paths) helloworld_u.c
	$(CC) -o helloworldhost helloworld_u.o host.o $(Host_Link_Flags)

clean:
	rm -f helloworldhost host.o helloworld_u.o helloworld_u.c helloworld_u.h helloworld_args.h
