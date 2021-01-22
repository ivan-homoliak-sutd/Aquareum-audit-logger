cmake_minimum_required(VERSION 3.12)

# !!! cmake from folder build-host builds eEVM for host and from folder 'build' builds eEVM for enlcave

# causes EVM to print debug trace messages
set(DEBUG OFF)
set(ASSERTS OFF)

###################################################
## configure HUNTER first
# set(HUNTER_CONFIGURATION_TYPES Release CACHE STRING "Hunter configuration types")
# set(HUNTER_JOBS_NUMBER 4 CACHE STRING "Hunter jobs number")
# set(HUNTER_CACHE_SERVERS "https://github.com/ethereum/hunter-cache" CACHE STRING "Hunter cache servers")
# set(HUNTER_PASSWORDS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Hunter/passwords.cmake CACHE STRING "Hunter password path")

# include("cmake/HunterGate.cmake")
# HunterGate(
#     URL "https://github.com/ruslo/hunter/archive/v0.23.211.tar.gz"
#     SHA1 "c022f0c2480e835a6bafdd734035ec2071b51897"
#     LOCAL
# )

###################################################
## Project eEVM
project(eevm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE on)
include(CheckCXXCompilerFlag) # TODO:

# Apply Spectre mitigations if available.
set(SPECTRE_MITIGATION_FLAGS -mllvm -x86-speculative-load-hardening)
CHECK_CXX_COMPILER_FLAG("${SPECTRE_MITIGATION_FLAGS}" SPECTRE_MITIGATION_CXX_FLAGS_SUPPORTED)
if (SPECTRE_MITIGATION_CXX_FLAGS_SUPPORTED)
  message(STATUS "Spectre 1 mitigations supported")
  # We set this variable to indicate the flags are supported. It is empty otherwise.
  set(OE_SPECTRE_MITIGATION_FLAGS ${SPECTRE_MITIGATION_FLAGS})
  # TODO: We really should specify this only on the `oecore` target;
  # however, the third-party mbed TLS build needs it set to, so we
  # have to keep this here for now.
  add_compile_options(${OE_SPECTRE_MITIGATION_FLAGS})
else ()
  message(WARNING "Spectre 1 mitigations NOT supported")
endif ()

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
  # "/opt/boost-incl"
  # "/usr/lib/gcc/x86_64-linux-gnu/7/include"
)

# Obtanined by: $ pkg-config oeenclave-g++ --libs
SET(CXX_ENCLAVE_LINK_FLAGS
  "-nostdlib -nodefaultlibs -nostartfiles -fno-strict-aliasing -Wl,--no-undefined -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--export-dynamic -Wl,-pie -Wl,--build-id -Wl,-z,noexecstack -Wl,-z,now"
)

# Obtanined by: $ pkg-config oehost-g++ --libs
SET(CXX_HOST_LINK_FLAGS
  "-rdynamic -Wl,-z,noexecstack"
)

# Obtanined by: $ pkg-config oeenclave-g++ --cflags
SET(CXX_ENCLAVE_COMPILE_FLAGS
  "-DENCLAVE_BUILD -nostdinc -m64 -fPIE -ftls-model=local-exec -fvisibility=hidden -fno-stack-protector"
)

# Obtanined by: $ pkg-config oehost-g++ --cflags
SET(CXX_HOST_COMPILE_FLAGS
  "-fstack-protector-strong -D_FORTIFY_SOURCE=2"
)

if(${CMAKE_BINARY_DIR} MATCHES ".+/build-host$")
  # building eEVM for host
  message(STATUS ">>> BUILDING EEVM for HOST <<<")
  SET(CMAKE_CXX_FLAGS "${CXX_HOST_COMPILE_FLAGS}")
  SET(CMAKE_CC_FLAGS "${CXX_HOST_COMPILE_FLAGS}")
  SET(CMAKE_EXE_LINKER_FLAGS  "${CXX_HOST_LINK_FLAGS}")
  SET(SUM_CXX_INCLUDES "")
else()
  # building eEVM for enclave
  message(STATUS ">>> BUILDING EEVM for ENCLAVE <<<")
  SET(CMAKE_CXX_FLAGS "${CXX_ENCLAVE_COMPILE_FLAGS}")
  SET(CMAKE_CC_FLAGS "${CXX_ENCLAVE_COMPILE_FLAGS}")
  SET(CMAKE_EXE_LINKER_FLAGS  "${CXX_ENCLAVE_LINK_FLAGS}")
  include_directories("${OE_SDK_CXX_INCLUDES}")
  SET(SUM_CXX_INCLUDES "${OE_SDK_CXX_INCLUDES}")
endif()
message(STATUS " ")

if(DEBUG)
  add_compile_definitions(TRACING_ENABLED)
  message(STATUS ">>> TRACING LOGS ENABLED <<<")
  message(STATUS " ")
endif()

if(ASSERTS)
  message(STATUS ">>> ASSERTS ARE ENABLED <<<")
  message(STATUS " ")
else()
  add_compile_definitions(NDEBUG)
  message(STATUS ">>> ASSERTS ARE DISABLED <<<")
  message(STATUS " ")
endif()

if(MSVC)
  # Set Windows compiler options.
  add_compile_options(/W3 /std:c++latest)
else()
  # Set Linux compiler options
  add_compile_options(-Wall -Werror)
  # add_compile_options(-std=c++17)

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
# Dependencies of 3rd party libs
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

# IH: removed dependency on linking "threat-related boost object
# set(CMAKE_THREAD_LIBS_INIT "-lpthread")
# set(Boost_USE_STATIC_LIBS ON)
# set(Boost_USE_MULTITHREADED ON)

# hunter_add_package(Boost COMPONENTS thread)
# find_package(Boost CONFIG REQUIRED thread)

# hunter_add_package(ethash)
# find_package(ethash CONFIG REQUIRED)

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
  src/normal/normalGlobalState.cpp
)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Libraries -
#           we archive eevm lib as object archive that does not contain static libraries
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
add_library(eevm OBJECT
  ${EEVM_CORE_SRCS}
  ${EEVM_SIMPLE_SRCS}
  ${KECCAK_SOURCES}
)

link_directories(/opt/openenclave/lib/openenclave/enclave/)
# set_property(TARGET eevm
#     PROPERTY POSITION_INDEPENDENT_CODE ON
# )

target_include_directories(eevm PRIVATE
  ${EEVM_INCLUDE_DIRS}
)

target_link_libraries(eevm
  intx::intx
  aleth::mp3
  # oeenclave
  # oecryptombed
  # mbedx509
  # mbedcrypto
  # oeenclave
  # oelibcxx
  # oelibc
  # oesyscall
  # oecore
)


return()

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
  ${SUM_CXX_INCLUDES}
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
