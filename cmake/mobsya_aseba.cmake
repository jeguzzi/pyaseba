include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

if(NOT DEFINED mobsya_aseba_patched)
  set(patchCommand PATCH_COMMAND git apply
                   "${CMAKE_CURRENT_LIST_DIR}/mobsya_aseba_patch.diff")
else()
  unset(patchCommand)
endif()

set(mobsya_aseba_patched
    ON
    CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
  mobsya_aseba
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/Mobsya/aseba.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR non-existant
  GIT_SUBMODULES ""
  ${patchCommand})

FetchContent_MakeAvailable(mobsya_aseba)

include(${mobsya_aseba_SOURCE_DIR}/CMakeModules/cpp_features.cmake)
include(${mobsya_aseba_SOURCE_DIR}/CMakeModules/aseba_conf.cmake)

set(ANDROID TRUE)
add_subdirectory(${mobsya_aseba_SOURCE_DIR}/aseba/common aseba/common)
unset(ANDROID)
add_subdirectory(${mobsya_aseba_SOURCE_DIR}/aseba/compiler aseba/compiler)
add_subdirectory(${mobsya_aseba_SOURCE_DIR}/aseba/transport aseba/transport)
add_subdirectory(${mobsya_aseba_SOURCE_DIR}/aseba/vm aseba/vm)

target_include_directories(asebacommon PUBLIC ${mobsya_aseba_SOURCE_DIR}/aseba)
target_include_directories(asebacompiler
                           PUBLIC ${mobsya_aseba_SOURCE_DIR}/aseba)
target_include_directories(asebavm PUBLIC ${mobsya_aseba_SOURCE_DIR}/aseba)
target_include_directories(asebavmbuffer
                           PUBLIC ${mobsya_aseba_SOURCE_DIR}/aseba)

include(${CMAKE_CURRENT_LIST_DIR}/asebazeroconf.cmake)
