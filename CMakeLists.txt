cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0167 NEW)

project(cpu_miner
   VERSION 0.1.0
   DESCRIPTION "Educational Stratum v1 CPU miner / share finder"
   LANGUAGES CXX
)

# ---- Language / build settings ------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(CPU_MINER_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CPU_MINER_ENABLE_TESTS "Enable tests" ON)
option(CPU_MINER_ENABLE_NATIVE_OPTIMIZATION
   "Enable -march=native/-mtune=native for Release-style local builds" OFF)

# ---- Dependencies -------------------------------------------------------------

# Homebrew paths are harmless to prepend on Apple platforms.
if(APPLE)
   list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

# Force classic FindBoost instead of BoostConfig.cmake.
# This avoids Homebrew/component config mismatches such as boost_systemConfig.cmake.
set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost REQUIRED COMPONENTS json)

message(STATUS "Boost include dir: ${Boost_INCLUDE_DIRS}")

# ---- Library: util ------------------------------------------------------------

add_library(cpu_miner_util
   src/util/hex.cpp
   src/util/endian.cpp
   src/util/log.cpp
)

target_include_directories(cpu_miner_util
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
   SYSTEM PRIVATE
      ${Boost_INCLUDE_DIRS}
)

target_compile_features(cpu_miner_util PUBLIC cxx_std_23)

# ---- Library: sha256 ----------------------------------------------------------

add_library(cpu_miner_sha256
   src/sha256/sha256.cpp
   src/sha256/midstate.cpp
)

target_include_directories(cpu_miner_sha256
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
   SYSTEM PRIVATE
      ${Boost_INCLUDE_DIRS}
)

target_compile_features(cpu_miner_sha256 PUBLIC cxx_std_23)

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
   src/mining_job/work_state.cpp
   src/mining_job/scan.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
   set_source_files_properties(
      src/mining_job/target.cpp
      src/mining_job/scan.cpp
      PROPERTIES
      COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion;-Wno-old-style-cast;-Wno-pedantic"
   )
endif()

target_include_directories(cpu_miner_mining_job
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
   SYSTEM PRIVATE
      ${Boost_INCLUDE_DIRS}
)

target_compile_features(cpu_miner_mining_job PUBLIC cxx_std_23)

target_link_libraries(cpu_miner_mining_job
   PUBLIC
      cpu_miner_util
      cpu_miner_sha256
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
   SYSTEM PRIVATE
      ${Boost_INCLUDE_DIRS}
)

target_compile_features(cpu_miner_stratum PUBLIC cxx_std_23)

target_link_libraries(cpu_miner_stratum
   PUBLIC
      cpu_miner_util
      cpu_miner_mining_job
)

# ---- Executable ----------------------------------------------------------------

add_executable(cpu_miner
   src/main.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
   set_source_files_properties(
      src/main.cpp
      PROPERTIES
      COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion;-Wno-old-style-cast;-Wno-pedantic"
   )
endif()

target_include_directories(cpu_miner
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
   SYSTEM PRIVATE
      ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cpu_miner
   PRIVATE
      cpu_miner_stratum
      cpu_miner_mining_job
      cpu_miner_sha256
      cpu_miner_util
      Boost::json
)

add_executable(rejected_share_repro
   src/repro/rejected_share_repro.cpp
)

target_include_directories(rejected_share_repro
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
   SYSTEM PRIVATE
      ${Boost_INCLUDE_DIRS}
)

target_link_libraries(rejected_share_repro
   PRIVATE
      cpu_miner_mining_job
      cpu_miner_sha256
      cpu_miner_util
)

# ---- Warnings and Optimizations ----------------------------------------------

function(cpu_miner_set_optimization target_name)
   if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
      if(CMAKE_BUILD_TYPE STREQUAL "Release")
         target_compile_options(${target_name} PRIVATE -O3)
      endif()

      if(CPU_MINER_ENABLE_NATIVE_OPTIMIZATION)
         target_compile_options(${target_name} PRIVATE -march=native -mtune=native)
      endif()
   endif()
endfunction()

function(cpu_miner_set_warnings target_name)
   if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
      target_compile_options(${target_name} PRIVATE
         -Wall
         -Wextra
         -Wpedantic
         -Wconversion
         -Wsign-conversion
         -Wshadow
         -Wnull-dereference
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

cpu_miner_set_optimization(cpu_miner_util)
cpu_miner_set_optimization(cpu_miner_sha256)
cpu_miner_set_optimization(cpu_miner_mining_job)
cpu_miner_set_optimization(cpu_miner_stratum)
cpu_miner_set_optimization(cpu_miner)

cpu_miner_set_warnings(rejected_share_repro)
cpu_miner_set_optimization(rejected_share_repro)

# ---- Tests --------------------------------------------------------------------

if(CPU_MINER_ENABLE_TESTS)
   include(CTest)
   enable_testing()

   include(FetchContent)

   FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.5.4
   )

   FetchContent_MakeAvailable(Catch2)

   add_executable(cpu_miner_tests
      tests/main.cpp
      tests/test_hex.cpp
      tests/test_sha256.cpp
      tests/test_merkle.cpp
      tests/test_header.cpp
      tests/test_work_state.cpp
      tests/test_regression_share.cpp
   )

   target_include_directories(cpu_miner_tests
      PRIVATE
         ${CMAKE_CURRENT_SOURCE_DIR}/src
   )

   target_link_libraries(cpu_miner_tests
      PRIVATE
         cpu_miner_util
         cpu_miner_sha256
         cpu_miner_mining_job
         Catch2::Catch2WithMain
   )

   target_compile_features(cpu_miner_tests PUBLIC cxx_std_23)

   cpu_miner_set_warnings(cpu_miner_tests)
   cpu_miner_set_optimization(cpu_miner_tests)

   include(Catch)
   catch_discover_tests(cpu_miner_tests)
endif()

