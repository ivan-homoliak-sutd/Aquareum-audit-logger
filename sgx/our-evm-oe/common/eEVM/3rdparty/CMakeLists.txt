cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

###########################################
# Intx library
add_library(intx INTERFACE)
add_library(intx::intx ALIAS intx)

target_sources(intx INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/intx/include/intx/intx.hpp
  # ${CMAKE_CURRENT_LIST_DIR}/intx/lib/intx/div.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/intx/lib/intx/div.hpp
)
target_include_directories(intx INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/intx/include/
)

###########################################
# MP trie library from Aleth

# set(Boost_USE_STATIC_LIBS ON)
# set(Boost_USE_MULTITHREADED ON)

# hunter_add_package(Boost COMPONENTS thread)
# find_package(Boost CONFIG REQUIRED thread)

# hunter_add_package(ethash)
# find_package(ethash CONFIG REQUIRED)

add_library(mp3 INTERFACE)
add_library(aleth::mp3 ALIAS mp3)

set(ALETH_MP3_SRC
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/Common.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/Common.h
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/CommonData.h
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/Exceptions.h
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/FixedHash.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/FixedHash.h
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/RLP.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/RLP.h
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/SHA3.h
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/TrieCommon.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/TrieCommon.h
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/TrieDB.h
  ${CMAKE_CURRENT_LIST_DIR}/intx/include/intx/intx.hpp
  ${CMAKE_CURRENT_LIST_DIR}/../include/eEVM/bigint.h
  ${CMAKE_CURRENT_LIST_DIR}/keccak/KeccakHash.h
)
set(ALETH_DB_SRC
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/database/db.h
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/database/dbfwd.h
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/database/Guards.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/database/Guards.h
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/database/MemoryDB.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/database/MemoryDB.h
)

target_sources(mp3 INTERFACE
    ${ALETH_MP3_SRC}
    ${ALETH_DB_SRC}
)
target_include_directories(mp3 INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/
  ${CMAKE_CURRENT_LIST_DIR}/aleth-mp3/database/
  # ${CMAKE_CURRENT_LIST_DIR}/../include/
)


# KECCAK library - not using as lib for now
# add_library(keccaklib INTERFACE)
# add_library(keccaklib::keccaklib ALIAS keccaklib)


# SET(OE_SDK_CC_INCLUDES
#   "/opt/openenclave/include/openenclave/3rdparty/libc"
#   "/opt/openenclave/include/openenclave/3rdparty"
#   "/opt/openenclave/include"
# )
# include_directories(BEFORE "${OE_SDK_CC_INCLUDES}")



# target_include_directories(keccaklib INTERFACE
#   ${CMAKE_CURRENT_LIST_DIR}/keccak/include/
# )

# target_include_directories(keccaklib BEFORE INTERFACE
#   "/opt/openenclave/include/openenclave/3rdparty/libc/"
# )

# target_sources(keccaklib INTERFACE
#   ${CMAKE_CURRENT_LIST_DIR}/keccak/KeccakHash.h
#   ${CMAKE_CURRENT_LIST_DIR}/keccak/KeccakSpongeWidth1600.h
#   ${CMAKE_CURRENT_LIST_DIR}/keccak/KeccakSpongeWidth1600.c
#   ${CMAKE_CURRENT_LIST_DIR}/keccak/KeccakHash.c
# )
