if(ZEROCONF)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${aseba_SOURCE_DIR}/CMakeModules)
  include(${aseba_SOURCE_DIR}/CMakeModules/zeroconf.cmake)
  if(HAS_ZEROCONF_SUPPORT)
    add_library(
      asebazeroconf
      ${aseba_SOURCE_DIR}/aseba/common/zeroconf/zeroconf.cpp
      ${aseba_SOURCE_DIR}/aseba/common/zeroconf/txtrecord.cpp
      ${aseba_SOURCE_DIR}/aseba/common/zeroconf/target.cpp
      ${aseba_SOURCE_DIR}/aseba/common/zeroconf/zeroconf-thread.cpp
      ${aseba_SOURCE_DIR}/aseba/common/zeroconf/zeroconf-dashelhub.cpp)
    set_target_properties(asebazeroconf PROPERTIES VERSION
                                                   ${LIB_VERSION_STRING})
    target_link_libraries(asebazeroconf PUBLIC asebacommon zeroconf)
    target_include_directories(asebazeroconf PRIVATE ${dashel_SOURCE_DIR}
                                                     ${aseba_SOURCE_DIR}/aseba)
  endif()
endif()
