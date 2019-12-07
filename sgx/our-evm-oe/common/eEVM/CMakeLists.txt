cmake_minimum_required(VERSION 3.10)

project(eevm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE on)

# Obtanined by: $ pkg-config oeenclave-g++ --libs
SET(CXX_ENCLAVE_LINK_FLAGS
  "-nostdlib -nodefaultlibs -nostartfiles -Wl,--no-undefined -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--export-dynamic -Wl,-pie -Wl,--build-id -Wl,-z,noexecstack -Wl,-z,now"
)
# Obtanined by: $ pkg-config oeenclave-g++ --cflags
SET(CXX_ENCLAVE_COMPILE_FLAGS
  "-nostdinc -m64 -fPIE -ftls-model=local-exec -fvisibility=hidden -fno-stack-protector"
)
SET(CMAKE_CXX_FLAGS "${CXX_ENCLAVE_COMPILE_FLAGS}")



SET(CMAKE_EXE_LINKER_FLAGS  "${CXX_ENCLAVE_LINK_FLAGS}")
# SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${CXX_ENCLAVE_LINK_FLAGS}")

# SET(OE_SDK_CC_INCLUDES
#   "/opt/openenclave/include/openenclave/3rdparty/libc"
#   "/opt/openenclave/include/openenclave/3rdparty"
#   "/opt/openenclave/include"
# )

SET(OE_SDK_CXX_INCLUDES
  "/opt/openenclave/include/openenclave/3rdparty/libcxx"
  "/opt/openenclave/include/openenclave/3rdparty/libc"
  "/opt/openenclave/include/openenclave/3rdparty"
  "/opt/openenclave/include"
)

include_directories("${OE_SDK_CXX_INCLUDES}")

if(MSVC)
  # Set Windows compiler options.
  add_compile_options(/W3 /std:c++latest)
else()
  # Set Linux compiler options
  # add_compile_options(-Wall -Werror)

  add_compile_definitions(OE_API_VERSION=2)

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    separate_arguments(COVERAGE_FLAGS UNIX_COMMAND "--coverage -fprofile-arcs -ftest-coverage")
    set(PLATFORM_SPECIFIC_TEST_LIBS "gcov")
  else()
    separate_arguments(COVERAGE_FLAGS UNIX_COMMAND "-fprofile-instr-generate -fcoverage-mapping")
    set(PLATFORM_SPECIFIC_TEST_LIBS "-fprofile-instr-generate")
  endif()
endif()

file(GLOB KECCAK_SOURCES
  3rdparty/keccak/*.c
)

add_subdirectory(3rdparty)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Options
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
option(RECORD_TRACE "Record a detailed trace of EVM execution during test runs" OFF)
if(RECORD_TRACE)
  add_definitions(-DRECORD_TRACE)
endif(RECORD_TRACE)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Common variables
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
set(EEVM_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(EEVM_CORE_SRCS
  src/disassembler.cpp
  src/processor.cpp
  src/stack.cpp
  src/transaction.cpp
  src/util.cpp
)

set(EEVM_SIMPLE_SRCS
  src/simple/simpleaccount.cpp
  src/simple/simpleglobalstate.cpp
  src/simple/simplestorage.cpp
)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Libraries
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
add_library(eevm STATIC
  ${EEVM_CORE_SRCS}
  ${KECCAK_SOURCES}
)



link_directories(/opt/openenclave/lib/openenclave/enclave/)
# add_library(eevm STATIC oeenclave)
# add_library(eevm STATIC oecryptombed)
# add_library(eevm STATIC mbedx509)
# add_library(eevm STATIC mbedcrypto)
# add_library(eevm STATIC oelibcxx)
# add_library(eevm STATIC oelibc)
# add_library(eevm STATIC oesyscall)
# add_library(eevm STATIC oecore)

# set_property(TARGET eevm
#     PROPERTY POSITION_INDEPENDENT_CODE ON
# )
target_include_directories(eevm PRIVATE
  ${EEVM_INCLUDE_DIRS}
)

target_link_libraries(eevm
  intx::intx
  oeenclave
  oecryptombed
  mbedx509
  mbedcrypto
  oelibcxx
  oelibc
  oesyscall
  oecore
)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Executables
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
add_executable(eevm_tests
  tests/main.cpp
  tests/harness.cpp
  tests/rlp.cpp
  ${EEVM_SIMPLE_SRCS}
)
target_include_directories(eevm_tests PRIVATE
  ${EEVM_INCLUDE_DIRS}
  ${OE_SDK_CXX_INCLUDES}
)
target_compile_options(eevm_tests PRIVATE ${COVERAGE_FLAGS})
target_link_libraries(eevm_tests eevm ${PLATFORM_SPECIFIC_TEST_LIBS})


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Tests
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
enable_testing()

add_test(
  NAME eevm_tests
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit_test_wrapper.sh eevm_tests
)

if(NOT ENV{TEST_DIR})
  set_tests_properties(eevm_tests
    PROPERTIES
      ENVIRONMENT TEST_DIR=${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/test_cases
  )
endif()


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Samples
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
function(add_sample name)
  add_executable(${name}
    samples/${name}/main.cpp
    ${EEVM_SIMPLE_SRCS}
  )
  target_include_directories(${name} PRIVATE
    ${EEVM_INCLUDE_DIRS}
  )
  target_link_libraries(${name} eevm)
endfunction()

add_sample(hello_world)
add_sample(sum)
add_sample(erc20)
add_sample(disassembler)
