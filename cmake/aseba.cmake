include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

include(${aseba_SOURCE_DIR}/CMakeModules/cpp_features.cmake)
include(${aseba_SOURCE_DIR}/CMakeModules/aseba_conf.cmake)
include(${aseba_SOURCE_DIR}/CMakeModules/codesign.cmake)

add_subdirectory(${aseba_SOURCE_DIR}/aseba/common)
add_subdirectory(${aseba_SOURCE_DIR}/aseba/compiler)
add_subdirectory(${aseba_SOURCE_DIR}/aseba/transport)
add_subdirectory(${aseba_SOURCE_DIR}/aseba/vm)

target_include_directories(asebacommon PUBLIC ${aseba_SOURCE_DIR}/aseba)
target_include_directories(asebacompiler PUBLIC ${aseba_SOURCE_DIR}/aseba)
target_include_directories(asebavm PUBLIC ${aseba_SOURCE_DIR}/aseba)
target_include_directories(asebavmbuffer PUBLIC ${aseba_SOURCE_DIR}/aseba)

include(${CMAKE_CURRENT_LIST_DIR}/asebazeroconf.cmake)
