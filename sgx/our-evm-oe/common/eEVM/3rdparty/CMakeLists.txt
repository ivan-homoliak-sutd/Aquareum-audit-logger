cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

add_library(intx INTERFACE)
add_library(intx::intx ALIAS intx)

target_sources(intx INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/intx/include/intx/intx.hpp
  ${CMAKE_CURRENT_LIST_DIR}/intx/lib/intx/div.cpp
  ${CMAKE_CURRENT_LIST_DIR}/intx/lib/intx/div.hpp
)

target_include_directories(intx INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/intx/include/
)

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
