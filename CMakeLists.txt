cmake_minimum_required(VERSION 3.15)
project(pyaseba)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(WIN32)
  add_compile_definitions(NOMINMAX _ALLOW_KEYWORD_MACROS)
  add_definitions(-DDISABLE_WEAK_CALLBACKS)
else()
  add_compile_options(-fPIC)
endif()

if(NOT DEFINED aseba_patched)
  set(patchCommand PATCH_COMMAND git apply "${CMAKE_CURRENT_LIST_DIR}/aseba_patch.diff")
endif()

if(NOT aseba_FOUND)
  set(aseba_patched ON CACHE BOOL "" FORCE)
  include(FetchContent)
  FetchContent_Declare(
    aseba
    EXCLUDE_FROM_ALL
    GIT_REPOSITORY https://github.com/aseba-community/aseba.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    ${patchCommand}
    OVERRIDE_FIND_PACKAGE)
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
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL)
  set(PYBIND11_INSTALL ON)
  set(INCLUDE_PYBIND11 true)
  FetchContent_MakeAvailable(pybind11)
  find_package(pybind11 REQUIRED)
endif()

add_subdirectory(src/client)
add_subdirectory(src/server)
