cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0167 NEW)

project(cpu_miner
   VERSION 0.1.0
   DESCRIPTION "Educational Stratum v1 CPU miner / share finder"
   LANGUAGES CXX
)

# ---- Language / build settings ------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(CPU_MINER_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CPU_MINER_ENABLE_TESTS "Enable tests" ON)

# ---- Dependencies -------------------------------------------------------------

find_package(Boost 1.81 REQUIRED COMPONENTS system json)

# ---- Library: util ------------------------------------------------------------

add_library(cpu_miner_util
   src/util/hex.cpp
   src/util/endian.cpp
   src/util/log.cpp
)

target_include_directories(cpu_miner_util
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(cpu_miner_util PUBLIC cxx_std_20)

# ---- Library: sha256 ----------------------------------------------------------

add_library(cpu_miner_sha256
   src/sha256/sha256.cpp
   src/sha256/midstate.cpp
)

target_include_directories(cpu_miner_sha256
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(cpu_miner_sha256 PUBLIC cxx_std_20)

target_link_libraries(cpu_miner_sha256
   PUBLIC
      cpu_miner_util
)

# ---- Library: mining_job ------------------------------------------------------

add_library(cpu_miner_mining_job
   src/mining_job/job.cpp
   src/mining_job/coinbase.cpp
   src/mining_job/merkle.cpp
   src/mining_job/target.cpp
   src/mining_job/header.cpp
)

target_include_directories(cpu_miner_mining_job
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(cpu_miner_mining_job PUBLIC cxx_std_20)

target_link_libraries(cpu_miner_mining_job
   PUBLIC
      cpu_miner_util
      cpu_miner_sha256
      Boost::json
)

# ---- Library: stratum_client --------------------------------------------------

add_library(cpu_miner_stratum
   src/stratum_client/stratum_client.cpp
   src/stratum_client/messages.cpp
   src/stratum_client/session.cpp
)

target_include_directories(cpu_miner_stratum
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(cpu_miner_stratum PUBLIC cxx_std_20)

target_link_libraries(cpu_miner_stratum
   PUBLIC
      cpu_miner_util
      cpu_miner_mining_job
      Boost::system
      Boost::json
)

# ---- Executable ----------------------------------------------------------------

add_executable(cpu_miner
   src/main.cpp
)

target_link_libraries(cpu_miner
   PRIVATE
      cpu_miner_stratum
      cpu_miner_mining_job
      cpu_miner_sha256
      cpu_miner_util
)

# ---- Warnings -----------------------------------------------------------------

function(cpu_miner_set_warnings target_name)
   if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
      target_compile_options(${target_name} PRIVATE
         -Wall
         -Wextra
         -Wpedantic
         -Wconversion
         -Wsign-conversion
         -Wshadow
         -Wold-style-cast
         -Wnon-virtual-dtor
         -Woverloaded-virtual
         -Wnull-dereference
         -Wdouble-promotion
         -Wimplicit-fallthrough
      )

      if(CPU_MINER_WARNINGS_AS_ERRORS)
         target_compile_options(${target_name} PRIVATE -Werror)
      endif()
   elseif(MSVC)
      target_compile_options(${target_name} PRIVATE /W4)
      if(CPU_MINER_WARNINGS_AS_ERRORS)
         target_compile_options(${target_name} PRIVATE /WX)
      endif()
   endif()
endfunction()

cpu_miner_set_warnings(cpu_miner_util)
cpu_miner_set_warnings(cpu_miner_sha256)
cpu_miner_set_warnings(cpu_miner_mining_job)
cpu_miner_set_warnings(cpu_miner_stratum)
cpu_miner_set_warnings(cpu_miner)

# ---- Tests --------------------------------------------------------------------

if(CPU_MINER_ENABLE_TESTS)
   include(CTest)
   enable_testing()

   # Add Catch2 or another test framework later.
   # Example:
   # add_executable(cpu_miner_tests
   #    tests/test_main.cpp
   #    tests/test_hex.cpp
   #    tests/test_sha256.cpp
   #    tests/test_merkle.cpp
   # )
   #
   # target_link_libraries(cpu_miner_tests
   #    PRIVATE
   #       cpu_miner_util
   #       cpu_miner_sha256
   #       cpu_miner_mining_job
   #       cpu_miner_stratum
   # )
   #
   # cpu_miner_set_warnings(cpu_miner_tests)
   # add_test(NAME cpu_miner_tests COMMAND cpu_miner_tests)
endif()

