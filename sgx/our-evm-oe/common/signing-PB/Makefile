CXX=g++
RM=rm -f
CXX_FLAGS = -g
CXX_STANDARD = c++1z
# CPPFLAGS=-g $(shell root-config --cflags)
# LDFLAGS=-g $(shell root-config --ldflags)
# LDLIBS=-lcryptopp

SRCS=main.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

SECP256_LIB_DIR := ../common/secp256k1/.libs/
EEVM_BUILD := ../eEVM/build-host/CMakeFiles/eevm.dir

EEVM_OBJECTS=$(shell find $(EEVM_BUILD) | grep "\.o" | sed  ':a;N;$!ba;s/\n/ /g')

Host_Link_Flags := $(CXX_LDFLAGS) \
	-L$(CXX_LIB_DIR) -lstdc++ -lc \

Cpp_Files := \
	main.cpp \
	history-tree-host.cpp \
	history-tree-enc.cpp

Include_Paths := \
	-I./include \
	-I../include \
	-I../eEVM/3rdparty/intx/include \
	-I../eEVM/3rdparty \
	-I../eEVM/include

build:
	$(CXX) -c -g -Wall -Werror $(CXX_FLAGS) $(Include_Paths) -std=$(CXX_STANDARD) $(Cpp_Files)
	#@ echo "Compilers used: $(EEVM_OBJECTS)"
	$(CXX) -std=$(CXX_STANDARD) -o test main.o history-tree-enc.o history-tree-host.o  $(EEVM_OBJECTS) $(Host_Link_Flags)

run:
	./test

clean:
	$(RM) $(OBJS)
