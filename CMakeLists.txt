cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()
if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()

project(pyaseba)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(WIN32)
  add_compile_definitions(NOMINMAX _ALLOW_KEYWORD_MACROS)
  # add_definitions(-DDISABLE_WEAK_CALLBACKS)
else()
  add_compile_options(-fPIC)
endif()

if(NOT DEFINED aseba_patched)
  set(patchCommand PATCH_COMMAND git apply
                   "${CMAKE_CURRENT_LIST_DIR}/aseba_patch.diff")
endif()

if(NOT aseba_FOUND)
  set(aseba_patched
      ON
      CACHE BOOL "" FORCE)
  include(FetchContent)
  FetchContent_Declare(
    aseba
    EXCLUDE_FROM_ALL
    GIT_REPOSITORY https://github.com/aseba-community/aseba.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    ${patchCommand} OVERRIDE_FIND_PACKAGE)
  FetchContent_MakeAvailable(aseba)
endif()

if(TARGET zeroconf)
  message("With Zeroconf support")
  add_compile_definitions(ZEROCONF=1)
  set(EXTRA_LIBS asebazeroconf)
else()
  message("Without Zeroconf support")
endif()


set(Python3_FIND_STRATEGY LOCATION)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11 3 QUIET)
if(NOT pybind11_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v3.0.1
    GIT_SHALLOW TRUE
    OVERRIDE_FIND_PACKAGE EXCLUDE_FROM_ALL)
  set(PYBIND11_INSTALL ON)
  set(INCLUDE_PYBIND11 true)
  FetchContent_MakeAvailable(pybind11)
  find_package(pybind11 REQUIRED)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(DISABLE_WARNINGS "-w")
  set(WARNINGS -Wall -Wextra -Wno-pedantic -Wno-conversion -Wno-unused-parameter -Wno-sign-compare)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(DISABLE_WARNINGS "-Wno-everything")
  set(WARNINGS -Wall -Wextra -Wno-gnu-flexible-array-initializer -Wno-c99-extensions -Wno-unused-parameter -Wno-sign-compare) # -Wconversion 
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(WARNINGS "/W3")
  set(DISABLE_WARNINGS "/w")
endif()

add_subdirectory(src/client)
add_subdirectory(src/network)


