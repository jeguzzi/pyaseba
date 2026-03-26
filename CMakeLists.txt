cmake_minimum_required(VERSION 3.15)
project(pyaseba)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(NOT aseba_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    aseba
    EXCLUDE_FROM_ALL
    GIT_REPOSITORY https://github.com/aseba-community/aseba.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    PATCH_COMMAND
      truncate -s0 CMakeModules/enki.cmake && sed -I '' -e
      "s/ASEBA_PROTOCOL_VERSION 5/ASEBA_PROTOCOL_VERSION 9/"
      aseba/common/consts.h OVERRIDE_FIND_PACKAGE)
  FetchContent_MakeAvailable(aseba)
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
    OVERRIDE_FIND_PACKAGE)
  set(PYBIND11_INSTALL ON)
  set(INCLUDE_PYBIND11 true)
  FetchContent_MakeAvailable(pybind11)
  find_package(pybind11 REQUIRED)
endif()

pybind11_add_module(pyaseba src/main.cpp)
target_link_libraries(pyaseba PRIVATE asebacommon asebacompiler
                                      asebadashelplugins)
target_include_directories(pyaseba PRIVATE ${aseba_SOURCE_DIR}
                                           ${dashel_SOURCE_DIR})
target_compile_definitions(pyaseba PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES=1)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
  set_property(TARGET pyaseba PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(DISABLE_WARNINGS "-w")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(DISABLE_WARNINGS "-Wno-everything")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(DISABLE_WARNINGS "/w")
endif()

target_compile_options(dashel PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebacommon PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebacompiler PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebadashelplugins PRIVATE ${DISABLE_WARNINGS})

install(
  TARGETS pyaseba
  DESTINATION pyaseba
  COMPONENT pyaseba)
