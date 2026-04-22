include(FetchContent)
include(FeatureSummary)

if(NOT DEFINED dashel_patched)
  set(patchCommand PATCH_COMMAND git apply
                   "${CMAKE_CURRENT_LIST_DIR}/dashel_patch.diff")
else()
  unset(patchCommand)
endif()

set(dashel_patched
    ON
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  dashel
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/aseba-community/dashel.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  ${patchCommand})
FetchContent_MakeAvailable(dashel)

if(NOT DEFINED aseba_patched)
  set(patchCommand PATCH_COMMAND git apply
                   "${CMAKE_CURRENT_LIST_DIR}/aseba_patch.diff")
else()
  unset(patchCommand)
endif()

set(aseba_patched
    ON
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  aseba
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/aseba-community/aseba.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR non-existant
  GIT_SUBMODULES "" ${patchCommand})
FetchContent_MakeAvailable(aseba)

set(ASEBA_VERSION_MAJOR 3)
set(ASEBA_VERSION_MINOR 0)
set(ASEBA_VERSION_PATCH 0)
set(LIB_VERSION_MAJOR 3)
set(LIB_VERSION_MINOR 0)
set(LIB_VERSION_PATCH 0)
set(LIB_VERSION_STRING
    ${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH})
