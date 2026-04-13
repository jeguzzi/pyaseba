pybind11_add_module(_network_impl python.cpp network.cpp)
target_link_libraries(
  _network_impl PRIVATE dashel asebacommon asebavmbuffer asebavm asebacompiler
                       asebadashelplugins ${EXTRA_LIBS})
target_include_directories(_network_impl PRIVATE ${aseba_SOURCE_DIR}
                                                ${dashel_SOURCE_DIR}
                                                ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(_network_impl
                           PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES=1)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
  set_property(TARGET _network_impl PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(DISABLE_WARNINGS "-w")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(DISABLE_WARNINGS "-Wno-everything")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(DISABLE_WARNINGS "/w")
endif()

target_compile_options(asebavmbuffer PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebavm PRIVATE ${DISABLE_WARNINGS})
target_compile_options(_client_impl PRIVATE ${WARNINGS})

install(
  TARGETS _network_impl
  DESTINATION pyaseba/network
  COMPONENT pyaseba)
